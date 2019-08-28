#pragma once

#include <string>
#include "antlr4-runtime.h"

// Reporting of errors in user code (**not** errors in the compiler itself)
void initReporting(bool reportAllErrors);

void reportMsg(bool isError, const std::string& msg,
        const std::string locInfo = "", antlr4::tree::ParseTree* ctx = nullptr);

void reportErr(const std::string& msg, const std::string locInfo = "",
        antlr4::tree::ParseTree* ctx = nullptr);

void reportWarn(const std::string& msg, const std::string locInfo = "",
        antlr4::tree::ParseTree* ctx = nullptr);

void exitIfErrors();

// Error locations
std::string getLoc(antlr4::tree::ParseTree* pt);
std::string getSubLoc(antlr4::tree::ParseTree* pt);
