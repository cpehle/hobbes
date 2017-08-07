#include "hobbes/Lex/Lexer.h"
#include <string>

namespace hobbes {

auto Lexer::InitLexer(const char *BufStart, const char *BufPtr,
                      const char *BufEnd) -> void {
  BufferStart = BufStart;
  BufferPtr = BufPtr;
  BufferEnd = BufEnd;
}
    
    auto isIdentifierBody(const char C) -> bool {
        return (('A' <= C) && (C <= 'X')) || (('a' <= C) && (C <= 'x')) || (('0' <= C) &&  (C <= '9'));
    }
    
    auto isNumberBody(const char C) -> bool {
        return (('0' <= C) && (C <= '9'));
    }

  auto Lexer::LexIdentifier(Token &Result, const char *CurPtr) -> bool {
      // Match [_A-Za-z0-9]*, we have already matched [_A-Za-z$]
    unsigned char C = *CurPtr++;
    while (isIdentifierBody(C)) {
      C = *CurPtr++;
    }
    --CurPtr;   // Back up over the skipped character.
    FormTokenWithChars(Result, CurPtr, tok::identifier);
    return true;
  }

auto Lexer::LexNumericConstant(Token &Result, const char *CurPtr) -> bool {
    char C = *CurPtr++;
    char PrevCh = 0;
    while (isNumberBody(C)) {
        PrevCh = C;
        C = *CurPtr++;
    }
    // If we fell out, check for a sign, due to 1e+12.  If we have one, continue.
    if ((C == '-' || C == '+') && (PrevCh == 'E' || PrevCh == 'e')) {
        return LexNumericConstant(Result, CurPtr++);
    }
    // Update the location of token as well as BufferPtr.
    const char *TokStart = BufferPtr;
    FormTokenWithChars(Result, CurPtr, tok::numeric_constant);
    Result.setLiteralData(TokStart);
    return true;
}

auto Lexer::LexStringLiteral(Token &Result, const char *CurPtr, tok::TokenKind Kind) -> bool {
}

auto Lexer::LexCharConstant(Token &Result, const char *CurPtr, tok::TokenKind Kind) -> bool {
}

auto Lexer::LexEndOfFile(Token &Result, const char *CurPtr) -> bool {
}

auto Lexer::LexToken(Token &Result) -> bool {
  const char *CurPtr = BufferPtr;

  // skip whitespace
  if ((*CurPtr == ' ') || (*CurPtr == '\t')) {
    ++CurPtr;
    while ((*CurPtr == ' ') || (*CurPtr == '\t')) {
      ++CurPtr;
    }
    BufferPtr = CurPtr;
  }

  char Char = *CurPtr++;
  tok::TokenKind Kind;

  switch (Char) {
  case 0: // Null.
    // Found end of file?
    if (CurPtr - 1 == BufferEnd) {
      return LexEndOfFile(Result, CurPtr - 1);
    }
    // Diag(CurPtr - 1, diag::null_in_file);
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return LexNumericConstant(Result, CurPtr);
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
  case 'G':
  case 'H':
  case 'I':
  case 'J':
  case 'K':
  case 'L':
  case 'M':
  case 'N':
  case 'O':
  case 'P':
  case 'Q':
  case 'R':
  case 'S':
  case 'T':
  case 'U':
  case 'V':
  case 'W':
  case 'X':
  case 'Y':
  case 'Z':
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
  case 'g':
  case 'h':
  case 'i':
  case 'j':
  case 'k':
  case 'l':
  case 'm':
  case 'n':
  case 'o':
  case 'p':
  case 'q':
  case 'r':
  case 's':
  case 't':
  case 'u':
  case 'v':
  case 'w':
  case 'x':
  case 'y':
  case 'z':
  case '_':
    return LexIdentifier(Result, CurPtr);
  case '\'':
    return LexCharConstant(Result, CurPtr, tok::char_constant);
  case '"':
    return LexStringLiteral(Result, CurPtr, tok::string_literal);    
  }
}
}
