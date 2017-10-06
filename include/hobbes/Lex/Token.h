#pragma once

namespace hobbes {

namespace tok {
enum TokenKind : short {
  #define TOK(X) X,
  #include "hobbes/Lex/TokenKinds.def"
  NUM_TOKENS,
};
}

class Token {
  tok::TokenKind Kind;
  unsigned UIntData;
  void *PtrData;
  
  //  unsigned Loc;
public:
  auto getKind() -> const tok::TokenKind { return Kind; }
  auto setKind(tok::TokenKind K) -> void { Kind = K; }
  auto setLength(unsigned Len) -> void { UIntData = Len; };
  //  auto setLocation(SourceLocation L) -> void { Loc = L.getRawEncoding(); }
  auto startToken() -> void {
    Kind = tok::unknown;
    PtrData = nullptr;
    UIntData = 0;
    // Loc = SourceLocation().getRawEncoding();
  }
  auto setLiteralData(const char *Ptr) -> void {
    PtrData = const_cast<char*>(Ptr);
  }
  auto getLiteralData() -> const char * {
      return static_cast<char *>(PtrData);
  }
  auto is(tok::TokenKind K) -> const bool { return Kind == K; }
  auto isNot(tok::TokenKind K) -> const bool { return Kind != K; }
  auto isOneOf(tok::TokenKind K1, tok::TokenKind K2) -> const bool {
    return is(K1) || is(K2);
  }
  template <typename... Ts>
  auto isOneOf(tok::TokenKind K1, tok::TokenKind K2, Ts... Ks) -> const bool {
    return is(K1) || isOneOf(K2, Ks...);
  }
};
}
