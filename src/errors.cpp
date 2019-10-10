#include <string>
#include <unordered_set>
#include "antlr4-runtime.h"
#include "errors.h"
#include "log.h"
#include "strutils.h"

using namespace antlr4;

// Error reporting
static std::unordered_set<std::string> warnMsgs, errMsgs;
static std::unordered_set<tree::ParseTree*> warnCtxs, errCtxs;
static size_t totalErrs = 0;
static size_t totalWarns = 0;
static bool reportAllMsgs = false;

void initReporting(bool reportAllErrors) {
    reportAllMsgs = reportAllErrors;
}

void reportMsg(bool isError, const std::string& msg,
        const std::string locInfo, tree::ParseTree* ctx) {
    auto& msgs = isError? errMsgs : warnMsgs;
    auto& ctxs = isError? errCtxs : warnCtxs;
    size_t& total = isError? totalErrs : totalWarns;
    if (msgs.count(msg)) {
        // Sometimes bsc derps out and spits the same error multiple times
        // (e.g. double-writes). If we have emitted EXACTLY the same error
        // already, then don't even count it as a total, regardless of
        // reportAllMsgs
        return;
    }
    if (reportAllMsgs || (!msgs.count(msg) && !ctxs.count(ctx))) {
        msgs.insert(msg);
        if (ctx) ctxs.insert(ctx);
        std::cerr << locInfo << msg << "\n";
    }
    total++;
}

void reportErr(const std::string& msg, const std::string locInfo,
        tree::ParseTree* ctx) { reportMsg(true, msg, locInfo, ctx); }

void reportWarn(const std::string& msg, const std::string locInfo,
        tree::ParseTree* ctx) { reportMsg(false, msg, locInfo, ctx); }

void exitIfErrors() {
    if (!totalErrs) return;
    if (totalErrs > errMsgs.size()) {
        auto omittedErrs = totalErrs - errMsgs.size();
        std::cerr << noteColored("note:") << " omitted " << omittedErrs
            << " errors similar to those reported; run with "
            << hlColored("--all-errors") << " to see all errors\n";
    }
    exit(-1);
}

// Error formatting / locs
std::string getLoc(const Token* tok) {
    assert(tok);
    std::stringstream ss;
    ss << tok->getTokenSource()->getSourceName() << ":" << tok->getLine() << ":" << tok->getCharPositionInLine() + 1;
    return ss.str();
}

std::string getSubLoc(const Token* tok) {
    assert(tok);
    std::stringstream ss;
    for (size_t i = 0; i < tok->getTokenSource()->getSourceName().size(); i++) ss << " ";
    ss << " " << tok->getLine() << ":" << tok->getCharPositionInLine() + 1;
    return ss.str();
}

Token* getStartToken(tree::ParseTree* pt) {
    assert(pt);
    auto ctx = dynamic_cast<ParserRuleContext*>(pt);
    auto tn = dynamic_cast<tree::TerminalNode*>(pt);
    assert(ctx || tn);
    return ctx? ctx->start : tn->getSymbol();
}

std::string getLoc(tree::ParseTree* pt) { return getLoc(getStartToken(pt)); }
std::string getSubLoc(tree::ParseTree* pt) { return getSubLoc(getStartToken(pt)); }
