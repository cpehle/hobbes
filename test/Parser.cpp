#include "hobbes/Lex/Parser.h"
#include "hobbes/Lex/Lexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/FileSystemOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "test.H"

TEST(Parser, Import) {
  std::string test = "import a.b.c";
  auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
  auto parser = hobbes::Parser(lexer);
  parser.ParseImportStatement();
}

TEST(Parser, Module) {
  std::string test = R"(
module maybe where

import test

)";

  auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
  auto parser = hobbes::Parser(lexer);
  parser.ParseModule();
}
namespace {
std::vector<std::string> match_tests = {
    "match 1 2 with | 1 2  -> 2 | 2 1 -> 1",
    //  "match 1 2 3 4 with | 1 2 3 4 -> 1 | _ _ _ _ -> 2",
};

std::vector<std::string> let_tests = {
    "let x = 1 in x", "let x = 1; y = 2; in y", "let x = 1; y = 2 in y",
};

std::vector<std::string> if_tests = {
    "if 0 then 1 else 2",
};

std::vector<std::string> primary_tests = {
    "1123", "12.0", "identifier", "id(a,b,c)",
};

std::vector<std::string> binary_expr_tests = {
    "1 + 2 * 3",
    "2 * 3 * 4",
};
}

TEST(Parser, Primary) {
  for (auto test : primary_tests) {
    auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
    auto parser = hobbes::Parser(lexer);
    parser.ConsumeToken(); // prime the parser with the first token
    auto Exp = parser.ParsePrimaryExpression();
  }
}

TEST(Parser, If) {
  for (auto test : if_tests) {
    auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
    auto parser = hobbes::Parser(lexer);
    parser.ConsumeToken();
    auto Exp = parser.ParseIfExpr();
  }
}

TEST(Parser, Let) {
  for (auto test : let_tests) {
    auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
    auto parser = hobbes::Parser(lexer);
    parser.ConsumeToken();
    auto Exp = parser.ParseLetExpr();
  }
}

/*
TEST(Parser, Match) {
  for (auto test : match_tests) {
    auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
    auto parser = hobbes::Parser(lexer);
    parser.ConsumeToken();
    auto Exp = parser.ParseMatchExpr();
  }
}
*/

TEST(Parser, BinaryExpr) {
  for (auto test : binary_expr_tests) {
    auto lexer = hobbes::Lexer(&test[0], &test[0], &test.end()[0]);
    auto parser = hobbes::Parser(lexer);
    parser.ConsumeToken();
    auto Exp = parser.ParseExpression();
  }
}

TEST(Parser, Class) {
  std::vector<std::string> tests = {
      "class constraints => id names",
      "class constraints => id names | fundeps",
      "class constraints => id names | fundeps where class_members",
      "class constraints => id names where class_members",
      "class id names",
      "class id names | fundeps",
      "class id names where class_members",
      "class id names | fundeps where class_members"};
}

TEST(Parser, Expr) {}
