#include "hobbes/Lex/Lexer.h"
#include "test.H"

struct LexTestCase {
  LexTestCase(std::string input, std::vector<hobbes::tok::TokenKind> output)
      : input(input), output(output) {}
  std::string input;
  std::vector<hobbes::tok::TokenKind> output;
};

std::vector<LexTestCase> test_cases = {
    LexTestCase("square int test",
                {hobbes::tok::identifier, hobbes::tok::identifier,
                 hobbes::tok::identifier}),

    LexTestCase("[](){}", {hobbes::tok::l_square, hobbes::tok::r_square,
                           hobbes::tok::l_paren, hobbes::tok::r_paren,
                           hobbes::tok::l_brace, hobbes::tok::r_brace}),
    LexTestCase(";,|",
                {hobbes::tok::semi, hobbes::tok::comma, hobbes::tok::pipe}),
    LexTestCase("=> = == ===",
                {hobbes::tok::doublearrow, hobbes::tok::equal,
                 hobbes::tok::equalequal, hobbes::tok::equalequalequal}),
    LexTestCase("+ ++", {hobbes::tok::plus, hobbes::tok::plusplus}),
    LexTestCase("< <= <-", {hobbes::tok::less, hobbes::tok::lessequal,
                            hobbes::tok::assign}),
    LexTestCase("// Hello World.\n square", {hobbes::tok::identifier}),
    LexTestCase("a /* This is a test */ +",
                {hobbes::tok::identifier, hobbes::tok::plus}),
    LexTestCase("> >=", {hobbes::tok::greater, hobbes::tok::greaterequal}),
    LexTestCase(":= :: :", {hobbes::tok::colonequal, hobbes::tok::coloncolon,
                            hobbes::tok::colon}),
    LexTestCase(". ..", {hobbes::tok::period, hobbes::tok::upto}),
    LexTestCase("- ->", {hobbes::tok::minus, hobbes::tok::arrow}),
    LexTestCase("! !=", {hobbes::tok::tnot, hobbes::tok::notequal}),
    LexTestCase("^ ? $ @ ~ % * \\ ` '",
                {hobbes::tok::caret, hobbes::tok::question, hobbes::tok::dollar,
                 hobbes::tok::at, hobbes::tok::tilde, hobbes::tok::percent,
                 hobbes::tok::star, hobbes::tok::fun, hobbes::tok::equote,
                 hobbes::tok::squote}),
    LexTestCase("match where import type",
                {hobbes::tok::kw_match, hobbes::tok::kw_where,
                 hobbes::tok::kw_import, hobbes::tok::kw_type}),
    LexTestCase("module where import type data class instance exists not let "
                "case default match matches parse with of and or if then else "
                "true false in pack unpack do return",
                {hobbes::tok::kw_module,   hobbes::tok::kw_where,
                 hobbes::tok::kw_import,   hobbes::tok::kw_type,
                 hobbes::tok::kw_data,     hobbes::tok::kw_class,
                 hobbes::tok::kw_instance, hobbes::tok::kw_exists,
                 hobbes::tok::kw_not,      hobbes::tok::kw_let,
                 hobbes::tok::kw_case,     hobbes::tok::kw_default,
                 hobbes::tok::kw_match,    hobbes::tok::kw_matches,
                 hobbes::tok::kw_parse,    hobbes::tok::kw_with,
                 hobbes::tok::kw_of,       hobbes::tok::kw_and,
                 hobbes::tok::kw_or,       hobbes::tok::kw_if,
                 hobbes::tok::kw_then,     hobbes::tok::kw_else,
                 hobbes::tok::kw_true,     hobbes::tok::kw_false,
                 hobbes::tok::kw_in,       hobbes::tok::kw_pack,
                 hobbes::tok::kw_unpack,   hobbes::tok::kw_do,
                 hobbes::tok::kw_return}),
};

TEST(Lexer, Ident) {
  for (auto t : test_cases) {
    auto lexer = hobbes::Lexer(&t.input[0], &t.input[0], &t.input.end()[0]);
    hobbes::Token tok;
    for (auto out : t.output) {
      lexer.LexToken(tok);
      EXPECT_EQ(static_cast<short>(tok.getKind()), static_cast<short>(out));
    }
  }
}

TEST(Lexer, String) {
  auto t = LexTestCase("\"Hello this is a string literal\"",
                       {hobbes::tok::string_literal});
  auto lexer = hobbes::Lexer(&t.input[0], &t.input[0], &t.input.end()[0]);
  hobbes::Token tok;
  for (auto out : t.output) {
    lexer.LexToken(tok);
    EXPECT_EQ(static_cast<short>(tok.getKind()), static_cast<short>(out));
  }
}
