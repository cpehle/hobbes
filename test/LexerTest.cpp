#include "hobbes/Lex/Lexer.h"
#include "test.H"

struct LexTestCase {
  LexTestCase(std::string input, std::vector<hobbes::Token> output)
      : input(input), output(output) {}
  std::string input;
  std::vector<hobbes::tok::TokenKind> output;
};

std::vector<LexTestCase> test_cases = {
    LexTestCase("square int test",
  {hobbes::tok::idententifier, hobbes::tok::identifier, hobbes::tok::identifier}),
};

TEST(Lexer, Ident) {
  for (auto t : test_cases) {
    auto lexer = hobbes::Lexer(&t.input[0], &t.input[0], &t.end()[0]);
    hobbes::Token tok;
    
  }
}

