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
