#include <algorithm>
#include <cctype>
#include <iostream>
#include <filesystem>
#include <regex>
#include <unordered_set>
#include <variant>
#include "antlr4-runtime.h"
#include "argparse/argparse.hpp"
#include "errors.h"
#include "log.h"
#include "parse.h"
#include "strutils.h"
#include "translate.h"
#include "version.h"
#include "MinispecLexer.h"
#include "MinispecParser.h"

using namespace antlr4;

// Bluespec errors
typedef std::function<std::string(uint32_t, uint32_t)> TranslateLocFn;
typedef std::function<std::string(uint32_t, uint32_t, const std::vector<std::string>&)> ContextStrFn;
typedef std::function<tree::ParseTree*(uint32_t, uint32_t)> FindFn;

void reportBluespecOutput(std::string str, const SourceMap& sm, const std::string& topLevel) {
    typedef std::regex_iterator<std::string::const_iterator> regex_it;
    const regex_it rend;

    // C++ regexes suck... I want ECMAScript mode with multiline (it seems
    // lookahead doesn't work with std::regex::extended), but
    // std::regex:multiline is a C++17 feature, and isn't even in gcc 8.2! So
    // substitute all newlines with a line termination token that doesn't show
    // up in Bluespec output.
    const char* lineTerm = " _@%@_ ";
    replace(str, "\n", lineTerm);
    std::regex evRegex("(Warning|Error): (.*?)(?=(Error:|Warning:|$))");

    const std::string locRegexStr = "\"(\\S+)\",\\s+line\\s+(\\d+),\\s+column\\s+(\\d+)";
    std::regex locRegex(locRegexStr);
    std::regex hdrRegex(locRegexStr + ":\\s+\\((\\S+)\\)"); // include type

    auto translateLoc = [&](uint32_t line, uint32_t lineChar) {
        auto pt = sm.find(line, lineChar);
        if (pt) return getLoc(pt);
        else return "(translated bsv:" + std::to_string(line) + ":" + std::to_string(lineChar) + ")";
    };

    auto translateAllLocs = [&](std::string& msg) {
        std::smatch locMatch;
        while (std::regex_search(msg, locMatch, locRegex)) {
            std::string file = locMatch[1];
            uint32_t line = atoi(locMatch[2].str().c_str());
            uint32_t lineChar = atoi(locMatch[3].str().c_str());
            std::string loc;
            if (file == "Translated.bsv") {
                loc = translateLoc(line, lineChar);
            } else {
                loc = file + ":" + std::to_string(line) + ":" + std::to_string(lineChar);
            }
            replace(msg, locMatch[0], hlColored(loc));
        }
    };

    auto contextStrFn = [&](uint32_t line, uint32_t lineChar,
            const std::vector<std::string>& elems) -> std::string
    {
        tree::ParseTree* ctx = nullptr;
        for (auto elem : elems) {
            ctx = sm.find(line, lineChar, elem);
            if (ctx) break;
        }
        if (!ctx) ctx = sm.find(line, lineChar);
	if (ctx) return contextStr(ctx, {ctx});
        return "";
    };

    auto reportUnknownMsg = [&](bool isError, std::string msg) {
        replace(msg, lineTerm, "\n");
        translateAllLocs(msg);
        msg = (isError? errorColored("error:") : warnColored("warning:")) + " " + msg + "\n";
        reportMsg(isError, msg);
    };

    for (regex_it rit(str.begin(), str.end(), evRegex); rit != rend; rit++) {
        auto& m = *rit;
        bool isError = std::string(m[1]) == "Error";
        std::string msg = m[2];
        std::smatch hdrMatch;
        if (!std::regex_search(msg, hdrMatch, hdrRegex)) {
            // Special-case not-found top-level error
            if (msg.find("Command line:") >= 0 && msg.find("Unbound variable `mk")) {
                bool isModule = isupper(topLevel[0]);
                msg = errorColored("error:") + " cannot find top-level " + (isModule? "module" : "function") + " " + errorColored("'" + topLevel + "'");
                reportMsg(isError, msg);
            } else {
                reportUnknownMsg(isError, msg);
            }
            continue;
        }
        std::string file = hdrMatch[1];
        uint32_t line = atoi(hdrMatch[2].str().c_str());
        uint32_t lineChar = atoi(hdrMatch[3].str().c_str());
        std::string code = hdrMatch[4];
        std::string body = msg.substr(hdrMatch.length());
        if (file != "Translated.bsv") {
            reportUnknownMsg(isError, "in imported BSV file " + msg);
            continue;
        }

        replace(body, lineTerm, " ");
        replace(body, "  ", " ");
        std::string loc = translateLoc(line, lineChar);
        body = trim(body);
        std::string unprocessedBody = body;
        if (body.size()) body[0] = tolower(body[0]);
        translateAllLocs(body);

        // Find and highlight syntax elements
        std::vector<std::string> elems;
        std::regex elemRegex("`(.*?)'");
        std::smatch elemMatch;
        while (std::regex_search(body, elemMatch, elemRegex)) {
            std::string elem = elemMatch[1];
            // Translate all module constructors back to the module name
            if (elem.size() > 2 && elem.find("mk") == 0 && isupper(elem[2]))
                elem = elem.substr(2);
	    elems.push_back(elem);
	    replace(body, elemMatch[0], errorColored("'" + elem + "'"));
        }

        // Special-case a few codes; these rewrite body on success, o/w they fall through the default code
        if (code == "T0020" || code == "T0080") {
            // NOTE: T0020 is for expressions and T0080 is for functions, but
            // Bluespec seems to implement several constant as functions (e.g.,
            // True and False). So, we output exactly the same error message
            // for both
            std::regex typeRegex((code == "T0020")? "type error at: (.*?) Expected type: (.*?) Inferred type: (.*?)$" : "type error at the use of the following function: (.*?) The expected return type of the function: (.*?) The return type according to the use: (.*?)$");
            std::smatch match;
            if (std::regex_search(body, match, typeRegex)) {
                std::string elem = match[1];
                std::string expectedType = match[2];
                std::string type = match[3];
                body = "expression " + errorColored("'" + elem + "'") + " has type " + hlColored(type) + ", but use requires type " + hlColored(expectedType);
                elems.push_back(elem);
            }
        } else if (code == "T0031") {
	    // Some of these messages are followed by "The proviso was implied
	    // by expressions at the following positions:" clarifications;
	    // ignore those (don't match at end ($) only).
	    std::regex provisoRegex("no instances of the form:\\s+(\\S+)#\\((.*)\\)");
            std::smatch match;
            if (std::regex_search(body, match, provisoRegex)) {
                std::string typeclass = match[1];
                std::string type = match[2];
                if (typeclass == "Arith") {
                    body = "type " + hlColored(type) + " does not support arithmetic operations";
                } else if (typeclass == "Ord") {
                    body = "type " + hlColored(type) + " does not support comparison operations";
                } else if (typeclass == "Literal") {
                    body = "cannot convert literal to type " + hlColored(type);
                }
            }
	} else if (code == "T0003") {
	    // I see these only on mistyped literals, but unbound constructor
	    // is such a general message that who knows where else it may show
	    // up. So leave the translated error general.
	    replace(body, "unbound constructor", "undefined literal, type, or module");
        } else if (code == "T0004") {
	    replace(body, "unbound variable", "undefined variable or function");
	} else if (code == "T0007") {
	    replace(body, "unbound type constructor", "undefined type or module");
        } else if (code == "G0005") {
            std::regex blockedRegex("The assertion `fire_when_enabled' failed for rule `(.*?)' because it is blocked by rule (.*?) in the scheduler");
            std::smatch match;
            if (std::regex_search(unprocessedBody, match, blockedRegex)) {
                body = "rules " + errorColored(match[1]) + " and " + errorColored(match[2]) +
                    " conflict and cannot both fire every cycle (e.g., they both try to set the same input of a shared module)";
            }
        }

        std::stringstream ss;
        ss << hlColored(loc + ":") << " " << (isError? errorColored("error:") : warnColored("warning:")) << " " << body << "\n";
        ss << contextStrFn(line, lineChar, elems);
        reportMsg(isError, ss.str(), sm.getContextInfo(line, lineChar), sm.find(line, lineChar));
    }
}

struct RunResult {
    std::string output;
    int exitCode;
};

RunResult run(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) error("cannot invoke subprocess");
    std::stringstream bscOutput;
    size_t bufSize = 1024;
    char buf[bufSize];
    while (!feof(pipe)) if (fgets(buf, bufSize, pipe)) bscOutput << buf;
    RunResult res;
    res.output = bscOutput.str();
    res.exitCode = pclose(pipe);
    return res;
}

static std::string tmpDirStr = "";
void cleanupTmpDir() {
    if (!tmpDirStr.size()) return;
    std::filesystem::remove_all(tmpDirStr);
}

[[noreturn]] void uncaughtExceptionHandler() noexcept {
    // dsm: Why is C++ so retarded? rethrow?
    std::string exStr = "??";
    std::exception_ptr ep = std::current_exception();
    try {
        if (ep) {
            std::rethrow_exception(ep);
        } else {
            exStr = "invalid exception";
        }
    } catch(const std::exception& e) {
        exStr = e.what();
    }
    panic("uncaught exception: %s", exStr.c_str());
}

int main(int argc, const char* argv[]) {
    std::set_terminate(uncaughtExceptionHandler);

    argparse::ArgumentParser args;
    args.add_argument("inputFile")
        .help("input file")
        .default_value(std::string(""));
    args.add_argument("topLevel")
        .help("name of module/function to compile (if not given, checks input for correctness)")
        .default_value(std::string(""));
    args.add_argument("-o", "--output")
        .help("type of output(s) desired [default: sim]\n                  sim: simulation executable\n                  verilog (or v): Verilog file\n                  bsv: Bluespec file\n                  Use commas to specify multiple outputs (e.g., -o sim,verilog)")
        .default_value(std::string("sim"));
    args.add_argument("-p", "--path")
        .help("path for source files (for multiple directories, use : as separator)")
        .default_value(std::string(""));
    args.add_argument("-b", "--bscOpts")
        .help("extra options for the Bluespec compiler (use quotes for multiple options)")
        .default_value(std::string(""));
    args.add_argument("-v", "--version")
        .help("show version information")
        .default_value(false)
        .implicit_value(true);
    args.add_argument("--all-errors")
        .help("report all errors and warnings (by default, similar/repeating errors are filtered)")
        .default_value(false)
        .implicit_value(true);
    args.add_argument("--keep-tmps")
        .help("keep temporary files around (useful for compiler debugging)")
        .default_value(false)
        .implicit_value(true);

    PARSE_ARGS(args, argc, argv);

    if (args.get<bool>("--version")) {
        std::cout << "Minispec compiler version " << getVersion() << "\n";
        exit(0);
    }

    std::string inputFile = args.get<std::string>("inputFile");
    if (inputFile == "") error("no input file");
    std::string topLevel = args.get<std::string>("topLevel");

    // Find desired outputs
    bool bsvOut = false;
    bool simOut = false;
    bool verilogOut = false;
    bool defaultOut = args.is_default("--output");
    {
        std::string outsArg = args.get<std::string>("--output");
        std::string outs = outsArg;
        replace(outs, ",", " ");
        std::istringstream iss(outs);
        std::string out;
        while (iss >> out) {
            if (out == "bsv") bsvOut = true;
            else if (out == "sim") simOut = true;
            else if (out == "verilog" || out == "v") verilogOut = true;
            else error("invalid output type %s (full argument: %s)",
                    errorColored("'" + out + "'").c_str(),
                    errorColored("'" + outsArg + "'").c_str());
        }
    }

    // Other options
    initReporting(args.get<bool>("--all-errors"));

    // Construct the Minispec path, composed of: (1) the input file's
    // directory, (2) the directories in the --path flag, and (3) the current
    // directory. This way we catch current-folder includes to avoid some
    // corner cases, but without clobbering same-dir includes.
    std::vector<std::string> path;
    path.push_back(std::filesystem::path(inputFile).remove_filename());
    std::stringstream pathSs(args.get<std::string>("--path"));
    for (std::string dir; std::getline(pathSs, dir, ':'); )
        path.push_back(dir);
    path.push_back("");

    auto dedup = [](const std::vector<std::string>& v) {
        std::vector<std::string> res;
        std::unordered_set<std::string> elems;
        for (auto e : v) if (!elems.count(e)) {
            elems.insert(e);
            res.push_back(e);
        }
        return res;
    };
    path = dedup(path);

    // Parse all files. Exits on lexer/parser errors.
    std::vector<MinispecParser::PackageDefContext*> parsedTrees =
        parseFileAndImports(inputFile, path);

    // Translate files to Bluespec. Exits on elaboration errors.
    SourceMap sm = translateFiles(parsedTrees, topLevel);

    // Save translated code
    char tmpDir[128];
    sprintf(tmpDir, "tmp_msc_XXXXXX");
    if (mkdtemp(tmpDir) != tmpDir) error("could not create temporary directory");
    if (args.get<bool>("--keep-tmps")) {
        std::cout << "storing temporary files in " << hlColored(std::string(tmpDir)) << "\n";
    } else {
        tmpDirStr = tmpDir;
        atexit(cleanupTmpDir);
    }
    std::string bsvFileName = tmpDir + std::string("/Translated.bsv");
    std::ofstream stream(bsvFileName);
    if (!stream.good()) error("Could not open output file %s", bsvFileName.c_str());
    stream << sm.getCode() << "\n";
    stream.close();

    // bsc path is simply the path with a corrected base for relative dirs
    std::stringstream bscPath;
    for (std::string dir : path) {
        auto path = std::filesystem::path(dir);
        bscPath << (path.is_relative()? "../" : "") << dir << ":";
    }
    bscPath << "%:+";
    std::string bscOpts = "-p " + bscPath.str() + " " + args.get<std::string>("--bscOpts");
    //std::cout << "BSC options: " << bscOpts << "\n";

    // Invoke Bluespec compiler and check for type errors
    auto runBscCmd = [&](const std::string& cmd) {
        //std::cout << cmd << "\n";
        auto compileRes = run(cmd);
        reportBluespecOutput(compileRes.output, sm, topLevel);
        exitIfErrors();
	if (compileRes.exitCode != 0) {
            // If we didn't parse any error but bsc failed, this is typically
            // because bsc wasn't found. So print the output.
            error("could not compile file: %s", compileRes.output.c_str());
        }
    };

    std::string outName = topLevel;
    if (outName == "") {
        outName = std::filesystem::path(inputFile).stem();
    } else {
        // Sanitize parametrics
        replace(outName, "#", "_");
        replace(outName, ",", "_");
        replace(outName, "(", "");
        replace(outName, ")", "");
        replace(outName, " ", "");
        replace(outName, "'", "");
        replace(outName, "\t", "");
    }
    bool typechecked = false;

    if (simOut) {
        if (topLevel.size() && isupper(topLevel[0])) {
            std::stringstream cmd;
            cmd << "(cd " << tmpDir << " && bsc " << bscOpts << " -sim -g '" << sm.getTopModule() << "' -u Translated.bsv) 2>&1 >/dev/null";
            runBscCmd(cmd.str());
            typechecked = true;

            // Link simulation executable
            cmd.str("");
            cmd << "(cd " << tmpDir << " && bsc " << bscOpts << " -sim -e '" <<  sm.getTopModule() << "' -o '../" << outName << "') 2>&1 >/dev/null";
            runBscCmd(cmd.str());
            std::cout << "produced simulation executable " << hlColored(outName) << "\n";
        } else if (!defaultOut) {
            const char* problem = (topLevel == "")?
                "did not provide a top-level module" :
                "specified a top-level function, which can't be simulated";
            warn("you asked for sim output but %s, so not producing simulation executable", problem);
        }
    }

    if (verilogOut) {
        if (topLevel.size()) {
            std::stringstream cmd;
            cmd << "(cd " << tmpDir << " && bsc " << bscOpts << " -verilog -g '" << sm.getTopModule() << "' -u Translated.bsv) 2>&1 >/dev/null";
            runBscCmd(cmd.str());
            typechecked = true;

            cmd.str("");
            cmd << "cp '" << tmpDir << "/" << sm.getTopModule() << ".v' '" << outName << ".v'";
            run(cmd.str());
            std::cout << "produced verilog output " << hlColored(outName + ".v") << "\n";
        } else if (!defaultOut) {
            warn("you asked for verilog output but did not provide a top-level module or function, so not producing verilog");
        }
    }

    if (!typechecked) {
        std::stringstream cmd;
        cmd << "(cd " << tmpDir << " && bsc " << bscOpts << " -u Translated.bsv) 2>&1 >/dev/null";
        runBscCmd(cmd.str());
        typechecked = true;
        std::cout << "no errors found on " << hlColored(inputFile) << "\n";
    }

    if (bsvOut) {
        auto cpRes = run("cp " + std::string(tmpDir) + "/Translated.bsv '" + outName + ".bsv'");
        if (cpRes.exitCode != 0) {
            error("could not copy bsv file");
        }
        std::cout << "produced bsv output " << hlColored(outName + ".bsv") << "\n";
    }

    return 0;
}
