#include "hobbes/Lex/Parser.h"
#include "hobbes/Lex/Token.h"
#include "hobbes/lang/module.H"
#include "hobbes/util/autorelease.H"

namespace hobbes {

// cpp : id | cpp "." id
bool Parser::parseQualifiedId() {
  hobbes::Token tok;
  Lexer.LexToken(tok);
}

bool Parser::parseImportStatement() { parseQualifiedId(); }

bool parseNameSeq() {}

bool arseTypeDeclaration() { parseNameSeq(); }

bool Parser::parseModuleDefs() {
  hobbes::Token tok;
  Lexer.LexToken(tok);
  hobbes::tok::TokenKind kind = tok.getKind();

  if (kind == tok::kw_import) {
    parseImportStatement();
  } else if (kind == tok::kw_type || kind == tok::kw_data) {

  } else if (kind == tok::kw_class) {

  } else if (kind == tok::kw_instance) {

  } else if (kind == tok::identifier) {
  }
}

bool Parser::parseModule(const Module *&result) {
  hobbes::Token tok;

  Lexer.LexToken(tok);
  auto module_defs = autorelease(new ModuleDefs);
  if (tok.getKind() == tok::kw_module) {
    Lexer.LexToken(tok);
    if (tok.getKind() == tok::identifier) {
    }
    Lexer.LexToken(tok);
   if (tok.getKind() == tok::kw_where) {
    }
  } else {
  }

  parseModuleDefs();
}
}
