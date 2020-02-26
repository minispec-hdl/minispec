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

// String coloring
std::string errorColored(const std::string& str);
std::string warnColored(const std::string& str);
std::string noteColored(const std::string& str);
std::string fixColored(const std::string& str);
std::string hlColored(const std::string& str);

// String manipulation
void replace(std::string& s, const std::string& sub, const std::string& repl);
std::string trim(const std::string& s);
