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
