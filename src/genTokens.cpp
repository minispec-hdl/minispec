/** $lic$
 * Copyright (C) 2019-2024 by Daniel Sanchez
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

#include <algorithm>
#include <cctype>
#include <iostream>
#include <filesystem>
#include <regex>
#include <sys/stat.h>
#include <unordered_set>
#include "antlr4-runtime.h"
#include "MinispecLexer.h"
#include "MinispecParser.h"
#include "MinispecBaseListener.h"

using namespace antlr4;
using antlrcpp::Any;
using misc::Interval;
using std::string;
using std::stringstream;

class CaptureTokensErrorStrategy : public DefaultErrorStrategy {
    public:
        misc::IntervalSet expectedTokens;
    protected:
        virtual void reportInputMismatch(Parser *recognizer, const InputMismatchException &e) override {
            expectedTokens = e.getExpectedTokens();
            throw ParseCancellationException();
        }
};

misc::IntervalSet getExpectedTokensForError(const std::string& program) {
    ANTLRInputStream input(program);
    MinispecLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    auto errorStrategyPtr = std::make_shared<CaptureTokensErrorStrategy>();
    try {
        MinispecParser parser(&tokens);
        parser.setErrorHandler(errorStrategyPtr);
        parser.packageDef();
    } catch (ParseCancellationException& p) {}
    return errorStrategyPtr->expectedTokens;
}

std::string setToString(const std::set<ssize_t>& set) {
    std::stringstream ss;
    bool first = true;
    for (auto elem : set) {
        if (first) { ss << elem; first = false; }
        else { ss << ", " << elem;}
    }
    return ss.str();
}

int main(int argc, const char* argv[]) {
    std::string missingStmt = "function X f;\n if (a)\nendfunction\n";
    std::string missingExpr = "function X f;\n let a = ;\nendfunction\n";
    std::cout << "const misc::IntervalSet stmtTokens(-1 /* not used */, " << setToString(getExpectedTokensForError(missingStmt).toSet()) << ");\n";
    std::cout << "const misc::IntervalSet exprTokens(-1 /* not used */, " << setToString(getExpectedTokensForError(missingExpr).toSet()) << ");\n";
    return 0;
}
