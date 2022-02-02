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
#include <map>
#include <sstream>
#include "antlr4-runtime.h"
#include "MinispecParser.h"

// Stores the translated Bluespec source as well as the map to the Minispec
// source syntax elements that produced each piece of Bluespec code
class SourceMap {
    private:
        typedef std::tuple<ssize_t, ssize_t> Range;
        const std::map<Range, antlr4::tree::ParseTree*> dstToSrc;
        const std::map<Range, std::string> dstToInfo;
        const std::string code;
        const std::string topModule;
        std::vector<size_t> lineToPos;

        SourceMap(const std::map<Range, antlr4::tree::ParseTree*>& dstToSrc,
                  const std::map<Range, std::string>& dstToInfo,
                  const std::string& code, const std::string& topModule) :
            dstToSrc(dstToSrc), dstToInfo(dstToInfo), code(code), topModule(topModule)
        {
            lineToPos.push_back(0);
            for (size_t p = 0; p < code.size(); p++) {
                if (code[p] == '\n') lineToPos.push_back(p + 1);
            }
        }

        size_t getPos(size_t line, size_t lineChar) const {
            assert(line <= lineToPos.size());
            assert(line > 0);
            assert(lineChar > 0);
            return lineToPos[line - 1] + (lineChar - 1);
        }

        friend class TranslatedCode;  // for private constructor

    public:
        // Find source element for this output position
	antlr4::tree::ParseTree* find(size_t line, size_t lineChar) const {
            size_t pos = getPos(line, lineChar);
            Range range = std::make_tuple(pos, pos);
            auto it = dstToSrc.lower_bound(range);
            if (it == dstToSrc.end()) return nullptr;
            auto [foundStart, foundEnd] = it->first;
            if (foundStart != (ssize_t)pos) return nullptr;
            return it->second;
        }

        // Find exact source element match for output text
	antlr4::tree::ParseTree* find(size_t line, size_t lineChar, std::string_view sv) const {
            size_t pos = getPos(line, lineChar);
            Range range = std::make_tuple(pos, pos + sv.size());
            auto it = dstToSrc.find(range);
            if (it == dstToSrc.end()) return nullptr;
            if (getCode().substr(pos, sv.size()) != sv) return nullptr;
            return it->second;
        }

        std::string getContextInfo(size_t line, size_t lineChar) const {
            // Include all context info, outside-in.
            // NOTE: There are faster implementation, but this one is simple
            // and fast enough (used only on errors, few infos, etc.)
            size_t pos = getPos(line, lineChar);
            std::stringstream ss;
            for (auto& [range, info] : dstToInfo) {
                auto& [start, end] = range;
                if (start <= (ssize_t)pos && end >= (ssize_t)pos) {
                    ss << "In " << info << "\n";
                }
                if (start > (ssize_t)pos) break;  // nothing useful beyond...
            }
            return ss.str();
        }

        const std::string& getCode() const { return code; }
        const std::string& getTopModule() const { return topModule; }
};

void setElabLimits(uint64_t maxSteps, uint64_t maxDepth);

SourceMap translateFiles(const std::vector<MinispecParser::PackageDefContext*>& parsedTrees, const std::string& topLevel);
