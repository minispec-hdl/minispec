# Minispec Jupyter kernel
# Author: Daniel Sanchez
# Based on Jupyter EchoKernel example, https://jupyter-client.readthedocs.io/en/latest/wrapperkernels.html

from ipykernel.kernelbase import Kernel
from IPython.display import SVG
from subprocess import Popen, PIPE
from tempfile import mkdtemp
import os, re, select, signal, sys

## Helper functions

def readFile(file):
    f = open(file, "r")
    data = f.read()
    f.close()
    return data

def writeFile(file, data):
    f = open(file, "w")
    f.write(data)
    f.close()


class MinispecKernel(Kernel):
    implementation = 'Minispec'
    implementation_version = '0.1'
    language = 'minispec'
    language_version = '0.5'
    language_info = {
        'name': 'minispec',
        'mimetype': 'text/x-minispec',
        'file_extension': '.ms',
    }
    banner = "Minispec kernel"

    # Stores all files that form part of history
    history_files = []
    tmpDir = ""

    # Display functions allow post-processing of a command's stdout/stderr 
    # before sending to Jupyter. runCmd guarantees that:
    # - name is always "stdout" or "stderr"
    # - text is always a fixed number of lines, so that display functions 
    #   can do line-level regex/replace/capture
    def _defaultDisplay(self, name, text):
        content = {'name': name, 'text': text}
        self.send_response(self.iopub_socket, 'stream', content)

    def runCmd(self, cmd, display=_defaultDisplay):
        # Run subprocesses with line buffering. Python programs (e.g., synth)
        # buffer aggressively if the output is not a tty; stdbuf avoids it.
        p = Popen('stdbuf -oL -eL bash -c "' + cmd + '"', shell=True,
                stdout=PIPE, stderr=PIPE, preexec_fn=os.setsid)
              
        activePipes = { # name, linebuf
            p.stdout : ("stdout", []),
            p.stderr : ("stderr", [])
        }
        anyOutput = False
        while activePipes:
            readyPipes, _, _ = select.select(activePipes.keys(), [], [], 0)
            for pipe in readyPipes:
                (name, linebuf) = activePipes[pipe]
                data = os.read(pipe.fileno(), 1024)
                if data == "":
                    # NOTE: This discards linebuf, so it doesn't print an
                    # unterminated last line, which is standard Jupyter
                    # behavior.
                    del activePipes[pipe]
                    self.log.warn(name + " went inactive")
                else:
                    anyOutput = True
                    # Perform line buffering
                    (body, sep, trail) = data.rpartition("\n")
                    if sep == "\n":  # found newline
                        text = "".join(linebuf) + body + "\n"
                        try:
                            display(self, name, text)
                        except:
                            os.killpg(os.getpgid(p.pid), signal.SIGTERM)
                            return -1
                        self.log.warn(name + " | " + text)
                        linebuf[:] = [trail]  # mutates linebuf
                    else:  # no newline
                        linebuf.append(data)
        
        p.communicate()  # close pipes & wait for return
        if p.returncode != 0 and not anyOutput:
            stream_content = {'name': 'stderr', 'text': "command %s silently failed with exit code %d\n" % (cmd, p.returncode)}
            self.send_response(self.iopub_socket, 'stream', stream_content)
        
        return p.returncode

    def do_execute(self, code, silent, store_history=True, user_expressions=None,
                   allow_stdin=False):
        # Initialize
        if self.tmpDir == "":
            self.tmpDir = mkdtemp(suffix="msj")
            self.log.warn("Using tmpDir: " + self.tmpDir)
        tmpDir = self.tmpDir
        errMsg = {'status': 'error'}
        userDir = os.getcwd()

        # Extract magics
        lines = code.split("\n")
        codeLines = []
        magics = []
        for line in lines:
            ls = line.strip()
            if ls.startswith("%%"):
                magics.append(ls[2:].strip())
                codeLines.append("") # keep line in code to get consistent error reporting
            else:
                codeLines.append(line)
        code = "\n".join(codeLines)

        codeFile = "In%d.ms" % (self.execution_count,)
        writeFile(os.path.join(tmpDir, codeFile), code)

        def mscDisplay(self, name, text):
            if name == "stdout":
                text = re.sub("produced simulation executable (.*?)\n", "", text)
                text = re.sub("no errors found on (.*?)\n", "no errors found\n", text)
            text = re.sub(r"In(\d+)\.ms", r"In [\1]", text)
            if len(text):
                self._defaultDisplay(name, text)

        simLines = [0] # mutable...
        maxLines = 1000 # TODO: Make configurable
        def simDisplay(self, name, text):
            # No idea why these happen, but simulation executables call grep...
            if name == "stderr":
                text = re.sub("grep: write error: Broken pipe\n", "", text)
            if len(text):
                simLines[0] += text.count("\n")
                if simLines[0] <= maxLines:
                    self._defaultDisplay(name, text)
                else:
                    self._defaultDisplay("stderr", "exceeded maximum simulation output (%d lines), aborting" % (maxLines,))
                    raise Exception("too much simulation output")

        # Produce history
        combineArgs = " ".join(self.history_files + [codeFile])
        if self.runCmd("(cd %s && minispec-combine %s > History___.ms)" % (tmpDir, combineArgs), display=mscDisplay): return errMsg
       
        # Produce top-level file. This leverages the fact that the compiler
        # flattens imports to avoid modifying the actual input files, which
        # gives better error messages.
        topFile = "Top___.ms"
        hf = os.path.join(tmpDir, topFile)
        writeFile(hf, "import History___;\nimport In%d;\n" % (self.execution_count,))

        # All magics have to compile the code, so do a basic compile only if there are no magics
        if len(magics) == 0:
            if self.runCmd("(cd %s && msc '%s' -p '%s')" % (tmpDir, topFile, userDir), display=mscDisplay): return errMsg;

        for magic in magics:
            (cmd, _, args) = magic.partition(" ")
            args = args.strip()
            if cmd == "sim":
                modName = args
                if self.runCmd("(cd %s && msc '%s' '%s' -p '%s')" % (tmpDir, topFile, modName, userDir), display=mscDisplay): return errMsg
                if self.runCmd("(cd %s && ./%s)" % (tmpDir, modName), display=simDisplay): return errMsg
            elif cmd == "eval":
                # TODO: Pre-check expr is a valid expression (use MinispecParser/msutil)
                # TODO: Combine multiple eval magics into a single compile
                # TODO: Code.ms should still be precompiled before Eval.ms to minimize eval error handling...
                expr = args
                evalFile = os.path.join(tmpDir, "Eval___.ms")
                evalCode = '''
import Top___;
// Auto-generated eval module
module Eval___;
  rule eval;
    let expr =
%s
    ;
    $display("%s = ", fshow(expr));
    $finish;
  endrule
endmodule''' % (expr, expr)
                writeFile(evalFile, evalCode)
                if self.runCmd("(cd %s && msc Eval___.ms Eval___ -p '%s')" % (tmpDir, userDir), display=mscDisplay): return errMsg
                if self.runCmd("(cd %s && ./Eval___)" % tmpDir, display=simDisplay): return errMsg
            elif cmd == "synth":
                def synthDisplay(self, name, text):
                    text = re.sub("from file (.*?)\n", "\n", text)
                    cur = 0
                    for m in re.finditer("Produced circuit diagram in (.*?)\n", text):
                        self._defaultDisplay(name, text[cur:m.start()] + "Circuit diagram:\n")
                        svgFilename = m.group(1).strip()
                        im = SVG(filename=os.path.join(tmpDir, svgFilename))
                        stream_content = {'data': {"image/svg+xml" : im._repr_svg_()}}
                        self.send_response(self.iopub_socket, 'display_data', stream_content)
                        cur = m.end() + 1
                    if cur < len(text):
                        self._defaultDisplay(name, text[cur:])
                cmd = "(cd %s && synth %s %s)" % (tmpDir, topFile, args)
                if self.runCmd(cmd, display=synthDisplay): return errMsg
            elif cmd == "help":
                helpMsg = '''Available magics:
  %%eval <expression>           Evaluate expression
  %%sim <moduleName>            Simulate module
  %%synth <function/module>     Synthesize function or module (for more help, run %%synth -h)
  %%help                        Print help message'''
                self._defaultDisplay("stdout", helpMsg)
            else:
                stream_content = {'name': 'stderr', 'text': "Invalid magic: " + magic}
                self.send_response(self.iopub_socket, 'stream', stream_content)
                return errMsg

        # Success!
        if store_history:
            self.history_files.append(codeFile)
        return {'status': 'ok',
                # The base class increments the execution count
                'execution_count': self.execution_count,
                'payload': [],
                'user_expressions': {},
               }

if __name__ == '__main__':
    from ipykernel.kernelapp import IPKernelApp
    IPKernelApp.launch_instance(kernel_class=MinispecKernel)
