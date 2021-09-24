import os

buildDir = "build/"
srcDir = "src/"
antlrJar = "antlr4.jar"
antlrBase = "antlr4/"
antlrPath = os.path.join(antlrBase, "runtime/Cpp/runtime/src/")
antlrLib = os.path.join(antlrBase, "runtime/Cpp/dist/libantlr4-runtime.a")
env = Environment(ENV = os.environ)
env.VariantDir(buildDir, srcDir, duplicate=0)
env.Append(CPPPATH = [antlrPath, buildDir])
env.Append(CPPFLAGS = ["-g", "-std=c++17", "-Wall"])
env.Append(LIBS = [File(antlrLib), "stdc++fs"])

# Optional, builds a static executable (to avoid deps issues on other distros)
env.Append(LINKFLAGS=['-static', '-pthread', '-Wl,--whole-archive,-lpthread,--no-whole-archive'])

# Silence spurious warnings from ANTLR code
env.Append(CPPFLAGS = ["-Wno-attributes"])

# Coverage builds
AddOption('--cov', dest='cov', default=False, action='store_true', help='Build with coverage (gcov/lcov)')
if GetOption("cov"):
    env.Append(CPPFLAGS = ["-O0", "--coverage"], LINKFLAGS = ["--coverage"])
else:
    env.Append(CPPFLAGS = ["-O2"])

# Automatically download and build ANTLR4 if not present
def cmd(c):
    exitCode = os.WEXITSTATUS(os.system(c))
    if exitCode != 0:
        print("Command %s failed with exit code %d" % (c, exitCode))
if not os.path.exists(antlrJar):
    print("Downloading ANTLR jar...")
    cmd("wget -nv https://www.antlr.org/download/antlr-4.7.2-complete.jar -O " + antlrJar)
if not os.path.exists(antlrBase):
    print("Downloading ANTLR sources...")
    cmd("wget -nv https://github.com/antlr/antlr4/archive/4.7.2.tar.gz -O 4.7.2.tar.gz")
    cmd("tar xfz 4.7.2.tar.gz && mv antlr4-4.7.2 %s && rm 4.7.2.tar.gz" % (antlrBase,))
if not os.path.exists(antlrLib):
    print("Building ANTLR runtime lib...")
    cmd("cd %s/runtime/Cpp/ && mkdir -p build && cd build && cmake .. && make -j`nproc`" % (antlrBase,))

# Version tag. Depends on all sources and git repo state
allSrcs = [f for d, _, _ in os.walk("src") for f in Glob(d + "/*")]
versionFile = os.path.join(buildDir, "version.inc")
env.Command(versionFile, allSrcs + [".git/index", "SConstruct"],
    'echo "const std::string version = \\"`python3 misc/gitver.py`, built on `date`\\";" >>' + versionFile)
# Recent scons versions have added "improved cycle detection logic" that flags
# a circular dep on versionFile. Looks like this is because scons does not
# distinguish between .cpp and .o dependences, so e.g., build/version.inc deps
# on src/msc.cpp, and src/msc.o deps on build/version.inc --> "cycle".
# Just ignoring the cycle works fine.
# See https://stackoverflow.com/questions/41228497/scons-cyclic-dependency
env.Ignore(versionFile, versionFile)

# ANTLR-generated grammar C++ files
grammarSrc = os.path.join(srcDir, "Minispec.g4")
def getGrammarFiles(ext):
    grammarStems = ["Lexer", "Parser", "BaseListener", "Listener"]
    return [os.path.join(buildDir, "Minispec" + stem + ext) for stem in grammarStems]
grammarHdrs = getGrammarFiles(".h")
grammarCpps = getGrammarFiles(".cpp")
grammarOuts = grammarHdrs + grammarCpps
env.Command(grammarOuts, grammarSrc,
    "java -classpath %s -Xmx500M org.antlr.v4.Tool -o %s -Xexact-output-dir -Dlanguage=Cpp %s"
        % (antlrJar, buildDir, grammarSrc))

# Grammar-dependent auto-generated files
genTokensProg = os.path.join(buildDir, "genTokens")
env.Program(genTokensProg, grammarCpps + [os.path.join(buildDir, "genTokens.cpp")])
tokenSets = os.path.join(buildDir, "tokenSets.inc")
env.Command(tokenSets, genTokensProg, "./%s >> %s" % (genTokensProg, tokenSets))

# Turn the prelude into an includable file
preludeSrc = os.path.join(srcDir, "MinispecPrelude.bsv")
preludeInc = os.path.join(buildDir, "MinispecPrelude.inc")
env.Command(preludeInc, preludeSrc, "xxd -i < %s >> %s" % (preludeSrc, preludeInc))

# Minispec compiler
mscCpps = ["msc.cpp", "errors.cpp", "log.cpp", "parse.cpp", "strutils.cpp", "translate.cpp", "version.cpp"]
env.Program("msc", grammarCpps + [os.path.join(buildDir, f) for f in mscCpps])

# Minispec file combiner (for Jupyter kernel)
combineCpps = ["combine.cpp", "log.cpp", "parse.cpp", "strutils.cpp"]
env.Program("minispec-combine", grammarCpps + [os.path.join(buildDir, f) for f in combineCpps])
