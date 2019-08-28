#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include "antlr4-runtime.h"
#include "log.h"
#include "parse.h"
#include "strutils.h"
#include "MinispecLexer.h"
#include "MinispecParser.h"

using namespace antlr4;

// NOTE: Auto-generated stmtTokens & exprTokens IntervalSets, see genTokens.cpp
#include "tokenSets.inc"

static bool contained(const misc::IntervalSet& a, const misc::IntervalSet& b) { return a.Or(b) == a; }

static std::string formatExpectedTokens(misc::IntervalSet tokens, const dfa::Vocabulary& vocabulary) {
    std::vector<std::string> res;
    // The set of tokens alone can be overwhelming, so trim by detecting those
    // corresponding to syntax elements that may be lots of tokens.
    //
    // NOTE: Inferring syntax elements from token sets is a COLOSSAL HACK, and
    // works well only because of the structure of the grammar (a statement can
    // be an expression and nothing seems to be a non-trivial superset of a
    // statement...). However, I don't see a way to get the parser/ATN to spit
    // out potential syntax elements instead of expected tokens, so it'll have
    // to do unless we want to go to a hand-written parser.
    if (contained(tokens, stmtTokens)) {
        tokens = tokens.subtract(stmtTokens);
        res.push_back("statement");
    }
    if (contained(tokens, exprTokens)) {
        tokens = tokens.subtract(exprTokens);
        res.push_back("expression");
    }

    for (auto token: tokens.toList()) {
        std::string tokStr;
        if (token == -1) tokStr = "<EOF>";
        else if (token == -2) tokStr = "<EPSILON>";
        else if (token == MinispecParser::UpperCaseIdentifier) tokStr = "type or module name (uppercase identifier)";
        else if (token == MinispecParser::LowerCaseIdentifier) tokStr = "variable or function name (lowercase identifier)";
        else if (token == MinispecParser::DollarIdentifier) tokStr = "system function name (identifier beginning with $)";
        else if (token == MinispecParser::IntLiteral) tokStr = "integer literal";
        else if (token == MinispecParser::StringLiteral) tokStr = "string literal";
        else tokStr = vocabulary.getDisplayName(token);
        res.push_back(tokStr);
    }

    assert(res.size()); // we must expect something...
    if (res.size() == 1) {
        return hlColored(res[0]);
    } else if (res.size() == 2) {
        return hlColored(res[0]) + " or " + hlColored(res[1]);;
    } else {
        // In this house we use the Oxford comma
        std::stringstream ss;
        for (uint32_t i = 0; i < res.size() - 1; i++) {
            ss << hlColored(res[i]) << ", ";
        }
        ss << "or " << hlColored(res.back());
        return ss.str();
    }
}

class ErrorListener : public BaseErrorListener {
    public:
        typedef std::function<std::string_view(uint32_t)> GetLineFn;
        ErrorListener(GetLineFn getLine) : getLine(getLine) {}

        virtual void syntaxError(Recognizer *recognizer, Token *offendingSymbol,
                                 size_t line, size_t charPositionInLine,
                                 const std::string &msg, std::exception_ptr e) override {
            std::stringstream errLoc;
            errLoc << recognizer->getInputStream()->getSourceName() << ":" << line << ":" << charPositionInLine + 1;

            // Handle token recognition errors here, since the lexer doesn't use an ErrorStrategy we can override
            std::string errMsg = msg;
            std::string errToken = "";
            if (msg.find("token recognition error") == 0) {
                // NOTE: If ANTLR's message changes, this code should not cause
                // an error, but default to giving ANTLR's message
                auto errTokenStart = msg.find("'");
                auto errTokenEnd = msg.rfind("'");
                if (errTokenStart != std::string::npos && errTokenEnd != std::string::npos && (errTokenStart + 1) < errTokenEnd) {
                    errToken = msg.substr(errTokenStart + 1, errTokenEnd - errTokenStart - 1);
                    replace(errToken, "\\n", "");
                    replace(errToken, "\\r", "");
                    replace(errToken, "\\t", "");

                    if (errToken.size() > 0 && errToken[0] == '\"') {
                        errMsg = "unterminated string";
                    } else if (errToken.size() > 0 && errToken[0] == '\'') {
                        errMsg = "invalid integer literal";
                    } else {
                        errMsg = "invalid input";
                    }
                    errMsg += " " + errorColored("'" + errToken + "'");
                }
            }

            std::cerr << hlColored(errLoc.str()) << ": " << errorColored("error: ") << errMsg << "\n";

            // Print preceding context if this is the first token in the line
            if (offendingSymbol && offendingSymbol->getTokenIndex() > 0) {
                TokenStream* tokenStream = dynamic_cast<TokenStream*>(recognizer->getInputStream());
                assert(tokenStream);
                Token* prevToken = tokenStream->get(offendingSymbol->getTokenIndex() - 1);
                size_t prevLine = prevToken->getLine();
                if (prevLine < line && (line - prevLine) < 5) {
                    for (size_t i = prevLine; i < line; i++)
                        std::cerr << "    " << getLine(i) << "\n";
                }
            }

            // Print error's line
            std::string lineStr = std::string(getLine(line));
            size_t symbolStart = charPositionInLine;
            size_t symbolLen = offendingSymbol? offendingSymbol->getText().size() :
                errToken.size()? errToken.size() : 0;
            symbolLen = std::min(symbolLen, lineStr.size() - symbolStart);
            size_t symbolEnd = symbolStart + symbolLen;
            std::cerr << "    " << lineStr.substr(0, symbolStart) <<
                errorColored(lineStr.substr(symbolStart, symbolLen)) <<
                lineStr.substr(symbolEnd) << "\n";

            // Until we refine recovery, bail on first error; others are often confusing
            throw ParseCancellationException();
        }

    private:
        GetLineFn getLine;
};

std::string getContextName(RuleContext* ctx) {
    if (dynamic_cast<MinispecParser::FunctionDefContext*>(ctx)) return "function definition";
    if (dynamic_cast<MinispecParser::MethodDefContext*>(ctx)) return "method definition";
    if (dynamic_cast<MinispecParser::ModuleDefContext*>(ctx)) return "module definition";

    if (dynamic_cast<MinispecParser::ExpressionContext*>(ctx)) return "expression";
    if (dynamic_cast<MinispecParser::IfStmtContext*>(ctx)) return "if statement";
    if (dynamic_cast<MinispecParser::ForStmtContext*>(ctx)) return "for loop";
    if (dynamic_cast<MinispecParser::StmtContext*>(ctx)) return "statement";

    if (dynamic_cast<MinispecParser::ArgContext*>(ctx)) return "argument";
    if (dynamic_cast<MinispecParser::ArgsContext*>(ctx)) return "arguments";
    if (dynamic_cast<MinispecParser::ArgFormalContext*>(ctx)) return "argument definition";
    if (dynamic_cast<MinispecParser::ArgFormalsContext*>(ctx)) return "arguments list";

    if (dynamic_cast<MinispecParser::ParamsContext*>(ctx)) return "parameter";
    if (dynamic_cast<MinispecParser::ParamsContext*>(ctx)) return "parameters";
    if (dynamic_cast<MinispecParser::ParamFormalContext*>(ctx)) return "parameter definition";
    if (dynamic_cast<MinispecParser::ParamFormalsContext*>(ctx)) return "parameters list";

    if (dynamic_cast<MinispecParser::TypeContext*>(ctx)) return "type";
    if (dynamic_cast<MinispecParser::TypeIdContext*>(ctx)) return "type id";

    if (dynamic_cast<MinispecParser::TypeDeclContext*>(ctx)) return "type declaration";
    if (dynamic_cast<MinispecParser::VarDeclContext*>(ctx)) return "variable declaration";

    return "";
}

class ErrorStrategy : public DefaultErrorStrategy {
    protected:
        virtual void reportInputMismatch(Parser *recognizer, const InputMismatchException &e) override {
            std::string contextName = getContextName(e.getCtx());
            std::string expectedText = formatExpectedTokens(e.getExpectedTokens(), recognizer->getVocabulary());
            // Now that formatExpectedTokens translates to syntax elements, avoid
            // printing "when parsing expression, expected expression" and
            // similar redundtant messages.
            bool printWhenParsing = (contextName != "") && (hlColored(contextName) != expectedText);
            std::string msg = "mismatched input " + errorColored(getTokenErrorDisplay(e.getOffendingToken())) +
                (printWhenParsing? (" when parsing " + contextName) : "") + ", expected " + expectedText;
            recognizer->notifyErrorListeners(e.getOffendingToken(), msg, std::make_exception_ptr(e));
        }

        virtual void reportUnwantedToken(Parser *recognizer) override {
            if (inErrorRecoveryMode(recognizer)) return;
            beginErrorCondition(recognizer);

            Token *t = recognizer->getCurrentToken();
            std::string tokenName = getTokenErrorDisplay(t);
            std::string msg = "extraneous input " + errorColored(tokenName) +
                ", expected " + formatExpectedTokens(getExpectedTokens(recognizer), recognizer->getVocabulary());
            recognizer->notifyErrorListeners(t, msg, nullptr);
        }

        virtual void reportMissingToken(Parser *recognizer) override {
            if (inErrorRecoveryMode(recognizer)) return;
            beginErrorCondition(recognizer);

            std::string expectedText = formatExpectedTokens(getExpectedTokens(recognizer), recognizer->getVocabulary());
            Token *t = recognizer->getCurrentToken();
            std::string msg = "missing " + expectedText + " before " + errorColored(getTokenErrorDisplay(t));
            recognizer->notifyErrorListeners(t, msg, nullptr);
        }

        void reportNoViableAlternative(Parser *recognizer, const NoViableAltException &e) override {
            TokenStream *tokens = recognizer->getTokenStream();
            std::string input;
            if (tokens != nullptr) {
                if (e.getStartToken()->getType() == Token::EOF) {
                    input = "<EOF>";
                } else {
                    input = tokens->getText(e.getStartToken(), e.getOffendingToken());
                }
            } else {
                input = "<unknown input>";
            }
            std::string msg = "cannot parse " + errorColored(escapeWSAndQuote(input));
            recognizer->notifyErrorListeners(e.getOffendingToken(), msg, std::make_exception_ptr(e));
        }
};

struct ParsedFile {
    const std::string data;
    const std::vector<std::string_view> lines;
    std::vector<ParsedFile*> imports;

    // Used by the constructor
    static std::vector<std::string_view> getLines(const std::string& str) {
        std::vector<std::string_view> res;
        const char* cStr = str.c_str();
        size_t lastPos = 0;
        for (auto pos = str.find("\n"); pos != std::string::npos; pos = str.find("\n", pos + 1)) {
            res.push_back(std::string_view(cStr + lastPos, pos - lastPos));
            lastPos = pos + 1;
        }
        // Edge case: catch last line if file has no newline at end
        if (str.size() > lastPos) {
            res.push_back(std::string_view(cStr + lastPos, str.size() - lastPos));
        }
        //for (auto x : res) info("|%s|", std::string(x).c_str());
        return res;
    }

    std::string_view getLine(uint32_t line) {
        assert(line > 0);  // line is 1-based
        return (line <= lines.size())? lines[line-1] : "";
    }

    ANTLRInputStream input;
    MinispecLexer lexer;
    CommonTokenStream tokenStream;
    MinispecParser parser;
    ErrorListener errorListener;
    MinispecParser::PackageDefContext* tree;

    ParsedFile(const std::string& fileName, std::ifstream& stream) :
        data(std::istreambuf_iterator<char>(stream), {}), lines(getLines(data)),
        input(data), lexer(&input), tokenStream(&lexer), parser(&tokenStream),
        errorListener([&] (uint32_t line) { return this->getLine(line); }) {
            input.name = fileName;
            lexer.removeErrorListeners();
            lexer.addErrorListener(&errorListener);
            parser.removeErrorListeners();
            parser.addErrorListener(&errorListener);
            parser.setErrorHandler(std::make_shared<ErrorStrategy>());
            ParsedFiles[tokenStream.getTokenSource()] = this;
            tree = parser.packageDef();
    }

    static ParsedFile* Get(TokenSource* tokenSource) { return ParsedFiles[tokenSource]; }

    private:
        static std::unordered_map<TokenSource*, ParsedFile*> ParsedFiles;
};

std::unordered_map<TokenSource*, ParsedFile*> ParsedFile::ParsedFiles;

TokenStream* getTokenStream(ParserRuleContext* ctx) {
    return &ParsedFile::Get(ctx->start->getTokenSource())->tokenStream;
}

ParsedFile* parseFile(const std::string& fileName) {
    std::ifstream stream;
    // We generate the stream here due to RAII restrictions (lexing and parsing
    // are done in ParsedFile's constructor).
    stream.open(fileName);
    if (!stream.good()) error("Could not read source file %s", fileName.c_str());
    try {
        auto parsedFile = new ParsedFile(fileName, stream);
        return parsedFile;
    } catch (ParseCancellationException& p) {
        error("could not parse file %s", fileName.c_str());
    }
}

std::string findImportedFile(MinispecParser::IdentifierContext* importItem, ParsedFile* parsedFile, const std::vector<std::string>& path) {
    std::string fileName = importItem->getText() + ".ms";
    struct stat sb;
    for (auto dir : path) {
        std::string fullName = std::filesystem::path(dir) / fileName;
        if (stat(fullName.c_str(), &sb) == 0) return fullName;
    }
    error("Could not find import %s from parsed file %s", fileName.c_str(),
            parsedFile->tokenStream.getSourceName().c_str());
}

ParsedFile* parseFileAndImports(std::unordered_map<std::string, ParsedFile*>& parsedFiles,
        const std::string& fileName, const std::vector<std::string>& path) {
    auto it = parsedFiles.find(fileName);
    if (it != parsedFiles.end()) {
        // Already parsed
        return it->second;
    } else {
        auto parsedFile = parseFile(fileName);
        parsedFiles[fileName] = parsedFile;

        for (auto stmt : parsedFile->tree->packageStmt()) {
            if (auto importDecl = stmt->importDecl()) {
                for (auto importItem : importDecl->identifier()) {
                    std::string importFile = findImportedFile(importItem, parsedFile, path);
                    auto parsedImport = parseFileAndImports(parsedFiles, importFile, path);
                    parsedFile->imports.push_back(parsedImport);
                }
            }
        }
        return parsedFile;
    }
}

std::vector<MinispecParser::PackageDefContext*> parseFileAndImports(const std::string& fileName, const std::vector<std::string>& path) {
    std::unordered_map<std::string, ParsedFile*> parsedFilesMap;
    ParsedFile* parsedFile = parseFileAndImports(parsedFilesMap, fileName, path);

    // Topologically sort files and detect import cycles
    struct TopoSort {
        std::vector<ParsedFile*> path;
        void topoSort(ParsedFile* pf, std::vector<MinispecParser::PackageDefContext*>& out) {
            auto it = std::find(path.begin(), path.end(), pf);
            if (it != path.end()) {
                std::stringstream ss;
                while (it != path.end()) ss << (*it++)->tokenStream.getSourceName() << " -> ";
                ss << pf->tokenStream.getSourceName();
                error("import cycle detected: %s", ss.str().c_str());
            }
            if (std::find(out.begin(), out.end(), pf->tree) == out.end()) {
                path.push_back(pf);
                for (auto i : pf->imports) topoSort(i, out);
                path.pop_back();
                out.push_back(pf->tree);
            }
        }
    };
    std::vector<MinispecParser::PackageDefContext*> sortedTrees;
    TopoSort().topoSort(parsedFile, sortedTrees);
    return sortedTrees;
}

MinispecParser::PackageDefContext* parseSingleFile(const std::string& fileName) {
    return parseFile(fileName)->tree;
}

std::string contextStr(tree::ParseTree* pt, std::vector<tree::ParseTree*> highlights) {
    Token* startToken;
    Token* endToken;
    {
        if (auto ctx = dynamic_cast<ParserRuleContext*>(pt)) {
            startToken = ctx->start;
            endToken = ctx->stop;
        } else {
            auto tn = dynamic_cast<tree::TerminalNode*>(pt);
            assert(tn);
            startToken = endToken = tn->getSymbol();
        }
    }

    auto startLine = startToken->getLine();
    auto endLine = std::max(endToken->getLine(), startLine);
    std::stringstream ss;
    std::vector<uint32_t> lineOffsets;
    lineOffsets.push_back(0);
    for (auto line = startLine; line <= endLine; line++) {
        std::string_view sv = ParsedFile::Get(startToken->getTokenSource())->getLine(line);
        ss << sv << "\n";
        lineOffsets.push_back(lineOffsets.back() + sv.size() + 1);
    }
    std::string str = ss.str();

    std::vector<std::tuple<uint32_t, uint32_t>> hlRanges;
    for (auto p : highlights) {
        uint32_t startPos, len;
        if (auto c = dynamic_cast<ParserRuleContext*>(p)) {
            startPos = lineOffsets[c->start->getLine() - startLine] + c->start->getCharPositionInLine();
            len = getTokenStream(c)->getText(c->getSourceInterval()).size();
        } else {
            auto t = dynamic_cast<tree::TerminalNode*>(pt);
            assert(t);
            startPos = lineOffsets[t->getSymbol()->getLine() - startLine] + t->getSymbol()->getCharPositionInLine();
            len = t->getSymbol()->getText().size();
        }
        hlRanges.push_back(std::make_tuple(startPos, len));
    }
    std::sort(hlRanges.begin(), hlRanges.end());

    std::stringstream hlSs;
    size_t pos = 0;
    for (auto r : hlRanges) {
        uint32_t startPos, len;
        std::tie(startPos, len) = r;
        if (startPos < pos) continue;  // was nested within previous highlight
        hlSs << str.substr(pos, startPos - pos) << errorColored(str.substr(startPos, len));
        pos = startPos + len;
    }
    if (pos < str.size()) hlSs << str.substr(pos);
    std::string hlStr = hlSs.str();
    replace(hlStr, "\n", "\n    ");
    return "    " + hlStr.substr(0, hlStr.size() - 4);  // omit space after last newline
}
