#!/usr/bin/env python3
# High-level minispec interface, mimics that of minispec Jupyter kernel
# Author: Daniel Sanchez

from subprocess import Popen, PIPE
from tempfile import mkdtemp
import atexit, os, re, select, shutil, signal, subprocess, sys

def writeFile(file, data):
    f = open(file, "w")
    f.write(data)
    f.close()

# Runs a command with fail-stop semantics, like make. If the program returns
# successfully, run() returns stdout and stderr; on a failure, run() stops the
# program.
def run(cmd, failMsg=None):
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    (stdout, stderr) = p.communicate()
    if p.returncode == 0:
        return stdout
    else:
        sys.stderr.write(stderr)
        sys.stderr.flush()
        sys.exit(p.returncode)

def decolorize(s):
    # https://stackoverflow.com/a/14693789
    return re.sub(r"\x1B[@-_][0-?]*[ -/]*[@-~]", "", s)

def printUsage():
    print(
'''usage: ms <command> [<args>]

Available commands:
  eval [<file>] <expression>                    Evaluate expression
  sim <file> <module>                           Simulate module
  synth <file> <function/module> <synthArgs>    Synthesize function or module
  help                                          Print help message

<file> should be a Minispec file with the target function or module. The file
argument is optional in ms eval, as it is not needed if <expression> does not
call any user-defined functions.

Arguments must be quoted as needed to avoid being interpreted by the shell.
For example,
  ms eval log2(16) will cause an error due to shell expansion on parentheses.
  ms eval "log2(16)" works correctly.
Quotes are required for most expressions (e.g., ms eval "2'b101 + 3") and for
parametric functions and modules (e.g., ms synth add.ms "add#(32)".

The synth command can take additional arguments; run ms synth -h for more info.'''
    )

args = sys.argv
if len(args) < 2:
    print ("error: no arguments given")
    printUsage()
    sys.exit(1)

# TODO(dsm): There's some commonality with the jupyter kernel that could be factored out.
cmd = args[1]
cmdArgs = [] if len(args) < 3 else ["'" + a + "'" for a in args[2:]]
if cmd == "eval":
    tmpDir = mkdtemp(suffix="ms")
    atexit.register(shutil.rmtree, tmpDir) # auto-remove tmpDir on exit
    if len(cmdArgs) == 0:
        print("error: need an expression to evaluate")
        sys.exit(1)
    elif len(cmdArgs) == 1:
        pathArg = ""
        importStmt = ""
        expr = args[2]
    else:
        file = args[2]
        if not file.endswith(".ms"):
            print ("Invalid file argument: %s must be a Minispec file (ending in .ms)" % file)
        pathArg = "-p '%s'" % os.path.dirname(os.path.abspath(file))
        stem = os.path.basename(file)[:-3]
        importStmt = "import %s;" % stem
        expr = " ".join(args[3:]) # provide some tolerance for lack of quotes...
    # TODO: Pre-parse expression (using external utils) to avoid confusing parse errors on eval file
    evalFile = os.path.join(tmpDir, "Eval___.ms")
    evalCode = '''%s
// Auto-generated eval module
module Eval___;
  rule eval;
    let expr =
%s
    ;
    $display("%s = ", fshow(expr));
    $finish;
  endrule
endmodule''' % (importStmt, expr, expr)
    writeFile(evalFile, evalCode)
    run("(cd %s && msc Eval___.ms Eval___ %s)" % (tmpDir, pathArg))
    sys.exit(os.system("(cd %s && ./Eval___)" % tmpDir))
elif cmd == "sim":
    if len(cmdArgs) < 2:
        print("error: need file and module arguments")
        sys.exit(1)
    file = args[2]
    print ("Compiling module")
    mscOut = decolorize(run("msc -o sim '%s' %s" % (file, " ".join(cmdArgs[1:]))))
    m = re.search("produced simulation executable (.*?)\n", mscOut)
    if m is None:
        print("error: msc didn't produce a simulation executable!?")
        sys.exit(1)
    execName = m.group(1)
    print ("Simulating module")
    sys.exit(os.system("./%s" % execName))
elif cmd == "synth":
    sys.exit(os.system("synth " + " ".join(cmdArgs)))
elif cmd == "help":
    print("Minispec command-line interface\n")
    printUsage()
else:
    print("error: invalid command " + cmd + " (run 'ms help' for more information)")
    sys.exit(1)
