#!/usr/bin/python3

# $lic$
# Copyright (C) 2018-2020 by Daniel Sanchez
#
# This file is part of the Minispec compiler and toolset.
#
# Minispec is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, version 2.
#
# Minispec is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <http://www.gnu.org/licenses/>.

# Harness for automated regression testing; based on the Swarm architecture
# simulator test script --- https://github.com/SwarmArch/sim

import sys
import os
import getpass
import glob
import subprocess as sp
import multiprocessing
import re
import tempfile
import shutil
import argparse
import signal
import time
from ctypes import cdll

parser = argparse.ArgumentParser()
parser.add_argument("-d", "--dir", type=str,
        default=os.path.dirname(os.path.realpath(__file__)),
        help="tests directory")
parser.add_argument("-e", "--expdir", type=str,
        default="",
        help="expected results directory (leave empty to omit verification)")
parser.add_argument("-o", "--outdir", type=str,
        default="/tmp/{}/test_runs".format(getpass.getuser()),
        help="directory for test runs and outputs")
parser.add_argument("-v", "--verbose",
        default=False, action="store_true",
        help="print out more information")
parser.add_argument("--workers", type=int,
        default=0,
        help="Workers for pmap calls")
parser.add_argument("--resume", default=False, action="store_true", help="resume a long run")
args = parser.parse_args()

# Set by runTargets, see below
preRunHook = None

# Call from an arbitrary process to ensure it terminates if the parent does
def autoterm():
    PR_SET_PDEATHSIG = 1
    SIGTERM = 15
    result = cdll['libc.so.6'].prctl(PR_SET_PDEATHSIG, SIGTERM)
    if result != 0:
        print("ERROR: prctl() failed")
        sys.exit(0)

def diff(f1, f2):
    return os.popen("diff -u %s %s 2>&1" % (f1, f2)).read().strip()

def runAndVerify(argList):
    (progname, cmdlist) = argList
    suffix = "_" + "".join(progname.split())
    tempDir = tempfile.mkdtemp(dir=args.outdir, suffix=suffix)
    if preRunHook is not None:
        cmdlist = preRunHook(progname, cmdlist, tempDir)
        
    outPath = os.path.join(args.outdir, progname + '.out')
    errPath = os.path.join(args.outdir, progname + '.err')
    with open(outPath, 'w') as out, open(errPath, 'w') as err:
        p = sp.Popen(cmdlist, stdout=out, stderr=err, cwd=tempDir, preexec_fn=autoterm)
        p.wait()

    retVal = (progname, "OK", "")
    if len(args.expdir):
        outCmp = os.path.join(args.expdir, progname + '.out')
        errCmp = os.path.join(args.expdir, progname + '.err')
        outDiff = diff(outCmp, outPath)
        errDiff = diff(errCmp, errPath)
        fullDiff = (outDiff + "\n" + errDiff).strip()
        if len(fullDiff):
            retVal = (progname, "FAIL", fullDiff)
 
    shutil.rmtree(tempDir)
    return retVal

class Sampler:
    def __init__(self, nsamples):
        self.nsamples = nsamples
        self.samples = []
        self.lastSample = 0
        self.sum = 0.0

    def sample(self, val):
        if len(self.samples) < self.nsamples:
            self.samples.append(val)
        else:
            self.sum -= self.samples[self.lastSample]
            self.samples[self.lastSample] = val
            self.lastSample = (self.lastSample + 1)  % len(self.samples)
        self.sum += val

    def estimate(self):
        return self.sum / len(self.samples)

testsDone = 0
tSampler = Sampler(200)

def printResult(argList):
    global testsDone, ntests, tstart, tSampler
    testsDone += 1
    (progname, status, diff) = argList
    if status != "OK" or args.verbose:
        if not args.verbose: print("") # Don't follow status line
        print(progname, '.'*(60 - len(progname) - len(status)), status)
    if status != "OK":
        print("        " + "\n        ".join(diff.split("\n")))
    if not args.verbose:
        # Print status line (important to keep it fixed-width)
        pct = 100.0 * testsDone / ntests
        timeElapsed = time.time() - tstart
        timePerTest = timeElapsed / testsDone
        tSampler.sample(timePerTest)
        timeLeft = (ntests - testsDone) * tSampler.estimate()
        header = "Completed %d tests" % testsDone
        trailer = "%3.1f%% | %.0fs left" % (pct, timeLeft)
        sys.stdout.write("\r%s %s %s" % (header, '.'*(60 - len(header) - len(trailer)), trailer))
        sys.stdout.flush()


# Parallel map procedure
# dsm: Based on http://noswap.com/blog/python-multiprocessing-keyboardinterrupt
def __init_pmap_worker():
    autoterm()
    signal.signal(signal.SIGINT, signal.SIG_IGN)

def pmap(func, iterable, workers, callback):
    if len(iterable) == 0: return []
    if workers == 1: # produces clearer stack traces
        def func_and_callback(x):
            res = func(x)
            callback(res)
            return res
        return list(map(func_and_callback, iterable))

    pool = multiprocessing.Pool(workers, initializer=__init_pmap_worker) if workers > 0 else multiprocessing.Pool(initializer=__init_pmap_worker) # as many workers as HW threads
    try:
        res = [pool.apply_async(func, [i], callback=callback) for i in iterable]
        pool.close()
        ret = []
        for r in res:
            while not r.ready(): r.wait(0.05) # 50ms, to catch keyboard interrupts
            ret.append(r.get())
    except KeyboardInterrupt:
        print("Caught KeyboardInterrupt, terminating workers")
        pool.terminate()
        pool.join()
        raise KeyboardInterrupt
    else:
        #print "pmap finished normally"
        pool.join()
        return ret

testDir = os.path.abspath(args.dir)
if os.path.exists(os.path.join(testDir, "runTargets.py")):
    sys.path.append(testDir)
    import runTargets
    def fullName(file, cmd, tgt=None):
        if tgt == None:
            return file + "_" + "cmd"
        # Sanitize parametrics inname
        s = tgt.replace("#", ".").replace("(", "").replace(",", ".").replace(")", "").replace(" ", "")
        return "_".join([file, cmd, s])
    compileCmds = [(fullName(file, "compile"), ["msc", os.path.join(testDir, file + ".ms")])
            for file in runTargets.compileTargets]
    simCmds = [(fullName(file, "sim", tgt), ["ms", "sim", os.path.join(testDir, file + ".ms"), tgt])
            for (file, tgt) in runTargets.simTargets]
    synthCmds = [(fullName(file, "synth", tgt), ["synth", os.path.join(testDir, file + ".ms"), tgt])
            for (file, tgt) in runTargets.synthTargets]
    # Do synth/sim first, since they take longer
    cmds = synthCmds + simCmds + compileCmds
    if hasattr(runTargets, "preRunHook"):
        preRunHook = runTargets.preRunHook
else:
    # If no runTargets, default strategy is to compile only (typ. this is for different types of error tests)
    def runCmd(srcFile):
        cmdList = ["msc", srcFile]
        # Find name of last (non-parametric) module, compile that
        with open(srcFile, "r") as f: input = f.read()
        strippedMultiline = re.sub('\/\*(.*?)\*\/', '', input, flags = re.MULTILINE | re.DOTALL)
        stripped = re.sub('\/\/(.*?)\n', '\n', strippedMultiline)
        m = None
        for m in re.finditer('module ([a-zA-Z0-9_]+);', input):
            pass # skip to last
        if m is not None:
            modName = m.group(1).strip()
            cmdList.append(modName)
        return cmdList

    tests = [path for path in [os.path.abspath(os.path.join(testDir, f))
                               for f in os.listdir(testDir)
                               if f.endswith('.ms')]]

    cmds = [(os.path.splitext(os.path.basename(test))[0],
        runCmd(test)) for test in tests]

if not os.path.exists(args.outdir): os.makedirs(args.outdir)
if args.resume:
    print("Finding non-started tests (WARNING: Manually remove incomplete or suspected incomplete tests, e.g., those with mtime close to the end)")
    todoCmds = []
    for cmd in cmds:
        (progname, _) = cmd
        if not os.path.exists(os.path.join(args.outdir, progname + ".out")) or not os.path.exists(os.path.join(args.outdir, progname + ".err")):
            todoCmds.append(cmd)
    print("Running %d out of %d tests" % (len(todoCmds), len(cmds)))
    cmds = todoCmds

# dsm: Choose workers to maximize throughput
ntests = len(cmds)
workers = args.workers
if workers == 0:
    workers = multiprocessing.cpu_count()
    if ntests < 2*workers: workers = ntests

tstart = time.time()
print("Running {} tests | {} workers".format(ntests, workers))
res = pmap(runAndVerify, cmds, workers, printResult)
print("\n%d/%d tests completed successfully in %.1f s" % (len([x for x in res if x[1] == "OK"]), ntests, time.time() - tstart))
