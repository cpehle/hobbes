#pragma once

#include "hobbes/Lex/Token.h"

namespace hobbes {
struct Lexer {
  Lexer(const char *BufStart, const char *BufPtr, const char *BufEnd) {
    InitLexer(BufStart, BufPtr, BufEnd);
  }

  auto InitLexer(const char *BufStart, const char *BufPtr, const char *BufEnd)
      -> void;
  auto LexIdentifier(Token &Result, const char *CurPtr) -> bool;
  auto LexNumericConstant(Token &Result, const char *CurPtr) -> bool;
  auto LexStringLiteral(Token &Result, const char *CurPtr, tok::TokenKind Kind)
      -> bool;
  auto LexCharConstant(Token &Result, const char *CurPtr, tok::TokenKind Kind)
      -> bool;
  auto LexEndOfFile(Token &Result, const char *CurPtr) -> bool;
  auto LexToken(Token &Result) -> bool;

  auto FormTokenWithChars(Token &Result, const char *TokEnd,
                          tok::TokenKind Kind) -> void {
    unsigned TokLen = TokEnd - BufferPtr;
    Result.setLength(TokLen);
    Result.setKind(Kind);
    BufferPtr = TokEnd;
  }

private:
  const char *BufferStart;
  const char *BufferEnd;
  const char *BufferPtr;
};
}
