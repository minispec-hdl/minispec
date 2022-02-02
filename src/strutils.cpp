/** $lic$
 * Copyright (C) 2019-2022 by Daniel Sanchez
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

#include <string>
#include <regex>
#include "strutils.h"


// Colorized output (chosen to be gcc-like)
static const char* errorColorCode = "\x1B[1;31m";
static const char* warnColorCode = "\x1B[1;35m";
static const char* noteColorCode = "\x1B[1;34m";
static const char* fixColorCode = "\x1B[32m";
static const char* hlColorCode = "\x1B[1;37m";
static const char* clearCode = "\033[0m";

static std::string colorize(const char* colorCode, const std::string& str) {
    return colorCode + str + clearCode;
}

std::string errorColored(const std::string& str) { return colorize(errorColorCode, str); }
std::string warnColored(const std::string& str) { return colorize(warnColorCode, str); }
std::string noteColored(const std::string& str) { return colorize(noteColorCode, str); }
std::string fixColored(const std::string& str) { return colorize(fixColorCode, str); }
std::string hlColored(const std::string& str) { return colorize(hlColorCode, str); }


// String manipulation
void replace(std::string& s, const std::string& sub, const std::string& repl) {
    bool replHasSub = (repl.find(sub) != std::string::npos);
    auto pos = s.find(sub);
    while (pos != std::string::npos) {
        s.replace(pos, sub.size(), repl);
        // If the replacement string has sub, skip it to avoid infinite loops.
        // Otherwise, match from the current location (including repl).
        pos = s.find(sub, replHasSub? pos + repl.size() : pos);
    }
}

std::string trim(const std::string& s) {
    // https://stackoverflow.com/a/21815483
    return std::regex_replace(s, std::regex("^ +| +$|( ) +"), "$1");
}
