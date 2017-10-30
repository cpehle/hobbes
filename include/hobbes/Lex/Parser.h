#pragma once

#include "hobbes/Lex/Lexer.h"
#include "hobbes/lang/module.H"

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

namespace hobbes {

class Parser {
public:
  Lexer Lexer;
  /// Current Token we are looking at
  Token Tok;
  bool Note();
  bool Warning();
  bool Error();
  bool parseModule(const Module *&result);
  bool parseModuleDefs();
  bool parseImportStatement();
  bool parseQualifiedId();
  
  /*
  SourceLocation ConsumeToken() {
    PrevTokenLocation = Tok.getLocation();
    Lexer.LexToken(Tok);
    return PrevTokenLocation;
  }

  bool TryConsumeToken(tok::TokenKind Expected) {
    if (Tok.isNot(Expected)) {
      return false;
    }
    PrevTokLocation = Tok.getLocation();
    Lexer.LexToken(Tok);
    return true;
  }
  */
private:
  
};
}
