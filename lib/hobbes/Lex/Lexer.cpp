#include "hobbes/Lex/Lexer.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/MemoryBuffer.h>

#include <string>

namespace hobbes {
auto Lexer::InitLexer(const char *BufStart, const char *BufPtr,
                      const char *BufEnd) -> void {
  BufferStart = BufStart;
  BufferPtr = BufPtr;
  BufferEnd = BufEnd;
}

auto isIdentifierBody(const char C) -> bool {
  return (('A' <= C) && (C <= 'Z')) || (('a' <= C) && (C <= 'z')) ||
         (('0' <= C) && (C <= '9'));
}

auto isNumberBody(const char C) -> bool { return (('0' <= C) && (C <= '9')); }

auto Lexer::LexIdentifier(Token &Result, const char *CurPtr) -> bool {
  // Match [_A-Za-z0-9]*, we have already matched [_A-Za-z$]
  unsigned char C = *CurPtr++;
  while (isIdentifierBody(C)) {
    C = *CurPtr++;
  }
  --CurPtr; // Back up over the skipped character.
  auto Kind = llvm::StringSwitch<tok::TokenKind>(
                  llvm::StringRef(BufferPtr, CurPtr - BufferPtr))
                  .Case("module", tok::kw_module)
                  .Case("where", tok::kw_where)
                  .Case("import", tok::kw_import)
                  .Case("type", tok::kw_type)
                  .Case("data", tok::kw_data)
                  .Case("class", tok::kw_class)
                  .Case("instance", tok::kw_instance)
                  .Case("exists", tok::kw_exists)
                  .Case("not", tok::kw_not)
                  .Case("let", tok::kw_let)
                  .Case("case", tok::kw_case)
                  .Case("default", tok::kw_default)
                  .Case("match", tok::kw_match)
                  .Case("matches", tok::kw_matches)
                  .Case("parse", tok::kw_parse)
                  .Case("with", tok::kw_with)
                  .Case("of", tok::kw_of)
                  .Case("and", tok::kw_and)
                  .Case("or", tok::kw_or)
                  .Case("if", tok::kw_if)
                  .Case("then", tok::kw_then)
                  .Case("else", tok::kw_else)
                  .Case("true", tok::kw_true)
                  .Case("false", tok::kw_false)
                  .Case("in", tok::kw_in)
                  .Case("pack", tok::kw_pack)
                  .Case("unpack", tok::kw_unpack)
                  .Case("do", tok::kw_do)
                  .Case("return", tok::kw_return)
                  .Default(tok::identifier);
  FormTokenWithChars(Result, CurPtr, Kind);
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
  // Back Up
  CurPtr--;
  // Update the location of token as well as BufferPtr.
  const char *TokStart = BufferPtr;
  FormTokenWithChars(Result, CurPtr, tok::numeric_constant);
  Result.setLiteralData(TokStart);
  return true;
}

auto Lexer::LexStringLiteral(Token &Result, const char *CurPtr) -> bool {
  // consume the '"' character
  CurPtr++;
  while (*CurPtr != '"') {
    ++CurPtr;
    if (*CurPtr == 0) {
      LexEndOfFile(Result, CurPtr);
      return true;
    }
  }
  FormTokenWithChars(Result, CurPtr - 1, tok::string_literal);
  BufferPtr = CurPtr + 1;
  return false;
}

auto Lexer::LexCharConstant(Token &Result, const char *CurPtr,
                            tok::TokenKind Kind) -> bool {}

auto Lexer::LexEndOfFile(Token &Result, const char *CurPtr) -> bool {
  FormTokenWithChars(Result, CurPtr, tok::eof);
  return true;
}

auto Lexer::SkipMultiLineComment(const char *CurPtr) -> bool {
  while (*CurPtr != 0) {
    ++CurPtr;
    if (*CurPtr == '*') {
      ++CurPtr;
      if (*CurPtr == '/') {
        ++CurPtr;
        break;
      }
    }
  }
  BufferPtr = CurPtr;
  return true;
}

auto Lexer::SkipSingleLineComment(const char *CurPtr) -> bool {
  while (*CurPtr != 0 && *CurPtr != '\n') {
    ++CurPtr;
  }
  if (*CurPtr == '\n') {
    CurPtr++;
  }
  BufferPtr = CurPtr;
  return true;
}

/// Advance by one character and return the previous character
char advanceChar(const char *Ptr) { return *Ptr++; }

auto Lexer::LexToken(Token &Result) -> bool {
  // return here if a comment was encountered
  while (1) {
    const char *CurPtr = BufferPtr;
    // skip whitespace
    if ((*CurPtr == ' ') || (*CurPtr == '\t')) {
      ++CurPtr;
      while ((*CurPtr == ' ') || (*CurPtr == '\t')) {
        ++CurPtr;
      }
      BufferPtr = CurPtr;
    }

    // Read a character, advancing over it.
    char Char = advanceChar(CurPtr);
    tok::TokenKind Kind;
    switch (Char) {
    case 0:
      if (CurPtr - 1 == BufferEnd) {
        return LexEndOfFile(Result, CurPtr - 1);
      } else {
        Kind = tok::unknown;
      }
      break;
    // clang-format off
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return LexNumericConstant(Result, CurPtr);
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y':
    case 'Z': case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h': case 'i':
    case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's':
    case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z': case '_':
      return LexIdentifier(Result, CurPtr);
    // clang-format on
    case '"':
      return LexStringLiteral(Result, CurPtr);
    case '[':
      Kind = tok::l_square;
      break;
    case ']':
      Kind = tok::r_square;
      break;
    case '(':
      Kind = tok::l_paren;
      break;
    case ')':
      Kind = tok::r_paren;
      break;
    case '{':
      Kind = tok::l_brace;
      break;
    case '}':
      Kind = tok::r_brace;
      break;
    case '|':
      Kind = tok::pipe;
      break;
    case ',':
      Kind = tok::comma;
      break;
    case ';':
      Kind = tok::semi;
      break;
    case '=': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '=') {
        C = advanceChar(CurPtr);
        if (C == '=') {
          Kind = tok::equalequalequal;
        } else {
          CurPtr--;
          Kind = tok::equalequal;
        }
      } else if (C == '>') {
        Kind = tok::doublearrow;
      } else {
        CurPtr--;
        Kind = tok::equal;
      }
      break;
    }
    case '+': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '+') {
        Kind = tok::plusplus;
      } else {
        CurPtr--;
        Kind = tok::plus;
      }
      break;
    }
    case '-': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '>') {
        Kind = tok::arrow;
      } else {
        CurPtr--;
        Kind = tok::minus;
      }
      break;
    }
    case '!': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '=') {
        Kind = tok::notequal;
      } else {
        CurPtr--;
        Kind = tok::tnot;
      }
      break;
    }
    case '/': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '*') {
        SkipMultiLineComment(CurPtr);
        continue;
      } else if (C == '/') {
        SkipSingleLineComment(CurPtr);
        continue;
      } else {
        CurPtr--;
        Kind = tok::slash;
      }
      break;
    }
    case '>': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '=') {
        Kind = tok::greaterequal;
      } else {
        CurPtr--;
        Kind = tok::greater;
      }
      break;
    }
    case '<': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == '=') {
        Kind = tok::lessequal;
      } else if (C == '-') {
        Kind = tok::assign;
      } else {
        CurPtr--;
        Kind = tok::less;
      }
      break;
    }
    case ':': {
      CurPtr++;
      unsigned char C = *CurPtr++;
      if (C == ':') {
        Kind = tok::coloncolon;
      } else if (C == '=') {
        Kind = tok::colonequal;
      } else {
        CurPtr--;
        Kind = tok::colon;
      }
      break;
    }
    case '.': {
      CurPtr++;
      unsigned char C = *CurPtr;
      if (C == '.') {
        Kind = tok::upto;
      } else {
        CurPtr--;
        Kind = tok::period;
      }
      break;
    }
    case '^':
      Kind = tok::caret;
      break;
    case '~':
      Kind = tok::tilde;
      break;
    case '@':
      Kind = tok::at;
      break;
    case '$':
      Kind = tok::dollar;
      break;
    case '?':
      Kind = tok::question;
      break;
    case '%':
      Kind = tok::percent;
      break;
    case '*':
      Kind = tok::star;
      break;
    case '\\':
      Kind = tok::fun;
      break;
    case '\'':
      Kind = tok::squote;
      break;
    case '`':
      Kind = tok::equote;
      break;
    default:
      Kind = tok::unknown;
      break;
    }
    CurPtr++;
    FormTokenWithChars(Result, CurPtr, Kind);
    break;
  }
  return true;
}
}
