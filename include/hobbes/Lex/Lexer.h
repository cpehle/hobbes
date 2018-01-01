#pragma once

#include "hobbes/Lex/Token.h"

#include <iostream>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>

namespace hobbes {

class Lexer {

public:
  Lexer(const char *BufStart, const char *BufPtr, const char *BufEnd) {
    InitLexer(BufStart, BufPtr, BufEnd);
  }
  auto InitLexer(const char *BufStart, const char *BufPtr, const char *BufEnd)
      -> void;
  auto LexIdentifier(Token &Result, const char *CurPtr) -> bool;
  auto LexNumericConstant(Token &Result, const char *CurPtr) -> bool;
  auto SkipSingleLineComment(const char *CurPtr) -> bool;
  auto SkipMultiLineComment(const char *CurPtr) -> bool;
  auto LexStringLiteral(Token &Result, const char *CurPtr) -> bool;
  auto LexCharConstant(Token &Result, const char *CurPtr, tok::TokenKind Kind)
      -> bool;
  auto LexEndOfFile(Token &Result, const char *CurPtr) -> bool;

  /// Return the next token in the file. If the Lexer is at the end of the
  /// file it will return tok::eof.
  auto LexToken(Token &Result) -> bool;

  auto FormTokenWithChars(Token &Result, const char *TokEnd,
                          tok::TokenKind Kind) -> void {
    unsigned TokLen = TokEnd - BufferPtr;
    Result.setLength(TokLen);
    Result.setKind(Kind);
    Result.setLiteralData(llvm::StringRef(BufferPtr, TokLen));
    BufferPtr = TokEnd;
  }
  
  llvm::StringRef getBuffer() const {
    return llvm::StringRef(BufferStart, BufferEnd - BufferStart);
  }
private:
  const char *BufferStart; // start of the buffer
  const char *BufferEnd;   // end of the buffer
  const char *BufferPtr;   // current position in the buffer
};
}
