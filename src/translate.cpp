#include <algorithm>
#include <cctype>
#include <iostream>
#include <unordered_set>
#include <variant>
#include "antlr4-runtime.h"
#include "errors.h"
#include "log.h"
#include "parse.h"
#include "strutils.h"
#include "translate.h"
#include "version.h"
#include "MinispecLexer.h"
#include "MinispecParser.h"
#include "MinispecBaseListener.h"

using namespace antlr4;
using antlrcpp::Any;
using misc::Interval;
using std::string;
using std::stringstream;

struct ParametricUse {
    std::string name;
    bool escape;
    std::vector<Any> params; // Each param may be an int64_t or a ParametricUsePtr

    bool operator==(const ParametricUse& other) const {
        if (name != other.name) return false;
        if (params.size() != other.params.size()) return false;
        for (uint32_t i = 0; i < params.size(); i++) {
            Any p1 = params[i];
            Any p2 = other.params[i];
            if (p1.is<int64_t>()) {
                if (!p2.is<int64_t>()) return false;
                if (p1.as<int64_t>() != p2.as<int64_t>()) return false;
            } else {
                assert(p1.is<std::shared_ptr<ParametricUse>>());
                if (!p2.is<std::shared_ptr<ParametricUse>>()) return false;
                if (!(*p1.as<std::shared_ptr<ParametricUse>>() == *p2.as<std::shared_ptr<ParametricUse>>())) return false;
            }
        }
        return true;
    }

    std::string str(bool alreadyEscaped = false) const {
        std::stringstream ss;
        bool shouldEscape = escape && !alreadyEscaped;
        if (shouldEscape) {
            ss << "\\";
            alreadyEscaped = true;
        }
        ss << name;
        if (params.size()) ss << "#(";
        for (size_t i = 0; i < params.size(); i++) {
            Any p = params[i];
            if (p.is<int64_t>()) ss << p.as<int64_t>();
            else ss << p.as<std::shared_ptr<ParametricUse>>()->str(alreadyEscaped);
            ss << ((i == params.size() - 1)? ")" : ",");
        }
        if (shouldEscape) ss << " ";
        return ss.str();
    }
};

typedef std::shared_ptr<ParametricUse> ParametricUsePtr;

class Elaborator;
typedef std::unordered_map<std::string, std::tuple<ParserRuleContext*, Elaborator*>> ParametricsMap;

namespace std {
    template<> struct hash<ParametricUse> {
        size_t operator()(const ParametricUse& pu) const noexcept {
            std::hash<std::string> strHash;
            size_t res = strHash(pu.name);
            for (Any p : pu.params) {
                size_t h;
                if (p.is<int64_t>()) h = (size_t) p.as<int64_t>();
                else h = operator()(*p.as<ParametricUsePtr>());
                res = ((res << 63) | (res >> 1)) ^ h;
            }
            return res;
        }
    };
}

typedef std::function<Any(tree::ParseTree*)> GetValueFn;

struct Skip {};

class TranslatedCode;
typedef std::shared_ptr<TranslatedCode> TranslatedCodePtr;

class TranslatedCode {
    private:
        GetValueFn getValue;
        const bool skipSpaces;

        typedef SourceMap::Range Range;
        std::map<Range, tree::ParseTree*> dstToSrc;
        std::map<Range, std::string> dstToInfo;
        std::stringstream code;
        std::vector<std::tuple<tree::ParseTree*, ssize_t>> emitStack;

        typedef std::tuple<ParametricUse, tree::ParseTree*> ParametricUseInfo;
        std::vector<ParametricUseInfo> parametricUsesEmitted;

        ssize_t pos() {
            ssize_t wrPos = code.tellp();  // returns -1 if empty
            return (wrPos == -1)? 0 : wrPos;
        }

    public:
        TranslatedCode(GetValueFn getValue, bool skipSpaces = false)
            : getValue(getValue), skipSpaces(skipSpaces) {}

        // Build SourceMap data from ctx, emitting all children and (1)
        // patching with elaborated values, (2) integrating internally
        // elaborated sourcemaps.
        void emit(tree::ParseTree* ctx) {
            if (!ctx) return;
            ParserRuleContext* prCtx = dynamic_cast<ParserRuleContext*>(ctx);
            emitStart(ctx);
            Any value = getValue(ctx);
            if (value.is<int64_t>()) {
                code << value.as<int64_t>();
            } else if (value.is<bool>()) {
                code << (value.as<bool>()? "True" : "False");
            } else if (value.is<const char*>()) {
                code << value.as<const char*>();
            } else if (value.is<ParametricUsePtr>()) {
                auto v = value.as<ParametricUsePtr>();
                emit(v->str());
                parametricUsesEmitted.push_back(std::make_tuple(*v, ctx));
            } else if (value.is<Skip>()) {
                // Emit nothing
            } else if (value.is<TranslatedCodePtr>()) {
                const TranslatedCode& tc = *value.as<TranslatedCodePtr>();
                assert(tc.emitStack.empty());
                // Merge with ours
                ssize_t offset = pos();
                for (const auto& [range, srcCtx] : tc.dstToSrc) {
                    auto& [start, end] = range;
                    dstToSrc[std::make_tuple(start + offset, end + offset)] = srcCtx;
                }
                for (const auto& [range, info] : tc.dstToInfo) {
                    auto& [start, end] = range;
                    dstToInfo[std::make_tuple(start + offset, end + offset)] = info;
                }
                for (const auto& pui : tc.parametricUsesEmitted) {
                    parametricUsesEmitted.push_back(pui);
                }
                code << tc.code.str();
            } else if (prCtx) {
                auto tokenStream = getTokenStream(prCtx);
                for (uint32_t i = 0; i < prCtx->children.size(); i++) {
                    // Print inter-ctx whitespace
                    if (!skipSpaces && i > 0) {
                        Interval prev = prCtx->children[i-1]->getSourceInterval();
                        Interval cur = prCtx->children[i]->getSourceInterval();
                        if (prev.b + 1 < cur.a) {
                            code << tokenStream->getText(Interval(prev.b + 1, cur.a -1));
                        }
                    }
                    emit(ctx->children[i]);
                }
            } else {
                emit(ctx->getText());
            }
            emitEnd();
        }

        // Templated emit() for text or text + parse trees
        void emit(std::string_view sv) {
            code << sv;
        }

        // emit() is templated to take in any number of arguments, which
        // can be string_views (typically string literals) or parse tree nodes.
        // For convenience, emit() does NOT add spaces between string
        // literals and parse tree elements, but does add a space between
        // consecutive parse tree elements. This produces code without
        // extraneous spaces, yet doesn't force adding explicit spaces among
        // tree elements.
        template<typename... Args> void emit(std::string_view x, Args... args) {
            emit(x);
            emit(args...);
        }
        template<typename... Args> void emit(tree::ParseTree* tree, tree::ParseTree* next, Args... args) {
            emit(tree);
            emit(" ");  // leave a space between tree elements
            emit(next, args...);
        }
        template<typename... Args> void emit(tree::ParseTree* tree, std::string_view next, Args... args) {
            emit(tree);
            emit(next, args...);
        }

        void emitLine() { emit("\n"); }
        template<typename... Args> void emitLine(Args... args) { emit(args...); emitLine(); }

        void emitStart(tree::ParseTree* ctx) {
            emitStack.push_back(std::make_tuple(ctx, pos()));
        }

        void emitEnd(const std::string ctxInfo = "") {
            assert(!emitStack.empty());
            auto [ctx, startPos] = emitStack.back();
            emitStack.pop_back();

            ssize_t endPos = pos();
            if (startPos == endPos) return;

            Range range = std::make_tuple(startPos, endPos);
            dstToSrc[range] = ctx;
            if (ctxInfo != "") dstToInfo[range] = ctxInfo;
        }

        SourceMap getSourceMap(const std::string& simModule = "") const {
            return SourceMap(dstToSrc, dstToInfo, code.str(), simModule);
        }

        std::vector<ParametricUseInfo> dequeueParametricUsesEmitted() {
            std::vector<ParametricUseInfo> res = std::move(parametricUsesEmitted);
            parametricUsesEmitted.clear();  // needed, move-assignment leaves src container in unspecified state (jeez STL...)
            return res;
        }
};

class IntegerContext {
    public:
        // An Integer is INVALID if it has been declared but doesn't hold a
        // value, VALID if it holds a value, and POISONED if its value has been
        // defined outside an if/else or case block, then modified inside it.
        // POISONED has the same semantics as INVALID (trying to use them is an
        // error), but it's a different state to give better error reporting
        // (INVALID -> uninitialized value, whereas POISONED -> flow-sensitive
        // changes).
        enum IntegerState {INVALID, VALID, POISONED};

        struct IntegerData {
            IntegerState state;
            int64_t value;
        };

    private:
        typedef std::shared_ptr<IntegerData> IntegerDataPtr;
        struct Level {
            std::unordered_map<std::string, IntegerDataPtr> integers;
            std::unordered_set<std::string> nonIntegers;
            std::unordered_map<std::string, ParametricUsePtr> types;  // TODO: Rename class... VarContext?
            bool childrenCanMutate;
            bool poisonsAncestors;
        };

        std::vector<Level> levels;

        IntegerDataPtr findInteger(const std::string& name) const {
            for (auto lit = levels.rbegin(); lit != levels.rend(); lit++) {
                auto it = lit->integers.find(name);
                if (it != lit->integers.end()) return it->second;
                if (lit->nonIntegers.count(name)) return nullptr;
            }
            return nullptr;
        }

    public:
        IntegerContext() {
            // Outermost context is immutable
            enterImmutableLevel();
        }

        // Packages, modules
        void enterImmutableLevel() { levels.push_back({{}, {}, {}, false, false}); }
        // Functions, methods, begin/end blocks, for loops
        void enterMutableLevel() { levels.push_back({{}, {}, {}, true, false}); }
        // If/else, case
        void enterPoisoningLevel() { levels.push_back({{}, {}, {}, true, true}); }

        void exitLevel() { assert(levels.size() > 1); levels.pop_back(); }

        // Returns false on failure (variable already defined)
        bool defineVar(const std::string& name, bool isInteger) {
            Level& curLevel = levels.back();
            if (curLevel.nonIntegers.count(name)) return false;
            if (curLevel.integers.find(name) != curLevel.integers.end()) return false;
            if (isInteger) {
                auto idPtr = std::make_shared<IntegerData>();
                idPtr->state = INVALID;
                curLevel.integers[name] = idPtr;
            } else {
                curLevel.nonIntegers.insert(name);
            }
            return true;
        }

        bool isInteger(const std::string& name) const {
            return findInteger(name) != nullptr;
        }

        // Returns false if variable is not defined; caller still must check for validity of IntegerData
        bool get(const std::string& name, IntegerData& id) const {
            auto idPtr = findInteger(name);
            if (!idPtr) return false;
            id = *idPtr;
            return true;
        }

        // Returns false on failure (variable not defined or not mutable)
        bool set(const std::string& name, int64_t value) {
            Level* poisoningLevel = nullptr;
            IntegerDataPtr idPtr = nullptr;
            for (auto lit = levels.rbegin(); lit != levels.rend(); lit++) {
                if (lit != levels.rbegin() && !lit->childrenCanMutate) break;
                auto it = lit->integers.find(name);
                if (it != lit->integers.end()) {
                    idPtr = it->second;
                    break;
                }
                if (lit->nonIntegers.count(name)) break;
                // Capture outermost poisoning level
                if (lit->poisonsAncestors && !poisoningLevel)
                    poisoningLevel = &*lit;
            }
            if (!idPtr) return false;

            if (poisoningLevel) {
                idPtr->state = POISONED;
                idPtr = std::make_shared<IntegerData>();
                poisoningLevel->integers[name] = idPtr;
            }
            *idPtr = {VALID, value};
            return true;
        }

        // Handle type parametrics. These are simpler because we bind type params in limited cases
        void setType(const std::string& name, ParametricUsePtr pu) {
            assert(levels.size());
            levels.back().types[name] = pu;
        }

        bool getType(const std::string& name, ParametricUsePtr& pu) const {
            for (auto lit = levels.rbegin(); lit != levels.rend(); lit++) {
                auto it = lit->types.find(name);
                if (it != lit->types.end()) {
                    pu = it->second;
                    return true;
                }
            }
            return false;
        }
};

// Integer parsing facilities
bool isUnsizedLiteral(MinispecParser::IntLiteralContext *ctx) {
    auto s = ctx->getText();
    size_t quotePos = s.find("'");
    return quotePos == -1ul || quotePos == 0;
}

int64_t parseUnsizedLiteral(MinispecParser::IntLiteralContext *ctx) {
    assert(isUnsizedLiteral(ctx));
    auto s = ctx->getText();
    replace(s, "_", "");
    if (s.find("'") == -1ul) return std::stol(s); // decimal
    assert(s.size() >= 3);
    char base = s[1];
    auto num = s.substr(2);
    switch (base) {
        case 'd': return std::stol(num);
        case 'b': return std::stol(num, nullptr, 2);
        case 'h': return std::stol(num, nullptr, 16);
        default: panic("unsized int literal with unknown base, grammar must be outdated");
    }
}

// Helper for post-parse error messages
std::string quote(ParserRuleContext* ctx) {
    assert(ctx);
    std::string str = getTokenStream(ctx)->getText(ctx->getSourceInterval());
    replace(str, "\n",  "\\n");
    replace(str, "\t","\\r");
    replace(str, "\t","\\t");
    return errorColored("'" + str + "'");
}

// Elaboration (post-parse) errors
class SemanticError {
    public:
        virtual ParserRuleContext* getCtx() const { return nullptr; }
        virtual std::string str() const = 0;
};

class BasicError : public SemanticError {
    private:
        ParserRuleContext* ctx;
        std::string msg;

    public:
        BasicError(ParserRuleContext* ctx, const std::string& msg) : ctx(ctx), msg(msg) {}

        ParserRuleContext* getCtx() const override { return ctx; }

        virtual std::string str() const override {
            std::string errMsg = msg;
            replace(errMsg, "$CTX", quote(ctx));
            std::stringstream ss;
            ss << hlColored(getLoc(ctx) + ":")  << " " << errMsg << "\n";
            ss << contextStr(ctx, {ctx});
            return ss.str();
        }

        static Any create(ParserRuleContext* ctx, const std::string& msg) {
            return std::make_shared<BasicError>(ctx, msg);
        }

        friend class SubErrors;
        friend class ElabError;
};

typedef std::shared_ptr<BasicError> BasicErrorPtr;
class SubErrors;
typedef std::shared_ptr<SubErrors> SubErrorsPtr;

class SubErrors : public SemanticError {
    private:
        std::vector<BasicErrorPtr> errors;

    public:
        SubErrors() {}

        static Any create(Any val) {
            if (val.is<SubErrorsPtr>()) return val;
            if (val.is<BasicErrorPtr>()) return val;
            return nullptr;
        }

        static Any create(Any left, Any right) {
            SubErrorsPtr res = std::make_shared<SubErrors>();

            if (left.is<SubErrorsPtr>()) for (auto e : left.as<SubErrorsPtr>()->errors) res->errors.push_back(e);
            else if (left.is<BasicErrorPtr>()) res->errors.push_back(left.as<BasicErrorPtr>());

            if (right.is<SubErrorsPtr>()) for (auto e : right.as<SubErrorsPtr>()->errors) res->errors.push_back(e);
            else if (right.is<BasicErrorPtr>()) res->errors.push_back(right.as<BasicErrorPtr>());

            if (res->errors.size() == 0) return nullptr;
            else if (res->errors.size() == 1) return res->errors[0];
            else return res;
        }

        static SubErrorsPtr wrap(Any val) {
            if (val.is<SubErrorsPtr>()) return val;
            SubErrorsPtr res = std::make_shared<SubErrors>();
            if (val.is<BasicErrorPtr>()) res->errors.push_back(val.as<BasicErrorPtr>());
            return res;
        }

        virtual std::string str() const override {
            std::stringstream ss;
            for (auto e : errors) {
                std::string errMsg = e->msg;
                replace(errMsg, "$CTX", quote(e->ctx));
                ss << hlColored(getSubLoc(e->ctx) + ":")  << " " << errMsg << "\n";
            }
            return ss.str();
        }

        friend class ElabError;
};

class ElabError : public SemanticError {
    private:
        ParserRuleContext* ctx;
        SubErrorsPtr subErrors;
        const char* msg;
    public:
        ElabError(ParserRuleContext* ctx, Any exprVal, const char* msg = nullptr)
            : ctx(ctx), subErrors(SubErrors::wrap(exprVal)), msg(msg) {}

        ParserRuleContext* getCtx() const override { return ctx; }

        virtual std::string str() const override {
            std::stringstream ss;
            ss << hlColored(getLoc(ctx) + ":") << " " << errorColored("error:") << " ";
            ss << (msg? msg : "could not elaborate Integer expression") << "\n";
            ss << subErrors->str();

            std::vector<tree::ParseTree*> highlights;
            for (auto e : subErrors->errors) highlights.push_back(e->ctx);
            if (highlights.empty()) highlights.push_back(ctx);
            ss << contextStr(ctx, highlights);

            return ss.str();
        }
};

// Elaboration step control
struct ForElabStep {
    MinispecParser::ForStmtContext* ctx;
    int64_t indVar;
};
typedef std::variant<ParametricUse, ForElabStep> ElabStep;
static std::array<ElabStep, 16> elabStepBuf;
static uint64_t numElabSteps = 0;
static uint64_t maxElabSteps = 50000; // TODO: Make configurable
static uint64_t maxDepth = 1000; // TODO: Make configurable

void registerElabStep(ElabStep es, uint64_t depth = 0) {
    elabStepBuf[numElabSteps++ % elabStepBuf.size()] = es;
    bool error = false;
    // FIXME: Use error formatting helpers...
    if (maxElabSteps && numElabSteps > maxElabSteps) {
        error = true;
        std::cout << errorColored("error: ") << "exceeded maximum number of elaboration steps (" << maxElabSteps << "). The design may have a non-terminating loop or sequence of parametric functions, modules, or types. Fix the design to avoid non-termination, or increase the maximum number of elaboration steps if the design is correct.";
    } else if (maxDepth && depth > maxDepth) {
        error = true;
        std::cout << errorColored("error: ") << "exceeded maximum elaboration depth (" << maxDepth << "). The design may have a non-terminating recursion of parametric functions, modules, or types. Fix the design to avoid non-termination, or increase the maximum elaboration depth if the design is correct.";
    }
    if (error) {
        std::cout << "The last elaboration steps are:\n";
        for (size_t i = 0; i < std::min(elabStepBuf.size(), numElabSteps); i++) {
            auto elabStep = elabStepBuf[(numElabSteps - 1 - i) % elabStepBuf.size()];
            std::string stepStr;
            if (std::holds_alternative<ParametricUse>(elabStep)) {
                stepStr = std::get<ParametricUse>(elabStep).str(/*alreadyEscaped=*/true);
            } else {
                auto forElabStep = std::get<ForElabStep>(elabStep);
                std::stringstream ss;
                ss << "for loop at " << hlColored(getLoc(forElabStep.ctx));
                ss << ", iteration " << forElabStep.ctx->initVar->getText() << " = " << forElabStep.indVar;
                stepStr = ss.str();
            }
            info("    %12s: %s", hlColored(std::to_string(numElabSteps - i)).c_str(), stepStr.c_str());
        }
        exit(-1);
    }
}

// Keywords to check against. bsc checks against SystemVerilog keywords, but we'd get epic error messages if a BSV keyword was used as an identifier in Minispec...
const std::unordered_set<std::string> svKeywords = {"alias", "always", "always_comb", "always_ff", "always_latch", "and", "assert", "assert_strobe", "assign", "assume", "automatic", "before", "begin", "bind", "bins", "binsof", "break", "buf", "bufif0", "bufif1", "byte", "case", "casex", "casez", "cell", "chandle", "class", "clocking", "cmos", "config", "const", "constraint", "context", "continue", "cover", "covergroup", "coverpoint", "cross", "deassign", "default", "defparam", "design", "disable", "dist", "do", "edge", "else", "end", "endcase", "endclass", "endclocking", "endconfig", "endfunction", "endgenerate", "endgroup", "endinterface", "endmodule", "endpackage", "endprimitive", "endprogram", "endproperty", "endspecify", "endsequence", "endtable", "endtask", "enum", "event", "expect", "export", "extends", "extern", "final", "first_match", "for", "force", "foreach", "forever", "fork", "forkjoin", "function", "generate", "genvar", "highz0", "highz1", "if", "iff", "ifnone", "ignore_bins", "illegal_bins", "import", "incdir", "include", "initial", "inout", "input", "inside", "instance", "int", "integer", "interface", "intersect", "join", "join_any", "join_none", "large", "liblist", "library", "local", "localparam", "logic", "longint", "macromodule", "matches", "medium", "modport", "module", "nand", "negedge", "new", "nmos", "nor", "noshowcancelled", "not", "notif0", "notif1", "null", "or", "output", "package", "packed", "parameter", "pmos", "posedge", "primitive", "priority", "program", "property", "protected", "pull0", "pull1", "pulldown", "pullup", "pulsestyle_onevent", "pulsestyle_ondetect", "pure", "rand", "randc", "randcase", "randsequence", "rcmos", "real", "realtime", "ref", "reg", "release", "repeat", "return", "rnmos", "rpmos", "rtran", "rtranif0", "rtranif1", "scalared", "sequence", "shortint", "shortreal", "showcancelled", "signed", "small", "solve", "specify", "specparam", "static", "string", "strong0", "strong1", "struct", "super", "supply0", "supply1", "table", "tagged", "task", "this", "throughout", "time", "timeprecision", "timeunit", "tran", "tranif0", "tranif1", "tri", "tri0", "tri1", "triand", "trior", "trireg", "type", "typedef", "union", "unique", "unsigned", "use", "var", "vectored", "virtual", "void", "wait", "wait_order", "wand", "weak0", "weak1", "while", "wildcard", "wire", "with", "within", "wor", "xnor", "xor"};

const std::unordered_set<std::string> bsvKeywords = {"action", "endaction", "actionvalue", "endactionvalue", "ancestor", "deriving", "endinstance", "let", "match", "method", "endmethod", "par", "endpar", "powered_by", "provisos", "rule", "endrule", "rules", "endrules", "seq", "endseq", "schedule", "typeclass", "endtypeclass", "clock", "reset", "noreset", "no_reset", "valueof", "valueOf", "clocked_by", "reset_by", "default_clock", "default_reset", "output_clock", "output_reset", "input_clock", "input_reset", "same_family"};

class ElaboratorParseTreeWalker : public tree::ParseTreeWalker {
    public:
        virtual void walk(tree::ParseTreeListener* listener, tree::ParseTree* t) const override {
            // Stop the walk on nodes of certain types (the elaborator will
            // walk subtrees manually). This is needed when the translated code
            // doesn't follow the same structure as the original code.
            bool stop = dynamic_cast<MinispecParser::PackageDefContext*>(t) ||
                dynamic_cast<MinispecParser::ModuleDefContext*>(t) ||
                dynamic_cast<MinispecParser::ForStmtContext*>(t);
            if (stop) {
                enterRule(listener, t);
                exitRule(listener, t);
            } else {
                tree::ParseTreeWalker::walk(listener, t);
            }
        }
};

static const ElaboratorParseTreeWalker elaboratorWalker;

class Elaborator : public MinispecBaseListener {
    private:
        IntegerContext& ic;
        ParametricsMap& parametrics;
        const std::unordered_set<std::string>& localTypeNames;
        const ParametricUsePtr topLevelParametric;  // to elaborate function wrapper
        std::unordered_set<ParametricUse> parametricsEmitted;

        std::unordered_map<tree::ParseTree*, Any> elabValues;
        std::unordered_set<std::string> submoduleNames;

        void report(const SemanticError& error) {
            reportErr(error.str(), "", error.getCtx());
        }

        ParametricUsePtr createParametricUsePtr(const std::string& name, MinispecParser::ParamsContext* params) {
            auto res = std::make_shared<ParametricUse>();
            res->name = name;
            res->escape = islower(name[0]) || localTypeNames.count(name);
            if (params) {
                for (auto p : params->param()) {
                    if (p->intParam) {
                        Any val = getValue(p);
                        if (val.is<int64_t>()) {
                            res->params.push_back(val);
                        } else {
                            report(ElabError(p->intParam, res));
                        }
                    } else {
                        Any val = getValue(p);
                        if (val.is<ParametricUsePtr>()) {
                            res->params.push_back(val);
                        } else {
                            assert(val.isNull());
                            auto pu = createParametricUsePtr(p->type()->name->getText(), p->type()->params());
                            res->params.push_back(pu);
                        }
                    }
                }
            }
            return res;
        }

        // For ELABORATED paramFormals (so we can use the same types for parametric uses and emitted parametrics)
        ParametricUsePtr createParametricUsePtr(const std::string& name, MinispecParser::ParamFormalsContext* paramFormals) {
            auto res = std::make_shared<ParametricUse>();
            res->name = name;
            res->escape = islower(name[0]) || localTypeNames.count(name);
            if (paramFormals) {
                checkElaboratedParams(paramFormals);
                for (auto pf : paramFormals->paramFormal()) {
                    Any val = getValue(pf);
                    if (val.is<int64_t>() || val.is<ParametricUsePtr>()) {
                        res->params.push_back(val);
                    } else {
                        auto p = pf->param();
                        assert(p);
                        // FIXME: Dedup with above
                        if (p->intParam) {
                            Any val = getValue(p);
                            if (val.is<int64_t>()) {
                                res->params.push_back(val);
                            } else {
                                report(ElabError(p->intParam, res));
                            }
                        } else {
                            Any val = getValue(p);
                            if (val.is<ParametricUsePtr>()) {
                                res->params.push_back(val);
                            } else {
                                assert(val.isNull());
                                auto pu = createParametricUsePtr(p->type()->name->getText(), p->type()->params());
                                res->params.push_back(pu);
                            }
                        }
                    }
                }
            }
            return res;
        }

    public:
        Any getValue(tree::ParseTree* ctx) const {
            auto it = elabValues.find(ctx);
            return (it != elabValues.end())? it->second : Any(nullptr);
        }
    private:
        void setValue(tree::ParseTree* ctx, const Any& value) {
            if (value.isNull() && elabValues.find(ctx) == elabValues.end()) return;
            elabValues[ctx] = value;
        }

        int64_t getIntegerValue(MinispecParser::ExpressionContext* ctx) {
            assert(ctx);
            auto res = getValue(ctx);
            if (res.is<int64_t>()) {
                int64_t val = res.as<int64_t>();
                return val;
            } else {
                report(ElabError(ctx, res));
                return 42424242;  // doesn't matter, we'll error out (but give a dummy value to avoid reporting errors on uses of this variable)
            }
        }

        TranslatedCodePtr createTranslatedCodePtr(bool skipSpaces = false) {
            return std::make_shared<TranslatedCode>(
                    [&](tree::ParseTree* ctx) { return getValue(ctx); }, skipSpaces);
        }

        void checkElaboratedParams(ParserRuleContext* ctx) {
            class SubListener : public MinispecBaseListener {
                public:
                    Elaborator* parent;
                    virtual void enterEveryRule(ParserRuleContext* ctx) override {
                        auto expr = dynamic_cast<MinispecParser::ExpressionContext*>(ctx);
                        if (!expr) return;
                        auto res = parent->getValue(ctx);
                        if (!res.is<int64_t>()) parent->report(ElabError(ctx, res));
                    }
                    SubListener(Elaborator* parent) : parent(parent) {}
            };
            SubListener listener(this);
            elaboratorWalker.walk(&listener, ctx);
        }

        bool isConcrete(MinispecParser::ParamFormalsContext* ctx) {
            bool res = true;
            for (auto paramFormal : ctx->paramFormal()) {
                Any val = getValue(paramFormal);
                if ((paramFormal->intName && !val.is<int64_t>()) ||
                        (paramFormal->typeName && !val.is<ParametricUsePtr>())) {
                    res = false;
                    break;
                }
            }
            return res;
        }

    public:
        void clearValues(tree::ParseTree* tree) {
            auto it = elabValues.find(tree);
            if (it != elabValues.end()) elabValues.erase(it);

            auto ctx = dynamic_cast<ParserRuleContext*>(tree);
            if (ctx) for (auto child : ctx->children) clearValues(child);
        }

    public:
        // Context level control
        //void enterModuleDef(MinispecParser::ModuleDefContext* ctx) override { ic.enterImmutableLevel(); }
        void enterMethodDef(MinispecParser::MethodDefContext* ctx) override { ic.enterMutableLevel(); }
        void enterRuleDef(MinispecParser::RuleDefContext* ctx) override { ic.enterMutableLevel(); }
        void enterFunctionDef(MinispecParser::FunctionDefContext* ctx) override { ic.enterMutableLevel(); }
        void enterBeginEndBlock(MinispecParser::BeginEndBlockContext* ctx) override { ic.enterMutableLevel(); }
        void enterIfStmt(MinispecParser::IfStmtContext* ctx) override { ic.enterPoisoningLevel(); }
        void enterCaseStmt(MinispecParser::CaseStmtContext* ctx) override { ic.enterPoisoningLevel(); }
        void enterCaseExpr(MinispecParser::CaseExprContext* ctx) override { ic.enterPoisoningLevel(); } // TODO: Needed?

        //void exitModuleDef(MinispecParser::ModuleDefContext* ctx) override { ic.exitLevel(); }
        void exitMethodDef(MinispecParser::MethodDefContext* ctx) override { ic.exitLevel(); }
        void exitRuleDef(MinispecParser::RuleDefContext* ctx) override { ic.exitLevel(); }
        //void exitFunctionDef(MinispecParser::FunctionDefContext* ctx) override { ic.exitLevel(); }
        void exitBeginEndBlock(MinispecParser::BeginEndBlockContext* ctx) override { ic.exitLevel(); }
        //void exitIfStmt(MinispecParser::IfStmtContext* ctx) override { ic.exitLevel(); }
        void exitCaseStmt(MinispecParser::CaseStmtContext* ctx) override { ic.exitLevel(); }
        void exitCaseExpr(MinispecParser::CaseExprContext* ctx) override { ic.exitLevel(); } // TODO: Needed?

        // Catch all variable definitions (some of which include elaboration sites)
        void exitVarBinding(MinispecParser::VarBindingContext* ctx) override {
            assert(ctx->type());
            auto typeName = ctx->type()->name->getText();
            if (typeName == "Integer") {
                if (ctx->type()->params()) {
                    report(BasicError(ctx, "Integer type cannot have parameters"));
                }
                for (auto varInit : ctx->varInit()) {
                    auto varName = varInit->var->getText();
                    ic.defineVar(varName, true);
                    if (varInit->rhs) ic.set(varName, getIntegerValue(varInit->rhs));
                }
                setValue(ctx, Skip());
            } else {
                for (auto varInit : ctx->varInit()) {
                    auto varName = varInit->var->getText();
                    ic.defineVar(varName, false);
                }
            }
        }

        void exitLetBinding(MinispecParser::LetBindingContext* ctx) override {
            // Try to see if it's an Integer expression, and deduce the variable as Integer if so
            if (ctx->rhs) {
                Any value = getValue(ctx->rhs);
                if (value.is<int64_t>()) {
                    if (ctx->lowerCaseIdentifier().size() != 1) {
                        report(BasicError(ctx, "cannot assign an Integer value to multiple variables with unknown types"));
                    } else {
                        auto varName = ctx->lowerCaseIdentifier()[0]->getText();
                        ic.defineVar(varName, true);
                        ic.set(varName, value.as<int64_t>());
                        setValue(ctx, Skip());
                        return;
                    }
                }
            }
            // If this wasn't an Integer, define as non-Integer(s)
            for (auto var : ctx->lowerCaseIdentifier()) {
                ic.defineVar(var->getText(), false);
            }
        }

        void enterSubmoduleDecl(MinispecParser::SubmoduleDeclContext* ctx) override {
            ic.defineVar(ctx->name->getText(), false);
        }

        void enterArgFormal(MinispecParser::ArgFormalContext* ctx) override {
            ic.defineVar(ctx->argName->getText(), false);
        }

        // At elaboration time, paramFormals must be params or have their variables in Context for substitution
        void exitParamFormal(MinispecParser::ParamFormalContext* ctx) override {
            if (ctx->intName) {
                IntegerContext::IntegerData id;
                // NOTE: Variable can't be invalid/poisoned
                // because we set it when elaborating each instance
                if (ic.get(ctx->intName->getText(), id)) {
                    assert(id.state == IntegerContext::VALID);
                    setValue(ctx, Any(id.value));
                }
            } else if (ctx->typeName) {
                // TODO: Type substitution??
                //panic("type params not yet supported");
                ParametricUsePtr pu;
                bool res = ic.getType(ctx->typeName->getText(), pu);
                if (res) setValue(ctx, pu);
            } else {
                setValue(ctx, getValue(ctx->param()));
            }

        }

        void exitParam(MinispecParser::ParamContext* ctx) override {
            if (ctx->expression()) {
                setValue(ctx, getValue(ctx->expression()));
            } else if (ctx->type()) {
                setValue(ctx, getValue(ctx->type()));
            }
        }

        void exitParams(MinispecParser::ParamsContext* ctx) override {
            // All params should be elaborated at translation time
            checkElaboratedParams(ctx);
        }

        void exitArgFormal(MinispecParser::ArgFormalContext* ctx) override {
            if (ctx->type()->getText() == "Integer")
                report(BasicError(ctx->type(), "arguments cannot be of Integer type (use a parameter instead)"));
        }

        void exitVarAssign(MinispecParser::VarAssignContext* ctx) override {
            if (!ctx->var) return; // vars isn't Integer, as Integers cannot be bit-unpacked
            auto simpleLvalue = dynamic_cast<MinispecParser::SimpleLvalueContext*>(ctx->var);
            auto memberLvalue = dynamic_cast<MinispecParser::MemberLvalueContext*>(ctx->var);
            if (simpleLvalue) {
                auto varName = simpleLvalue->getText();
                if (ic.isInteger(varName)) {
                    ic.set(varName, getIntegerValue(ctx->expression()));
                    setValue(ctx, Skip());
                }
            } else if (memberLvalue) {
                auto base = dynamic_cast<MinispecParser::SimpleLvalueContext*>(memberLvalue->lvalue());
                if (base && submoduleNames.count(base->lowerCaseIdentifier()->getText())) {
                    TranslatedCodePtr tc = std::make_shared<TranslatedCode>(
                            [&](tree::ParseTree* ctx) { return getValue(ctx); });
                    tc->emitStart(ctx);
                    tc->emitStart(memberLvalue);
                    tc->emit(base, "." + memberLvalue->lowerCaseIdentifier()->getText() + "___input");
                    tc->emitEnd();
                    tc->emit("(", ctx->expression(), ");");
                    tc->emitEnd();
                    setValue(ctx, tc);
                }
            }
        }

        void exitVarExpr(MinispecParser::VarExprContext* ctx) override {
            if (!ctx->params()) {
                // Handle Integer elaboration
                IntegerContext::IntegerData integerData;
                auto varName = ctx->var->getText();
                Any res;
                if (varName == "True") {
                    res = true;
                } else if (varName == "False") {
                    res = false;
                } else {
                    bool found = ic.get(varName, integerData);
                    if (!found) {
                        res = BasicError::create(ctx->var, "$CTX is not an Integer variable");
                    } else if (integerData.state == IntegerContext::INVALID) {
                        res = BasicError::create(ctx->var, "Integer variable $CTX is uninitialized");
                    } else if (integerData.state == IntegerContext::POISONED) {
                        res = BasicError::create(ctx->var, "Integer variable $CTX is poisoned (it was set inside an if/else or case statement, so its value is unknown at compilation time)");
                    } else {
                        assert(integerData.state == IntegerContext::VALID);
                        res = integerData.value;
                    }
                }
                setValue(ctx, res);
            } else {
                // Handle parametric function calls
                checkElaboratedParams(ctx->params());
                setValue(ctx, createParametricUsePtr(ctx->var->getText(), ctx->params()));
                /*{   // For debug purposes only
                    auto tc = createTranslatedCodePtr();
                    tc->emit(ctx);
                    std::cout << "Got parametric function: |" << tc->getCode() << "|\n";
                }*/
            }
        }

        // Elaboration of control structures
        void exitIfStmt(MinispecParser::IfStmtContext* ctx) override {
            ic.exitLevel();  // was a poisoning level
            // If we know the condition at elab time, emit only the taken branch
            Any condValue = getValue(ctx->expression());
            if (condValue.is<bool>()) {
                bool cond = condValue.as<bool>();
                bool hasElse = ctx->stmt().size() == 2;
                auto tc = createTranslatedCodePtr();
                tc->emitStart(ctx);
                tc->emit(cond? "/* taken if */ " : hasElse? "/* taken else */ " : "/* non-taken if */ ");
                // if statements initiate a new lexical context, so enclose
                // statement in begin/end as we're removing if/else. This
                // fixes miscompilation of "if (x) let y = z;" and similar
                // (not really sensible, since the variable immediately goes
                // out of scope; but the BSC error is inscrutable)
                if (cond) tc->emit("begin ", ctx->stmt()[0], " end");
                else if (hasElse) tc->emit("begin ", ctx->stmt()[1], " end");
                tc->emitEnd();
                setValue(ctx, tc);
            }
        }

        void exitForStmt(MinispecParser::ForStmtContext* ctx) override {
            // Initial sanity checks
            if (ctx->type()->getText() != "Integer") {
                report(BasicError(ctx->type(), "induction variable must be an Integer"));
                return;
            }
            std::string varName = ctx->initVar->getText();
            if (ctx->updVar->getText() != varName) {
                report(BasicError(ctx->type(), "for loop must update (assign to the) induction variable, " + varName));
                return;
            }

            // NOTE: The loop's level is mutable, so we allow the body to
            // modify the induction variable. As long as it's a non-poisoning
            // modification, it's fine. If it poisons the induction variable,
            // we'll catch it on the termination check. The induction variable
            // might even be out of the loop...
            ic.enterMutableLevel();
            auto initExpr = ctx->expression()[0];
            auto condExpr = ctx->expression()[1];
            auto updateExpr = ctx->expression()[2];
            elaboratorWalker.walk(this, initExpr);
            Any indVar = getValue(initExpr);
            if (!indVar.is<int64_t>()) {
                report(ElabError(initExpr, indVar));
                ic.exitLevel();
                return;
            }
            ic.defineVar(varName, true);
            ic.set(varName, indVar.as<int64_t>());

            auto tc = createTranslatedCodePtr();
            tc->emitStart(ctx);
            tc->emit("/* for loop */");
            while (true) {
                clearValues(condExpr);
                elaboratorWalker.walk(this, condExpr);
                Any condVar = getValue(condExpr);
                if (!condVar.is<bool>()) {
                    report(ElabError(condExpr, indVar, "could not elaborate Boolean expression (make sure this is a comparison involving only Integers)"));
                    ic.exitLevel();
                    return;
                }
                if (!condVar.as<bool>()) {
                    tc->emitEnd();
                    setValue(ctx, tc);
                    ic.exitLevel();
                    return;
                }

                registerElabStep(ForElabStep({ctx, indVar.as<int64_t>()}));
                clearValues(ctx->stmt());
                elaboratorWalker.walk(this, ctx->stmt());
                tc->emitStart(ctx->stmt());
                tc->emit("begin ", ctx->stmt(), " end");
                tc->emitLine();
                tc->emitEnd("for loop in " + hlColored(getLoc(ctx)) +
                        ", iteration with " + noteColored(varName +
                            " = " + std::to_string(indVar.as<int64_t>())));

                clearValues(updateExpr);
                elaboratorWalker.walk(this, updateExpr);
                indVar = getValue(updateExpr);
                if (!indVar.is<int64_t>()) {
                    report(ElabError(updateExpr, indVar));
                    ic.exitLevel();
                    return;
                }
                ic.set(varName, indVar.as<int64_t>());
            }
        }

        // Bottom-up integer expression elaboration
        void exitIntLiteral(MinispecParser::IntLiteralContext* ctx) override {
            if (isUnsizedLiteral(ctx)) {
                setValue(ctx, parseUnsizedLiteral(ctx));
            }
        }

        void exitBinopExpr(MinispecParser::BinopExprContext* ctx) override {
            if (ctx->unopExpr()) {
                setValue(ctx, getValue(ctx->unopExpr()));
                return;
            }
            std::string op = ctx->op->getText();
            Any left = getValue(ctx->left);
            Any right = getValue(ctx->right);
            Any res;
            if (left.is<int64_t>() && right.is<int64_t>()) {
                int64_t l = left.as<int64_t>();
                int64_t r = right.as<int64_t>();
                if (op == "+") res = l + r;
                else if (op == "-") res = l - r;
                else if (op == "*") res = l * r;
                else if (op == "/") res = r? (l / r) : 0;
                else if (op == "%") res = r? (l % r) : 0;
                else if (op == "**") {
                    int64_t e = 1;
                    while (r-- > 0) e *= l;
                    res = e;
                }
                else if (op == "<<") res = l << r;
                else if (op == ">>") res = l >> r;
                // Bitwise logical
                else if (op == "&") res = l & r;
                else if (op == "|") res = l | r;
                else if (op == "^") res = l ^ r;
                else if (op == "^~" || op == "~^") res = ~l ^ r;  // what we negate doesn't matter
                else if (op == "<") res = (bool) (l < r);
                else if (op == "<=") res = (bool) (l <= r);
                else if (op == ">") res = (bool) (l > r);
                else if (op == ">=") res = (bool) (l >= r);
                else if (op == "==") res = (bool) (l == r);
                else if (op == "!=") res = (bool) (l != r);
                else res = BasicError::create(ctx, errorColored(op) + " is not a valid operator for Integer values");
            } else if (left.is<bool>() && right.is<bool>()) {
                int64_t l = left.as<bool>();
                int64_t r = right.as<bool>();
                if (op == "&&") res = (bool)(l && r);
                else if (op == "||") res = (bool)(l || r);
                else res = BasicError::create(ctx, errorColored(op) + " is not a valid operator for Bool values");
            } else if (left.is<int64_t>() && right.is<bool>()) {
                res = BasicError::create(ctx, "operands have values of incompatible types (Integer and Bool)");
            } else if (left.is<int64_t>() && right.is<bool>()) {
                res = BasicError::create(ctx, "operands have values of incompatible types (Bool and Integer)");
            } else {
                res = SubErrors::create(left, right);
            }
            setValue(ctx, res);
        }

        void exitUnopExpr(MinispecParser::UnopExprContext* ctx) override {
            if (!ctx->op) {
                setValue(ctx, getValue(ctx->exprPrimary()));
                return;
            }
            auto xorReduce = [](int64_t v) -> int64_t { return __builtin_parityl(v); };
            std::string op = ctx->op->getText();
            Any value = getValue(ctx->exprPrimary());
            Any res;
            if (value.is<int64_t>()) {
                int64_t v = value.as<int64_t>();
                if (op == "~") res = ~v;
                else if (op == "&") res = (v == -1)? 1 : 0;
                else if (op == "~&") res = (v == -1)? 0 : 1;
                else if (op == "|") res = (v == 0)? 0 : 1;
                else if (op == "~|") res = (v == 0)? 1 : 0;
                else if (op == "^") res = xorReduce(v);
                else if (op == "^~" || op == "~^") res = (xorReduce(v) == 0)? 1 : 0;
                else if (op == "+") res = v;
                else if (op == "-") res = -v;
                else res = BasicError::create(ctx, errorColored(op) + " is not a valid unary operator for an Integer value");
                setValue(ctx, res);
            } else if (value.is<bool>()) {
                bool v = value.as<bool>();
                if (op == "!") res = (bool)!v;
                else res = BasicError::create(ctx, errorColored(op) + " is not a valid unary operator for a Bool value");
            } else {
                // Propagate error, if any
                res = value;
            }
            setValue(ctx, res);
        }

        void exitCondExpr(MinispecParser::CondExprContext *ctx) override {
            Any predValue = getValue(ctx->pred);
            Any res;
            if (predValue.is<bool>()) {
                auto takenCtx = ctx->expression()[predValue.as<bool>()? 1 : 2];
                Any takenValue = getValue(takenCtx);
                if (takenValue.is<int64_t>() || takenValue.is<bool>()) {
                    // Use elaborated value directly
                    res = takenValue;
                } else {
                    auto tc = createTranslatedCodePtr();
                    tc->emitStart(ctx);
                    tc->emit("(", takenCtx, ")");
                    tc->emitEnd();
                    res = tc;
                }
            } else if (predValue.is<int64_t>()) {
                res = BasicError::create(ctx->pred, "$CTX has type Integer, should be Bool");
            } else {
                // NOTE: This does not catch elaboration errors on the
                // non-taken branch of the conditional. This is by design.
                res = SubErrors::create(predValue, getValue(ctx->expression()[1]));
                res = SubErrors::create(res, getValue(ctx->expression()[2]));
            }
            setValue(ctx, res);
        }

        void exitCaseExprItem(MinispecParser::CaseExprItemContext* ctx) override {
            // bsc does not parse compound expressions correctly in caseExpr,
            // so wrap them all in parentheses
            // NOTE: We're modifying ctx->body's value, rather then ctx, which
            // is unusual. This works fine even if ctx->body is elaborated (ie
            // non-null getValue()).  See TranslatedCode::emit().
            auto tc = createTranslatedCodePtr();
            tc->emitStart(ctx->body);
            tc->emit("(", ctx->body, ")");
            tc->emitEnd();
            setValue(ctx->body, tc);
        }

        // Propagate value
        void exitParenExpr(MinispecParser::ParenExprContext *ctx) override {
            setValue(ctx, getValue(ctx->expression()));
        }

        void exitOperatorExpr(MinispecParser::OperatorExprContext *ctx) override {
            setValue(ctx, getValue(ctx->binopExpr()));
        }

        void exitCallExpr(MinispecParser::CallExprContext *ctx) override {
            if (ctx->fcn->getText() == "log2" && ctx->expression().size() == 1) {
                Any v = getValue(ctx->expression()[0]);
                Any res;
                if (v.is<int64_t>()) {
                    int64_t val = v.as<int64_t>();
                    res = (int64_t) ((val > 0)? (63 - __builtin_clzl(val)) : 0);
                } else if (v.isNull() || v.is<bool>()) {
                    res = BasicError::create(ctx, "log2() requires an Integer expression as an argument");
                } else {
                    res = v;  // propagate error
                }
                setValue(ctx, res);
            }
        }

        // Module elaboration
        void enterModuleDef(MinispecParser::ModuleDefContext* ctx) override {
            ic.enterImmutableLevel();
            // Elaborate paramFormals, if they exist
            elaboratorWalker.walk(this, ctx->moduleId());
            if (ctx->argFormals()) elaboratorWalker.walk(this, ctx->argFormals());
            // Elaborate module elements in the right order
            submoduleNames.clear();

            for (auto stmt : ctx->moduleStmt()) {
                if (stmt->inputDef() || stmt->submoduleDecl() || stmt->stmt()) {
                    elaboratorWalker.walk(this, stmt);
                }
                if (auto s = stmt->submoduleDecl()) submoduleNames.insert(s->name->getText());
                // FIXME: This does not handle submodule variable assignments
                // (e.g., from an argument, such as "Counter c = cArg;"), and
                // handles submodule varDecls (e.g., "Counter c; c = cArg;")
                // only because submoduleDecl subsumes varDecl. Unfortunately,
                // the lack of type inference makes this hard. We could
                // specialize assignments, but we'd need to infer the RHS type
                // for letBinding varAssigns to work.
                //
                // Since we don't need stmt(), a simpler solution may be to
                // completely disallow module statements. But this is also
                // inconvenient, because we sometimes use Integers in module
                // contexts...
            }
            // Include argFormals in submodules...
            if (ctx->argFormals()) {
                for (auto s : ctx->argFormals()->argFormal()) {
                    submoduleNames.insert(s->argName->getText());
                }
            }
            for (auto stmt : ctx->moduleStmt()) {
                // NOTE: Rules are emitted before methods, but for elaboration
                // we don't care about their order, b/c they're independent
                if (stmt->ruleDef() || stmt->methodDef()) {
                    elaboratorWalker.walk(this, stmt);
                }
            }
            ic.exitLevel();

            // Emit
            auto tc = createTranslatedCodePtr();
            tc->emitStart(ctx);

            // First, emit the interface
            tc->emitLine("interface ", ctx->moduleId(), ";");
            for (auto stmt : ctx->moduleStmt()) {
                if (auto m = stmt->methodDef()) {
                    tc->emitLine("  method ", m->type(), m->name, "", m->argFormals(), ";");
                } else if (auto i = stmt->inputDef()) {
                    tc->emitLine("  method Action ", i->name, "___input(", i->type(), " value);");
                }
            }
            tc->emitLine("endinterface\n");

            // Emit interface and module as separate entities. bsc reports some
            // errors (e.g., conflicting declarations) at the beginning of the
            // module rather than the name. This way, we can catch the exact
            // location.
            tc->emitEnd();
            tc->emitStart(ctx);

            // Then, emit the module, following standard BSV conventions for naming
            if (ctx->moduleId()->paramFormals()) {
                auto pu = getValue(ctx->moduleId()).as<ParametricUsePtr>();
                assert(pu);
                tc->emit("module \\mk", pu->str(/*alreadyEscaped=*/true), " ");
            } else {
                tc->emit("module mk", ctx->moduleId());
            }
            if (ctx->argFormals()) tc->emit("#", ctx->argFormals());
            tc->emitLine("(", ctx->moduleId(), ");");

            // Emit in order required by bsv: submodules/input wires, then rules, then methods
            auto moduleName = [this](MinispecParser::TypeContext* modTypeCtx) {
                auto tc = createTranslatedCodePtr();
                tc->emit(modTypeCtx);
                std::string typeName = tc->getSourceMap().getCode();
                if (typeName.find("\\") == 0) return "\\mk" + typeName.substr(1);
                else return "mk" + typeName.substr(0, typeName.find("#"));
            };
            for (auto stmt : ctx->moduleStmt()) {
                tc->emitStart(stmt);
                if (auto i = stmt->inputDef()) {
                    if (i->defaultVal) {
                        tc->emitLine("  Wire#(", i->type(), ") ", i->name, " <- mkDWire(", i->defaultVal, ");");
                    } else {
                        tc->emitLine("  Wire#(", i->type(), ") ", i->name, " <- mkBypassWire;");
                    }
                } else if (auto s = stmt->submoduleDecl()) {
                    // HACK for Vector initialization
                    if (s->type()->name->getText() == "Vector") {
                        auto params = s->type()->params();
                        if (!params) {
                            report(BasicError(s->type(), "Vector must use parameters"));
                        } else {
                            auto paramVec = params->param();
                            if (paramVec.size() != 2) {
                                report(BasicError(s->type(), "Vector must use 2 parameters"));
                            } else {
                                auto elemType = paramVec[1]->type();
                                if (!elemType) {
                                    report(BasicError(paramVec[1], "Vector's second parameter must be a type"));
                                } else {
                                    tc->emitLine("  ", s->type(), s->name, " <- replicateM(", moduleName(elemType), "", s->args(), ");");
                                }
                            }
                        }
                    } else {
                        tc->emitLine("  ", s->type(), s->name, " <- ", moduleName(s->type()), s->args(), ";");
                    }
                } else if (auto x = stmt->stmt()) {
                    tc->emitLine("  ", x);
                }
                tc->emitEnd();
            }

            for (auto stmt : ctx->moduleStmt()) {
                tc->emitStart(stmt);
                if (auto r = stmt->ruleDef()) {
                    // Ensure all rules fire every cycle
                    tc->emitLine("  (* no_implicit_conditions, fire_when_enabled *) ", r);
                }
                tc->emitEnd();
            }

            for (auto stmt : ctx->moduleStmt()) {
                tc->emitStart(stmt);
                if (auto m = stmt->methodDef()) {
                    tc->emitLine("  ", m);
                } else if (auto i = stmt->inputDef()) {
                    tc->emitLine("  method Action ", i->name, "___input(", i->type(), " value);");
                    tc->emitLine("    ", i->name, " <= value;");
                    tc->emitLine("  endmethod");
                }
                tc->emitEnd();
            }
            tc->emitLine("endmodule\n");
            tc->emitEnd();
            setValue(ctx, tc);

            if (topLevelParametric &&
                topLevelParametric->name == ctx->moduleId()->name->getText() &&
                ctx->argFormals() && !ctx->argFormals()->argFormal().empty()) {
                report(BasicError(ctx->argFormals(), "top-level module " +
                        quote(ctx->moduleId()->name) + " cannot have arguments"));
            }
        }

        void exitFunctionDef(MinispecParser::FunctionDefContext* ctx) override {
            auto pu = createParametricUsePtr(ctx->functionId()->name->getText(), ctx->functionId()->paramFormals());
            if (topLevelParametric && *topLevelParametric == *pu) {
                // Emit synthesis wrapper
                std::string ifcName = ctx->functionId()->name->getText() + "___";
                ifcName[0] = std::toupper(ifcName[0]);
                std::string modName = "mk" + ctx->functionId()->name->getText();
                auto ifcPu = createParametricUsePtr(ifcName, ctx->functionId()->paramFormals());
                auto modPu = createParametricUsePtr(modName, ctx->functionId()->paramFormals());
                ifcPu->escape = true;  // not recognized as a local type, but it is, we're making it up now

                auto tc = createTranslatedCodePtr();
                tc->emitStart(ctx);
                tc->emit(ctx);
                tc->emitLine();
                tc->emitLine();
                tc->emitLine("interface ", ifcPu->str(), " ;");
                tc->emitLine("  (* prefix=\"_\", result = \"out\" *)");
                tc->emitLine("  method ", ctx->type(), " fn", ctx->argFormals(), ";");
                tc->emitLine("endinterface\n");
                tc->emitLine("module ", modPu->str(), " ( ", ifcPu->str(), " );");
                tc->emit("  method ", ctx->type(), " fn", ctx->argFormals(), " = ", pu->str(), " (");
                if (ctx->argFormals()) {
                    auto afVec = ctx->argFormals()->argFormal();
                    for (size_t i = 0; i < afVec.size(); i++) {
                        tc->emit(afVec[i]->argName);
                        if ((i+1) < afVec.size()) tc->emit(", ");
                    }
                }
                tc->emitLine(");");
                tc->emitLine("endmodule");
                tc->emitEnd();
                setValue(ctx, tc);
            }
            ic.exitLevel();
        }

        void exitFunctionId(MinispecParser::FunctionIdContext* ctx) override {
            if (ctx->paramFormals()) {
                auto pu = createParametricUsePtr(ctx->name->getText(), ctx->paramFormals());
                parametricsEmitted.insert(*pu);
                setValue(ctx, pu);
            }
        }

        void exitTypeId(MinispecParser::TypeIdContext* ctx) override {
            if (ctx->paramFormals()) {
                auto pu = createParametricUsePtr(ctx->name->getText(), ctx->paramFormals());
                parametricsEmitted.insert(*pu);
                setValue(ctx, pu);
            }
        }

        void exitModuleId(MinispecParser::ModuleIdContext* ctx) override {
            if (ctx->paramFormals()) {
                auto pu = createParametricUsePtr(ctx->name->getText(), ctx->paramFormals());
                parametricsEmitted.insert(*pu);
                setValue(ctx, pu);
            }
        }

        void exitType(MinispecParser::TypeContext* ctx) override {
            ParametricUsePtr formalPu;
            if (ic.getType(ctx->name->getText(), formalPu)) {
                if (!ctx->params()) {
                    setValue(ctx, formalPu);
                } else {
                    // Curry params, i.e., given type T with T = Vector#(4),
                    // T#(Reg#(Bit#(8)) will elab to Vector#(4, Reg#(Bit#(8)))
                    auto pu = createParametricUsePtr("", ctx->params());
                    auto mergedParams = formalPu->params;
                    mergedParams.insert(mergedParams.end(), pu->params.begin(), pu->params.end());
                    pu->name = formalPu->name;
                    pu->escape = formalPu->escape;
                    pu->params = mergedParams;
                    /// std::cout << "XXX " << ctx->name->getText() << "params=" << pu->str() <<  "\n";
                    setValue(ctx, pu);
                }
            } else if (localTypeNames.count(ctx->name->getText()) && ctx->params()) {
                checkElaboratedParams(ctx->params());
                setValue(ctx, createParametricUsePtr(ctx->name->getText(), ctx->params()));
                /*{   // For debug purposes only
                    auto tc = createTranslatedCodePtr();
                    tc->emit(ctx);
                    std::cout << "Got parametric type: |" << tc->getCode() << "|\n";
                }*/
            }
        }

        // Auto-deriving
        void exitTypeDefEnum(MinispecParser::TypeDefEnumContext* ctx) override {
            setValue(ctx->children.back() /*;*/, " deriving(Bits, Eq, FShow);");
        }
        void exitTypeDefStruct(MinispecParser::TypeDefStructContext* ctx) override {
            setValue(ctx->children.back() /*;*/, " deriving(Bits, Eq, FShow);");
        }

        // Imports
        void exitImportDecl(MinispecParser::ImportDeclContext* ctx) override {
            setValue(ctx, Skip());
        }
        void exitBsvImportDecl(MinispecParser::BsvImportDeclContext* ctx) override {
            auto tc = createTranslatedCodePtr();
            tc->emitStart(ctx);
            for (auto id : ctx->upperCaseIdentifier()) {
                tc->emitLine("import ", id, "::*;");
            }
            tc->emitEnd();
            setValue(ctx, tc);
        }

        // Forbid some identifiers to avoid conflicts
        void exitLowerCaseIdentifier(MinispecParser::LowerCaseIdentifierContext* ctx) override {
            auto id = ctx->getText();
            auto err = [&](std::string e) {
                report(BasicError(ctx, "lowercase identifier " + quote(ctx) + 
                            " " + e + ", which is forbidden"));
            };
 
            if (id.find("mk") == 0) err("begins with " + hlColored("'mk'"));
            if (id.find("___input") != -1ul) err("contains " + hlColored("'___input'"));
            if (svKeywords.count(id)) err("is a SystemVerilog keyword");
            if (bsvKeywords.count(id)) err("is a Bluespec (BSV) keyword");
        }

        void exitPackageDef(MinispecParser::PackageDefContext* ctx) override {
            for (auto stmt : ctx->packageStmt()) {
                // Detect and skip non-concrete parametrics
                MinispecParser::ParamFormalsContext* paramFormals = nullptr;
                ParserRuleContext* defCtx = nullptr;
                std::string name;
                if (stmt->functionDef()) {
                    auto functionId = stmt->functionDef()->functionId();
                    paramFormals = functionId->paramFormals();
                    name = functionId->name->getText();
                    defCtx = stmt->functionDef();
                } else if (stmt->moduleDef()) {
                    auto moduleId = stmt->moduleDef()->moduleId();
                    paramFormals = moduleId->paramFormals();
                    name = moduleId->name->getText();
                    defCtx = stmt->moduleDef();
                } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefSynonym()) {
                    auto typeId = stmt->typeDecl()->typeDefSynonym()->typeId();
                    paramFormals = typeId->paramFormals();
                    name = typeId->name->getText();
                    defCtx = stmt->typeDecl()->typeDefSynonym();
                } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefStruct()) {
                    auto typeId = stmt->typeDecl()->typeDefStruct()->typeId();
                    paramFormals = typeId->paramFormals();
                    name = typeId->name->getText();
                    defCtx = stmt->typeDecl()->typeDefStruct();
                }

                if (paramFormals) {
                    elaboratorWalker.walk(this, paramFormals);
                    if (isConcrete(paramFormals)) {
                        elaboratorWalker.walk(this, stmt);
                    } else {
                        parametrics[name] = std::make_tuple(defCtx, this);
                        setValue(stmt, Skip());
                    }
                } else {
                    elaboratorWalker.walk(this, stmt);
                }
            }
            setValue(ctx->EOF(), Skip());
        }

        Elaborator(IntegerContext* integerContext, ParametricsMap* parametrics, const std::unordered_set<std::string>* localTypeNames, ParametricUsePtr topLevelParametric) :
            ic(*integerContext), parametrics(*parametrics), localTypeNames(*localTypeNames), topLevelParametric(topLevelParametric) {}

        bool isParametricEmitted(const ParametricUse& p) const { return parametricsEmitted.count(p); }
};

static ParametricUsePtr createTopLevelParametricUsePtr(const std::string& name, MinispecParser::ParamsContext* params, const std::string& errHdr) {
    auto res = std::make_shared<ParametricUse>();
    res->name = name;
    res->escape = false;

    // We can only take literals, but the grammar allows expressions,
    // so we need to go dooown the hierarchy. This returns nullptr at
    // any point where the traversal fails, no point in giving more info
    auto intParamToIntLiteral = [](MinispecParser::ExpressionContext* intParamCtx) -> MinispecParser::IntLiteralContext* {
        auto opCtx = dynamic_cast<MinispecParser::OperatorExprContext*>(intParamCtx);
        if (!opCtx) return nullptr;
        auto binopCtx = opCtx->binopExpr();
        if (!binopCtx) return nullptr;
        auto unopCtx = binopCtx->unopExpr();
        if (!unopCtx) return nullptr;
        auto primCtx = unopCtx->exprPrimary();
        if (!primCtx) return nullptr;
        return dynamic_cast<MinispecParser::IntLiteralContext*>(primCtx);
    };

    if (params) {
        for (auto p : params->param()) {
            if (p->intParam) {
                auto ipStr = p->intParam->getText();
                auto litCtx = intParamToIntLiteral(p->intParam);
                if (!litCtx) error("%s", (errHdr + errorColored("'" + ipStr + "'") + " is not an integer literal").c_str());
                if (!isUnsizedLiteral(litCtx)) error("%s", (errHdr + errorColored("'" + ipStr + "'") + " is a sized integer literal (must be unsized)").c_str());
                res->params.push_back(parseUnsizedLiteral(litCtx));
            } else {
                auto pu = createTopLevelParametricUsePtr(p->type()->name->getText(), p->type()->params(), errHdr);
                res->params.push_back(pu);
            }
        }
    }
    return res;
}

static ParametricUsePtr validateTopLevel(const std::string& topLevel) {
    if (topLevel == "") return nullptr;
    std::string errHdr = "invalid top-level argument " +
        errorColored("'" + topLevel + "'") + ": ";
    try {
        ANTLRInputStream input(topLevel);
        MinispecLexer lexer(&input);
        CommonTokenStream tokenStream(&lexer);
        MinispecParser parser(&tokenStream);
        parser.setErrorHandler(std::make_shared<BailErrorStrategy>());
        auto topLevelExpr = dynamic_cast<MinispecParser::VarExprContext*>(parser.exprPrimary());
        if (!topLevelExpr) error("%s", (errHdr + "not a module or function id").c_str());
        return createTopLevelParametricUsePtr(topLevelExpr->var->getText(),
                topLevelExpr->params(), errHdr);
    } catch (ParseCancellationException& p) {
        error("%s", (errHdr + "not a module or function id").c_str());
    }
}

const char MinispecPrelude[] = {
  #include "MinispecPrelude.inc"  // Auto-generated
  , 0x00  // NULL-terminate
};
std::string getPrelude() {
    std::stringstream prelude;
    prelude << "// Produced by msc, version " << getVersion() << "\n\n" << MinispecPrelude;
    return prelude.str();
}

SourceMap translateFiles(const std::vector<MinispecParser::PackageDefContext*> parsedTrees, const std::string& topLevel) {
    // Initial validation of topLevel arg
    auto topLevelParametric = validateTopLevel(topLevel);

    // Do an initial pass to capture all type and module names. This advance visibility
    // is needed because we need to know whether a parametric type use maps to
    // a Minispec type or to a Bluespec type (it changes the emitted code)
    std::unordered_set<std::string> localTypeNames;
    for (auto tree : parsedTrees) {
        for (auto stmt : tree->packageStmt()) {
            if (stmt->moduleDef()) {
                localTypeNames.insert(stmt->moduleDef()->moduleId()->name->getText());
            } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefSynonym()) {
                auto typeId = stmt->typeDecl()->typeDefSynonym()->typeId();
                localTypeNames.insert(typeId->name->getText());
            } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefEnum()) {
                localTypeNames.insert(stmt->typeDecl()->typeDefEnum()->upperCaseIdentifier()->getText());
            } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefStruct()) {
                auto typeId = stmt->typeDecl()->typeDefStruct()->typeId();
                localTypeNames.insert(typeId->name->getText());
            }
        }
    }

    ParametricsMap parametrics;
    IntegerContext integerContext;
    Elaborator elab(&integerContext, &parametrics, &localTypeNames, topLevelParametric);
    TranslatedCode tc([&elab](tree::ParseTree* ctx) { return elab.getValue(ctx); });

    // Emit all non-parametrics (or fully elaborated parametrics)
    tc.emit(getPrelude());
    for (auto tree : parsedTrees) {
        elaboratorWalker.walk(&elab, tree);
        tc.emit(tree);
        // Ensure there's a newline between files even if the emmitted file
        // doesn't end with a newline
        tc.emitLine();
    }

    // Emit parametrics
    uint64_t elabDepth = 0;
    while (true) {
        elabDepth++;
        auto paramUses = tc.dequeueParametricUsesEmitted();
        if (elabDepth == 1 && topLevelParametric && !topLevelParametric->params.empty()) {
            paramUses.push_back(std::make_tuple(*topLevelParametric, nullptr));
        }
        if (paramUses.empty()) break;  // no more parametrics

        for (auto& [p, emitCtx] : paramUses) {
            auto it = parametrics.find(p.name);
            // NOTE: Fail silently so we can use parametric uses for non-local parametric types
            if (it == parametrics.end()) continue; //error(parametric %s not found", p.name.c_str());

            auto ctx = std::get<0>(it->second);
            if (elab.isParametricEmitted(p)) continue;

            registerElabStep(p, elabDepth);
            std::vector<MinispecParser::ParamFormalContext*> paramFormals;
            std::string paramType;
            if (auto funcCtx = dynamic_cast<MinispecParser::FunctionDefContext*>(ctx)) {
                paramFormals = funcCtx->functionId()->paramFormals()->paramFormal();
                paramType = "function";
            } else if (auto modCtx = dynamic_cast<MinispecParser::ModuleDefContext*>(ctx)) {
                paramFormals = modCtx->moduleId()->paramFormals()->paramFormal();
                paramType = "module";
            } else if (auto typedefCtx = dynamic_cast<MinispecParser::TypeDefSynonymContext*>(ctx)) {
                paramFormals = typedefCtx->typeId()->paramFormals()->paramFormal();
                paramType = "typedef";
            } else if (auto structCtx = dynamic_cast<MinispecParser::TypeDefStructContext*>(ctx)) {
                paramFormals = structCtx->typeId()->paramFormals()->paramFormal();
                paramType = "struct";
            } else {
                panic("unhandled parametric... did the grammar change? (%s)", p.name.c_str());
            }

            // Produce paramFormals string (we don't use getText() to avoid
            // comments within paramFormals and have  our own whitespace rules)
            assert(paramFormals.size());
            std::stringstream paramFormalsSs;
            for (uint32_t i = 0; i < paramFormals.size(); i++) {
                if (i > 0) paramFormalsSs << ", ";
                auto pf = paramFormals[i];
                if (pf->intName) paramFormalsSs << "Integer " << pf->intName->getText();
                else if (pf->typeName) paramFormalsSs << "type " << pf->typeName->getText();
                else paramFormalsSs << pf->getText();  // it's a param
            }
            std::string defStr = p.name + "#(" + paramFormalsSs.str() + ")";

            auto paramsErr = [&](const std::string& msg) {
                std::stringstream ss;
                std::string loc = emitCtx? getLoc(emitCtx) : "command-line arg";
                ss << hlColored(loc + ":") << " "
                    << errorColored(" error:") << " cannot instantiate "
                    << errorColored("'" + p.str(true) + "'")
                    << " from parametric " << paramType << " "
                    << hlColored(defStr) << " defined at "
                    << hlColored(getLoc(ctx)) << ": " << msg << "\n";
                if (emitCtx) ss << contextStr(emitCtx);
                reportErr(ss.str(), "", emitCtx);
            };

            // Bind params, produce params string
            integerContext.enterImmutableLevel();
            std::stringstream paramsSs;
            if (p.params.size() != paramFormals.size()) {
                paramsErr(std::to_string(paramFormals.size())
                        + " parameter" + ((paramFormals.size() > 1)? "s" : "")
                        + " required, " + std::to_string(p.params.size())
                        + " given" );
                continue;
            }
            bool paramMatchError = false;
            for (uint32_t i = 0; i < paramFormals.size(); i++) {
                auto paramFormal = paramFormals[i];
                if (i > 0) paramsSs << ", ";
                if (paramFormal->intName) {
                    if (!p.params[i].is<int64_t>()) {
                        paramsErr("parameter " + std::to_string(i + 1) + " is not an Integer");
                        paramMatchError = true;
                        continue;
                    }
                    auto varName = paramFormal->intName->getText();
                    integerContext.defineVar(varName, true);
                    integerContext.set(varName, p.params[i].as<int64_t>());
                    paramsSs << varName << " = " << p.params[i].as<int64_t>();
                } else if (paramFormal->typeName) {
                    if (!p.params[i].is<ParametricUsePtr>()) {
                        paramsErr("parameter " + std::to_string(i + 1) + " is not a type");
                        paramMatchError = true;
                        continue;
                    }
                    auto typeName = paramFormal->typeName->getText();
                    integerContext.setType(typeName, p.params[i].as<ParametricUsePtr>());
                    paramsSs << typeName << " = " << p.params[i].as<ParametricUsePtr>()->str(/*alreadyEscaped=*/true);
                } else {
                    paramsErr("partially specialized parametrics not yet allowed");
                }
            }
            if (paramMatchError) continue;

            std::string paramInfo = paramType  + " " + hlColored(defStr) +
                " with " + noteColored(paramsSs.str());

            elab.clearValues(ctx);
            elaboratorWalker.walk(&elab, ctx);
            integerContext.exitLevel();
            tc.emitStart(ctx);
            tc.emitLine();
            tc.emitLine(ctx);
            tc.emitEnd(paramInfo);
        }
    }
    
    std::string topModule = "";
    if (topLevelParametric) topModule = "mk" + topLevelParametric->str();

    // Top-level parametric modules with names containing #() break both bsc
    // -sim (the generated C++ files have the unescaped raw name all over) and
    // produce invalid Verilog output. So produce a wrapper module.
    if (topLevelParametric && !topLevelParametric->params.empty()) {
        if (!elab.isParametricEmitted(*topLevelParametric)) {
            std::string msg = errorColored("error:") + " cannot find top-level parametric " +
                errorColored("'" + topLevelParametric->str() + "'");
            reportErr(msg, "", nullptr);
        }
        
        ParametricUse ifcPu = *topLevelParametric;
        if (!isupper(ifcPu.name[0])) {
            ifcPu.name[0] = toupper(ifcPu.name[0]);
            ifcPu.name += "___";
        }
        tc.emitLine("\n// Top-level wrapper module");
        tc.emitLine("module mkTopLevel___( \\", ifcPu.str(), " );");
        tc.emitLine("  \\", ifcPu.str(), " res <- \\mk", topLevelParametric->str(), " ;");
        tc.emitLine("  return res;");
        tc.emitLine("endmodule");
        topModule = "mkTopLevel___";
    }

    exitIfErrors();
    return tc.getSourceMap(topModule);
}
