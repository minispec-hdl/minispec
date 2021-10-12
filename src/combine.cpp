/** $lic$
 * Copyright (C) 2019-2021 by Daniel Sanchez
 *
 * This file is part of the Minispec compiler and toolset.
 *
 * Minispec is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * Minispec is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* Combine multiple minispec files into a single file, meant to be used to
 * represent all previous cells in a Minispec Jupyter notebook.
 *
 * This tool is to be used by the Minispec Jupyter kernel, and does minimal
 * error checking because the files it processes have already been compiled
 * (for the most part).
 *
 * minispec-combine takes a list of files as inputs. It outputs the contents of
 * all files **except the last one** to stdout, with their globals potentially
 * RENAMED to avoid naming conflicts. This lets us implement Jupyter-style
 * history and semantics while preserving Minispec name-clash rules.
 *
 * Renaming is done ONCE per file and global and affects all uses of the
 * renamed global until its next def. Non-redefined globals are not renamed.
 * For example, minispec-combine In1.ms In2.ms In3.ms In4.ms, with:
 *
 * In1.ms: Integer i = 1;
 * In2.ms: Integer j = i + 1;
 * In3.ms: Bool i = True;
 * In4.ms: function Bool j = i;
 *
 * Will output:
 *
 * Integer i___In1 = 1;
 * Integer j___In2 = i___In1 + 1;
 * Bool i = True;
 * // function Bool j = i; not emitted since it's the last file, but renames j
 *
 * This style of renaming seeks to KEEP PREVIOUSLY-WORKING CODE WORKING. Note
 * how the i is redef'd to be a Bool and j to be a function, and yet the
 * intervening uses of the old values still work fine.
 *
 * The alternative would have been to redefine Integer i, but this has many
 * corner cases: it would require typechecking to not break existing code and
 * may run into circular defs (e.g., In3: Integer i = j + 1).
 *
 * The drawback of the current approach is that some behavior becomes
 * confusing. For example:
 *
 * In1: function Bool bar = True;
 * In2: function Bool foo = !bar;
 * In3: function Bool bar = False;
 *
 * In this case, minispec-combine In1.ms In2.ms In3.ms will use the OLD bar
 * def; however, minispec-combine In2.ms In1.ms In3.ms will use the NEW bar
 * def, and will in fact always use the lates because bar was not defined
 * before foo in this case.
 *
 * To retain sane behavior around parameterics, every def renames ALL PREVIOUS
 * parametrics. For example,
 *
 * In1: function Bit#(i) foo#(Integer i) = ...;
 *      function Bit#(1) foo#(1) = ...;
 * In2: function Bit#(2) foo#(2) = ...;
 *
 * In this case, foo#(2) will rename foo#(Integer i) and foo#(1) to foo___In1.
 * This makes things simple, but requites that a single file/cell contains ALL
 * DEFS (parametric and instances) of a parametric.
 *
 * TODO: Emit warnings on confusing behaviors above (ooo defs + partial
 * parametrics)
 */

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "antlr4-runtime.h"
#include "MinispecBaseListener.h"

#include "log.h"
#include "parse.h"
#include "strutils.h"

using namespace antlr4;
using misc::Interval;

class RenameTable {
    private:
        // Each rename element has the renamed identifier and the **file** where the def takes place
        typedef std::tuple<std::string, std::string> RenameElem;
        typedef std::deque<RenameElem> RenameQueue;
        std::unordered_map<std::string, RenameQueue> renameTable;

    public:
        RenameTable(const std::vector<MinispecParser::PackageDefContext*>& parseTrees) {
            for (auto tree : parseTrees) {
                for (auto stmt : tree->packageStmt()) {
                    std::vector<std::string> names;
                    if (stmt->functionDef()) {
                        auto name = stmt->functionDef()->functionId()->name->getText();
                        names.push_back(name);
                    } if (stmt->moduleDef()) {
                        auto name = stmt->moduleDef()->moduleId()->name->getText();
                        names.push_back(name);
                    } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefSynonym()) {
                        auto typeId = stmt->typeDecl()->typeDefSynonym()->typeId();
                        auto name = typeId->name->getText();
                        names.push_back(name);
                    } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefEnum()) {
                        auto typeDefEnum = stmt->typeDecl()->typeDefEnum();
                        names.push_back(typeDefEnum->upperCaseIdentifier()->getText());
                        for (auto elem : typeDefEnum->typeDefEnumElement()) {
                            names.push_back(elem->tag->getText());
                        }
                    } else if (stmt->typeDecl() && stmt->typeDecl()->typeDefStruct()) {
                        auto typeId = stmt->typeDecl()->typeDefStruct()->typeId();
                        auto name = typeId->name->getText();
                        names.push_back(name);
                    } else if (stmt->varDecl()) {
                        auto lb = dynamic_cast<MinispecParser::LetBindingContext*>(stmt->varDecl());
                        auto vb = dynamic_cast<MinispecParser::VarBindingContext*>(stmt->varDecl());
                        if (lb) {
                            for (auto var : lb->lowerCaseIdentifier()) {
                                names.push_back(var->getText());
                            }
                        } else if (vb) {
                            for (auto varInit : vb->varInit()) {
                                names.push_back(varInit->var->getText());
                            }
                        }
                    }

                    for (auto name : names) {
                        std::string fileName = getTokenStream(stmt)->getSourceName();
                        if (renameTable.find(name) == renameTable.end()) {
                            renameTable[name] = {std::make_tuple(name, fileName)};
                        } else {
                            auto& [prevName, prevFileName] = renameTable[name].back();
                            assert(prevName == name);
                            if (prevFileName != fileName) {  // only one rename per file
                                std::string suffix = "___" + prevFileName;
                                replace(suffix, ".ms", "");  // not safe in general, but these files are always named InXXX.ms
                                renameTable[name].back() = std::make_tuple(name + suffix, prevFileName);
				renameTable[name].push_back(std::make_tuple(name, fileName));
                            }
                        }
                    }
                }
            }
        }

        void advance(MinispecParser::PackageDefContext* tree) {
            std::string fileName = getTokenStream(tree)->getSourceName();
            for (auto& it : renameTable) {
		auto& rq = it.second;
                if (rq.size() > 1) {
                    auto& [nextName, nextFileName] = rq[1];
                    if (nextFileName == fileName) {
                        // The time for this name has come
                        rq.pop_front();
                    }
                }
            }
        }

        std::string rename(const std::string& name) const {
            auto it = renameTable.find(name);
            if (it == renameTable.end()) return name;
            return std::get<0>(it->second.front());
        }
};

class LocalVars {
    private:
        std::vector<std::unordered_set<std::string>> levelDefs;

    public:
        void enterLevel() { levelDefs.push_back({}); }
        void exitLevel() { assert(!levelDefs.empty()); levelDefs.pop_back(); }
        void define(const std::string& var) {
            if (!levelDefs.empty()) levelDefs.back().insert(var);
        }
        bool isDefined(const std::string& name) {
            for (auto& l : levelDefs) if (l.count(name)) return true;
            return false;
        }
};

class RenameListener : public MinispecBaseListener {
    private:
        const RenameTable& rt;
        LocalVars lv;
	std::unordered_map<tree::ParseTree*, std::string> names;

        void walk(tree::ParseTree* parseTree) {
            if (!parseTree) return;
            tree::ParseTreeWalker::DEFAULT.walk(this, parseTree);
        }

    public:
	RenameListener(const RenameTable& rt) : rt(rt) {}

        // Context level control
        //void enterTypeDefSynonym(MinispecParser::TypeDefSynonymContext* ctx) override { lv.enterLevel(); }
        //void enterTypeDefStruct(MinispecParser::TypeDefStructContext* ctx) override { lv.enterLevel(); }
        //void enterFunctionDef(MinispecParser::FunctionDefContext* ctx) override { lv.enterLevel(); }
        //void enterModuleDef(MinispecParser::ModuleDefContext* ctx) override { lv.enterLevel(); }
        void enterMethodDef(MinispecParser::MethodDefContext* ctx) override { lv.enterLevel(); }
        void enterRuleDef(MinispecParser::RuleDefContext* ctx) override { lv.enterLevel(); }
        void enterBeginEndBlock(MinispecParser::BeginEndBlockContext* ctx) override { lv.enterLevel(); }
        void enterIfStmt(MinispecParser::IfStmtContext* ctx) override { lv.enterLevel(); }
        void enterCaseStmt(MinispecParser::CaseStmtContext* ctx) override { lv.enterLevel(); }
        //void enterForStmt(MinispecParser::ForStmtContext* ctx) override { lv.enterLevel(); }

        void exitTypeDefSynonym(MinispecParser::TypeDefSynonymContext* ctx) override { lv.exitLevel(); }
        void exitTypeDefStruct(MinispecParser::TypeDefStructContext* ctx) override { lv.exitLevel(); }
        void exitFunctionDef(MinispecParser::FunctionDefContext* ctx) override { lv.exitLevel(); }
        void exitModuleDef(MinispecParser::ModuleDefContext* ctx) override { lv.exitLevel(); }
        void exitMethodDef(MinispecParser::MethodDefContext* ctx) override { lv.exitLevel(); }
        void exitRuleDef(MinispecParser::RuleDefContext* ctx) override { lv.exitLevel(); }
        void exitBeginEndBlock(MinispecParser::BeginEndBlockContext* ctx) override { lv.exitLevel(); }
        void exitIfStmt(MinispecParser::IfStmtContext* ctx) override { lv.exitLevel(); }
        void exitCaseStmt(MinispecParser::CaseStmtContext* ctx) override { lv.exitLevel(); }
        void exitForStmt(MinispecParser::ForStmtContext* ctx) override { lv.exitLevel(); }

        // Defining locals
        void enterVarBinding(MinispecParser::VarBindingContext* ctx) override {
            for (auto varInit : ctx->varInit()) {
                lv.define(varInit->var->getText());
            }
        }

        void enterLetBinding(MinispecParser::LetBindingContext* ctx) override {
            for (auto var : ctx->lowerCaseIdentifier()) {
                lv.define(var->getText());
            }
        }

        void enterParamFormal(MinispecParser::ParamFormalContext* ctx) override {
            if (ctx->intName) lv.define(ctx->intName->getText());
            else if (ctx->typeName) lv.define(ctx->typeName->getText());
        }

        void enterArgFormal(MinispecParser::ArgFormalContext* ctx) override {
            lv.define(ctx->argName->getText());
        }

        void enterSubmoduleDecl(MinispecParser::SubmoduleDeclContext* ctx) override {
            lv.define(ctx->name->getText());
        }

        void enterInputDef(MinispecParser::InputDefContext* ctx) override {
            lv.define(ctx->name->getText());
        }

        void enterForStmt(MinispecParser::ForStmtContext* ctx) override {
            lv.enterLevel();
            lv.define(ctx->initVar->getText());
        }

        // Parametrics --- elaborate paramFormals FIRST
        void enterTypeDefSynonym(MinispecParser::TypeDefSynonymContext* ctx) override {
            lv.enterLevel();
            walk(ctx->typeId()->paramFormals());
        }

        void enterTypeDefStruct(MinispecParser::TypeDefStructContext* ctx) override {
            lv.enterLevel();
            walk(ctx->typeId()->paramFormals());
        }

        void enterFunctionDef(MinispecParser::FunctionDefContext* ctx) override {
            lv.enterLevel();
            walk(ctx->functionId()->paramFormals());
        }

        void enterModuleDef(MinispecParser::ModuleDefContext* ctx) override {
            lv.enterLevel();
            walk(ctx->moduleId()->paramFormals());

            // Just in case, elaborate all the inputs, submodules, and stmts
            // before methods and rules. This way, if a method/rule use a local
            // or input defined later (which is legal Minispec b/c msc emits
            // things in the right order) we'll avoid renaming the local
            for (auto stmt : ctx->moduleStmt()) {
                if (stmt->inputDef() || stmt->submoduleDecl() || stmt->stmt()) {
                    walk(stmt);
                }
            }
        }

        // Renaming
        virtual void enterLowerCaseIdentifier(MinispecParser::LowerCaseIdentifierContext* ctx) override {
            // Not all lowerCaseIdentifiers are renameable; only cases are:
            //  1. varDecls ( = varInits + letBindings)
            //  2. functionIds
            //  3. varExprs (which may be function calls)
            // All other cases (struct declarations, memberBinds, fields, etc.) should not be renamed.
            bool isRenameable = false;
            if (ctx->parent) {
                auto p = ctx->parent;
                if (dynamic_cast<MinispecParser::VarInitContext*>(p)) isRenameable = true;
                else if (dynamic_cast<MinispecParser::LetBindingContext*>(p)) isRenameable = true;
                else if (dynamic_cast<MinispecParser::FunctionIdContext*>(p)) isRenameable = true;
                else if (dynamic_cast<MinispecParser::AnyIdentifierContext*>(p))
                    isRenameable = dynamic_cast<MinispecParser::VarExprContext*>(p->parent);
            }
            if (!isRenameable) return;

            std::string name = ctx->getText();
            if (!lv.isDefined(name)) {
                std::string newName = rt.rename(name);
                if (newName != name) names[ctx] = newName;
            }
        }

        virtual void enterUpperCaseIdentifier(MinispecParser::UpperCaseIdentifierContext* ctx) override {
            // Most upperCaseIds are renameable, only those on import statements are not
            bool isRenameable = true;
            if (ctx->parent) {
                auto p = ctx->parent;
                if (dynamic_cast<MinispecParser::BsvImportDeclContext*>(p)) isRenameable = false;
                else if (dynamic_cast<MinispecParser::IdentifierContext*>(p))
                    isRenameable = dynamic_cast<MinispecParser::ImportDeclContext*>(p->parent) == nullptr;
            }
            if (!isRenameable) return;

            std::string name = ctx->getText();
            if (!lv.isDefined(name)) {
                std::string newName = rt.rename(name);
                if (newName != name) names[ctx] = newName;
            }
        }

        void emit(tree::ParseTree* ctx) {
            if (!ctx) return;
            auto it = names.find(ctx);
            if (it != names.end()) {
                std::cout << it->second;
                return;
            }

            ParserRuleContext* prCtx = dynamic_cast<ParserRuleContext*>(ctx);
            if (prCtx) {
                auto tokenStream = getTokenStream(prCtx);
                for (uint32_t i = 0; i < prCtx->children.size(); i++) {
                    // Print inter-ctx whitespace
                    if (i > 0) {
                        Interval prev = prCtx->children[i-1]->getSourceInterval();
                        Interval cur = prCtx->children[i]->getSourceInterval();
                        if (prev.b + 1 < cur.a) {
                            std::cout << tokenStream->getText(Interval(prev.b + 1, cur.a -1));
                        }
                    }
                    emit(ctx->children[i]);
                }
            } else {
                std::string s = ctx->getText();
                if (s == "<EOF>") s = "\n";
                std::cout << s;
            }
        }
};

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cerr << "error: need some files!\n";
        exit(-1);
    }

    std::vector<MinispecParser::PackageDefContext*> parseTrees;
    for (int i = 1; i < argc; i++) {
        parseTrees.push_back(parseSingleFile(argv[i]));
    }

    RenameTable renameTable(parseTrees);
    RenameListener renameListener(renameTable);
    for (auto parseTree : parseTrees) {
        if (parseTree == parseTrees.back()) continue;  // skip last file

        renameTable.advance(parseTree);
        tree::ParseTreeWalker::DEFAULT.walk(&renameListener, parseTree);

        std::string fileName = getTokenStream(parseTree)->getSourceName();
        std::cout << "// File " << fileName << "\n";
        renameListener.emit(parseTree);
    }

    return 0;
}
