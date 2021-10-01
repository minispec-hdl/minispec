/** $lic$
 * Copyright (C) 2019-2020 by Daniel Sanchez
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

#include <string>
#include "antlr4-runtime.h"

// Reporting of errors in user code (**not** errors in the compiler itself)
void initReporting(bool reportAllErrors);

void reportMsg(bool isError, const std::string& msg,
        const std::string& locInfo = "", antlr4::tree::ParseTree* ctx = nullptr);

void reportErr(const std::string& msg, const std::string& locInfo = "",
        antlr4::tree::ParseTree* ctx = nullptr);

void reportWarn(const std::string& msg, const std::string& locInfo = "",
        antlr4::tree::ParseTree* ctx = nullptr);

void exitIfErrors();

// Error locations
std::string getLoc(antlr4::tree::ParseTree* pt);
std::string getSubLoc(antlr4::tree::ParseTree* pt);
