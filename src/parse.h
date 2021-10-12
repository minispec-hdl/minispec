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

#pragma once
#include <vector>
#include "antlr4-runtime.h"
#include "MinispecParser.h"

// Parses file and all imported files. Returns parse trees sorted in
// topological order. Exits on lexer or parser errors
std::vector<MinispecParser::PackageDefContext*> parseFileAndImports(const std::string& fileName, const std::vector<std::string>& path);

// Parse a single file without following imports. Returns file's parse tree.
MinispecParser::PackageDefContext* parseSingleFile(const std::string& fileName);

antlr4::TokenStream* getTokenStream(antlr4::ParserRuleContext* ctx);

// Prints the error context for an error associated with ctx
std::string contextStr(antlr4::tree::ParseTree* ctx, std::vector<antlr4::tree::ParseTree*> highlights = {});
