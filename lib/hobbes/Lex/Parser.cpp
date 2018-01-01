/// See LICENSE.md for license details
#include "hobbes/Lex/Parser.h"
#include "hobbes/Lex/Lexer.h"
#include "hobbes/Lex/Token.h"
#include "hobbes/lang/expr.H"
#include "hobbes/lang/module.H"
#include "hobbes/lang/pat/pattern.H"
#include "hobbes/lang/preds/consrecord.H"
#include "hobbes/util/autorelease.H"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <memory>

namespace hobbes {

using LetBinding = std::pair<PatternPtr, hobbes::ExprPtr>;
using LetBindings = std::vector<LetBinding>;

Expr *compileNestedLetMatch(hobbes::cc *cc, const LetBindings &bs,
                            const ExprPtr &e, const LexicalAnnotation &la) {
  ExprPtr r = e;
  for (LetBindings::const_reverse_iterator b = bs.rbegin(); b != bs.rend();
       ++b) {
    r = compileMatch(cc, list(b->second), list(PatternRow(list(b->first), r)),
                     la);
  }
  return r->clone();
}

PatternPtr pickNestedPat(Patterns *pats, const LexicalAnnotation &la) {
  if (pats->size() == 0) {
    return std::make_shared<MatchLiteral>(PrimitivePtr(new Unit(la)), la);
  } else {
    MatchRecord::Fields fds;
    for (unsigned i = 0; i < pats->size(); ++i) {
      fds.push_back(MatchRecord::Field(".f" + str::from(i), (*pats)[i]));
    }
    return std::make_shared<MatchRecord>(fds, la);
  }
}

Parser::Parser(class Lexer &Lex) : Lex(Lex) {
  // Initialize the operator table with the built in
  // operator precedences
  operators.insert({"and", {10, Assoc::left}});
  operators.insert({"or", {10, Assoc::left}});
  operators.insert({"o", {10, Assoc::left}});

  operators.insert({"~", {20, Assoc::left}});
  operators.insert({"===", {20, Assoc::left}});
  operators.insert({"==", {20, Assoc::left}});
  operators.insert({"!=", {20, Assoc::left}});

  operators.insert({"<", {30, Assoc::left}});
  operators.insert({"<=", {30, Assoc::left}});
  operators.insert({">", {30, Assoc::left}});
  operators.insert({">=", {30, Assoc::left}});

  operators.insert({"+", {40, Assoc::left}});
  operators.insert({"-", {40, Assoc::left}});
  operators.insert({"++", {40, Assoc::left}});

  operators.insert({"*", {50, Assoc::left}});
  operators.insert({"/", {50, Assoc::left}});
  operators.insert({"%", {50, Assoc::left}});
}

void Parser::ConsumeToken() {
  Lex.LexToken(Tok);
  std::cout << tok::getTokenName(Tok.getKind()) << std::endl;
}
void Parser::ExpectTokenKind(tok::TokenKind Kind) {
  if (Tok.is(Kind)) {
    Parser::ConsumeToken();
  } else {
    std::cout << "expected " << tok::getTokenName(Kind) << std::endl;
    exit(1);
  }
}

/// qualified-id:
///         id ['.' id]+
bool Parser::ParseQualifiedId(llvm::Twine &QualID) {
  ConsumeToken();
  while (Tok.is(tok::identifier)) {
    QualID.concat(Tok.getLiteralData());
    ConsumeToken();
    if (Tok.is(tok::period)) {
      QualID.concat(".");
      ConsumeToken();
    } else {
      break;
    }
  }
  return false;
}

/// import-statement:
///     'import' qualified-id
std::shared_ptr<MImport> Parser::ParseImportStatement() {
  ConsumeToken();
  llvm::Twine QualID;
  ParseQualifiedId(QualID);
  std::cout << QualID.str();
  return std::make_shared<MImport>(
      "", QualID.str(), LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
}

/// type-declaration:
///     "type" tyid "="
bool ParseTypeDeclaration() { return false; }

auto Parser::GetBinopPrecedence(Token &tok) -> std::pair<int, Parser::Assoc> {
  if (tok.getKind() <= tok::comment_single) {
    return {-1, Assoc::left};
  }

  auto OpInfo = operators.find(tok.getLiteralData());
  int NextTokenPrec = -1;
  Assoc Assoc = Assoc::left;

  if (OpInfo != operators.end()) {
    NextTokenPrec = OpInfo->getValue().first;
    Assoc = OpInfo->getValue().second;
  }

  return {NextTokenPrec, Assoc};
}

/// record-pattern-fields:
///         record-pattern-fields "," record-pattern-field
///         record-pattern-field
/// record-pattern-field:
///         id "=" pattern
auto Parser::ParseRecordPatternFields(tok::TokenKind ClosingTok) -> std::vector<MatchRecord::Field>* {
    auto MatchRecords = new MatchRecord::Fields();
    while (true) {
        if (Tok.isNot(tok::identifier)) {
            std::cout << "expected identifier" << std::endl;
            exit(1);
        }
        auto Id = Tok.getLiteralData();
        ConsumeToken();
        ExpectTokenKind(tok::equal);
            
        auto Pattern = Parser::ParsePattern();
        if (!Pattern) {
            std::cout << "expected pattern" << std::endl;
            exit(1);
        }
        MatchRecords->push_back(MatchRecord::Field(Id, Pattern));
        if (Tok.is(tok::r_paren)) {
            break;
        }
        ExpectTokenKind(tok::comma);
    }
    ExpectTokenKind(ClosingTok);
    return MatchRecords;
}
    
    
/// Pattern
///
///
///  
/// refutable-pattern:
///             "boolV"
///           | "charV"
///           | "byteV"
///           | "shortV"
///           | "intV"
///           | "longV"
///           | "doubleV"
///           | "bytesV"
///           | "stringV"
///           | tsseq
///           | "timeV"
///           | "dateTimeV"
///           | "regexV"
///           | "[" patternseq "]"
///           | "|" id "|"
///           | "|" id "=" pattern "|"
///           | "|" "intV" "=" pattern "|"
///           | "(" patternseq ")"
///           | "{" record-pattern-fields "}"
///           | id
PatternPtr Parser::ParsePattern() {
  if (Tok.is(tok::l_square)) {
    ConsumeToken();
    auto PatternSeq = Parser::ParsePatternSequence(tok::r_square);
    ExpectTokenKind(tok::r_square);
    return std::make_shared<MatchArray>(
        PatternSeq, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::pipe)) {
    ConsumeToken(); // consume '|'
    if (Tok.is(tok::identifier)) {
      auto Id = Tok.getLiteralData();
      ConsumeToken();
      if (Tok.is(tok::equal)) {
        ConsumeToken(); // consume '='
        auto Pattern = Parser::ParsePattern();
        ExpectTokenKind(tok::pipe);
        return std::make_shared<MatchVariant>(
            Id, Pattern, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
      }
      ExpectTokenKind(tok::pipe);
      return std::make_shared<MatchVariant>(
          Id, std::make_shared<MatchLiteral>(
                  std::make_shared<Unit>(
                      LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
                  LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
          LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
    } else if (Tok.is(tok::numeric_constant)) {
      auto Id = Tok.getLiteralData();
      ConsumeToken();
      ExpectTokenKind(tok::equal);
      auto Pattern = ParsePattern();
      ExpectTokenKind(tok::pipe);
      return std::make_shared<MatchVariant>(
          (".f" + Id).str(), Pattern,
          LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
    } else {
      std::cout << "expected variant pattern" << std::endl;
      exit(1);
      // Error
    }
  } else if (Tok.is(tok::l_paren)) {
    ConsumeToken();
    auto PatternSeq = Parser::ParsePatternSequence(tok::l_paren);
    ExpectTokenKind(tok::r_paren);
    return pickNestedPat(&PatternSeq,
                         LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::l_brace)) {
    ConsumeToken();
    auto RecordPatterns = Parser::ParseRecordPatternFields(tok::l_brace);
    ExpectTokenKind(tok::r_brace);
    return std::make_shared<MatchRecord>(
        *RecordPatterns, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::identifier)) {
    auto Id = Tok.getLiteralData();
    ConsumeToken();
    // TODO: Make overridable
    return std::make_shared<MatchAny>(
        Id, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::numeric_constant)) {
    // TODO: Disambiguate literals
    // return std::make_shared<MatchLiteral>(PrimitivePtr());
    return nullptr;
  }
  return nullptr;
}



/// pattern-sequence
///
std::vector<PatternPtr>
Parser::ParsePatternSequence(tok::TokenKind ClosingTok) {
  // empty
  if (Tok.is(ClosingTok)) {
    return Patterns();
  }
  std::vector<PatternPtr> Patterns;
  /// patternseqn:
  ///         | patternseqn "," pattern
  ///         | pattern
  while (true) {
    auto Pattern = Parser::ParsePattern();
    if (!Pattern) {
      std::cout << "expected pattern" << std::endl;
      exit(1);
    }
    Patterns.push_back(Pattern);
    if (Tok.is(ClosingTok)) {
      break;
    }
    ExpectTokenKind(tok::comma);
  }
  ExpectTokenKind(ClosingTok);
  return Patterns;
}

/// irrefutable-pattern:
///         id
///         "(" pattern-sequence ")"
///         "{" record-pattern-fields "}"
PatternPtr Parser::ParseIrrefutablePattern() {
  if (Tok.is(tok::identifier)) {
    auto Id = Tok.getLiteralData();
    ConsumeToken();
    return std::make_shared<MatchAny>(
        Id, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::l_paren)) {
    ExpectTokenKind(tok::l_paren);
    auto Patterns = ParsePatternSequence(tok::r_paren);
    return pickNestedPat(&Patterns,
                         LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::l_brace)) {
    ExpectTokenKind(tok::l_brace);
    auto MatchRecords = Parser::ParseRecordPatternFields(tok::r_brace);
    return std::make_shared<MatchRecord>(
        *MatchRecords, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else {
    // Error
    return nullptr;
  }
}

ExprPtr Parser::ParseIdentifierExpr() {
  auto IdName = Tok.getLiteralData();

  ConsumeToken(); // eat the identifier

  if (Tok.is(tok::l_paren)) {
    // this is a call
    ExpectTokenKind(tok::l_paren);
    std::vector<ExprPtr> Args;
    if (Tok.isNot(tok::r_paren)) {
      while (true) {
        if (auto Arg = ParseExpression()) {
          Args.push_back(std::move(Arg));
        } else {
          return nullptr;
        }

        if (Tok.is(tok::r_paren)) {
          break;
        }

        ExpectTokenKind(tok::comma);
      }
    }

    ExpectTokenKind(tok::r_paren);
    return std::make_shared<App>(
        var(IdName, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))), Args,
        LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::l_square)) {
    std::cout << "not implemented" << std::endl;
    exit(1);
  } else {
    return std::make_shared<Var>(
        IdName, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
    // found a variable
  }
}

/// match-expression:
///         'match' expr 'with' [ '|' pattern-row ]+
/// pattern-row:
///         pattern ['where' expr] '->' expr
ExprPtr Parser::ParseMatchExpr() {
  ExpectTokenKind(tok::kw_match);

  // expressions to be matched
  llvm::SmallVector<ExprPtr, 5> MatchExprs;

  while (true) {
    auto Match = Parser::ParseExpression();
    if (!Match) {
      std::cout << "expected match expression" << std::endl;
      exit(1);
      return nullptr;
    }
    MatchExprs.push_back(Match);
    if (Tok.is(tok::kw_where)) {
      break;
    }
  }
  ExpectTokenKind(tok::kw_where);

  // parse pattern rows
  llvm::SmallVector<std::shared_ptr<PatternRow>, 10> PatternRows;

  while (Tok.is(tok::pipe)) {
    // llvm::SmallVector<std::shared_ptr<Pattern>, 10> Patterns;
    std::vector<std::shared_ptr<Pattern>> Patterns;
    while (true) {
      auto Pattern = Parser::ParsePattern();
      if (!Pattern) {
	std::cout << "Expected Pattern" << std::endl;
	exit(1);
      }
      Patterns.push_back(Pattern);
      if (Tok.is(tok::arrow) || Tok.is(tok::kw_where)) {
        break;
      }
    }
    ExprPtr WhereExpr;
    if (Tok.is(tok::kw_where)) {

      WhereExpr = Parser::ParseExpression();
      if (!WhereExpr) {
	std::cout << "Expected where expression" << std::endl;
	exit(1);
      }
    }
    ExpectTokenKind(tok::arrow);
    auto PatternRowBody = Parser::ParseExpression();
    if (!PatternRowBody) {
      	std::cout << "Expected expression" << std::endl;
	exit(1);
    }
    PatternRows.push_back(
        std::make_shared<PatternRow>(Patterns, WhereExpr, PatternRowBody));
  }
  // Actions.ActOnMatch(MatchExpressions,
  // Actions.ActOnPatternRows(PatternRows));

  // compileMatch(compiler, MatchExprs, normPatternRules(PatternRows, ), );
}

/// let-expression:
///     'let' letbindings [';'] 'in' expression
ExprPtr Parser::ParseLetExpr() {
  ExpectTokenKind(tok::kw_let);

  llvm::SmallVector<LetBinding, 10> Bindings;

  // parse let bindings
  while (true) {
    // let-binding:
    //     irrefutable_pattern = expession
    auto Pattern = Parser::ParseIrrefutablePattern();
    ExpectTokenKind(tok::equal);
    auto Exp = Parser::ParseExpression();
    Bindings.push_back(std::pair<PatternPtr, ExprPtr>{Pattern, Exp});

    if (Tok.is(tok::kw_in)) {
      break;
    }

    ExpectTokenKind(tok::semi);

    if (Tok.is(tok::kw_in)) {
      break;
    }
  }
  ExpectTokenKind(tok::kw_in);

  auto Body = Parser::ParseExpression();
  if (!Body) {
    return nullptr;
  }

  // return Actions.ActOnLet(Bindings, Body);
  // compileNestedLetMatch(, Bindings, Body, LexicallyAnnotated::make(Pos(0,0),
  // Pos(0,0))));
  return nullptr;
}

/// if-expression:
///         'if' expression 'then' expression 'else' expression
ExprPtr Parser::ParseIfExpr() {
  ExpectTokenKind(tok::kw_if);

  auto Cond = ParseExpression();
  if (!Cond) {
    return nullptr;
  }

  ExpectTokenKind(tok::kw_then);

  auto Then = ParseExpression();
  if (!Then) {
    return nullptr;
  }

  ExpectTokenKind(tok::kw_else);

  auto Else = ParseExpression();
  if (!Else) {
    return nullptr;
  }

  return std::make_shared<App>(
      var("if", LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
      list(Cond, Then, Else), LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));

  // return Actions.ActOnIfExpr(std::move(Cond), std::move(Then),
  // std::move(Else));
}

/// numeric-constant:
///     float-literal
///     integer-literal
ExprPtr Parser::ParseNumericConstant() {
  long Lit;
  Tok.getLiteralData().getAsInteger(10, Lit);
  ConsumeToken();
  return std::make_shared<Int>(Lit,
                               LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
}

/// expr:
///      identifier expr'
///    | literal expr'
///    | array-constructor-elimination expr'
///    | variant-constructor-elimination expr'
///    | case-expr expr'
///    | record-construction-elimination expr'
///    | recordfield-path expr'
///    | pack-expression expr'
///    | unpack-expression expr'
///    | paren-expr expr'
///    | quoted-expr expr'

/// expr':
///      "(" cargs ")" expr'
///    | "[" l0expr "]" expr'
///    | record-field-path expr'
///    | empty

/// primary-expression:
///         numeric-constant
///       | identifier-expression
///       | if-expression
///       | let-expression
///       | match-expression
///       | do-expression
///       | case-expression
///       | string-literal
///       | char-literal
///       | array-literal
ExprPtr Parser::ParsePrimaryExpression() {
  switch (Tok.getKind()) {
  case tok::numeric_constant:
    return Parser::ParseNumericConstant();
  case tok::identifier:
    return Parser::ParseIdentifierExpr();
  case tok::kw_if:
    return Parser::ParseIfExpr();
  case tok::kw_let:
    return Parser::ParseLetExpr();
  case tok::kw_match:
    return Parser::ParseMatchExpr();
  case tok::kw_do:
  // return Parser::ParseDoExpr();
  case tok::kw_case:
  // return Parser::ParseCaseExpr();
  case tok::l_square:
  // return Parser::ParseArrayExpr();
  default:
    break;
  }
  return nullptr;
}

/// parse the right hand side of a binary operator
ExprPtr Parser::ParseBinopRHS(ExprPtr LHS, int MinPrec) {
  while (1) {
    // If this was an operator token we should handle the error
    // here
    int NextTokenPrec;
    Parser::Assoc Assoc;
    std::tie(NextTokenPrec, Assoc) = GetBinopPrecedence(Tok);

    if (NextTokenPrec < MinPrec) {
      return LHS;
    }
    Parser::ConsumeToken();

    auto RHS = ParsePrimaryExpression();
    int ThisPrec = NextTokenPrec;

    bool isRightAssoc = Assoc == Assoc::right;

    if (ThisPrec < NextTokenPrec ||
        (ThisPrec == NextTokenPrec && isRightAssoc)) {
      RHS = ParseBinopRHS(std::move(RHS), ThisPrec + !isRightAssoc);
      std::tie(NextTokenPrec, Assoc) = GetBinopPrecedence(Tok);
    }

    auto OldLHS = std::move(LHS);
    LHS = std::make_shared<App>(
        var(Tok.getLiteralData().str(),
            LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
        list(std::move(OldLHS), std::move(RHS)),
        LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  }
}

/// expression: primary binoprhs
ExprPtr Parser::ParseExpression() {
  auto LHS = Parser::ParsePrimaryExpression();
  if (!LHS) {
    std::cout << "Expected Primary Expression" << std::endl;
    return nullptr;
  }
  return Parser::ParseBinopRHS(std::move(LHS), 0);
}

/// type:
///    id
///    "<" cppid ">"
///    "[" "]"
///    "[" ltmtype "]"
///    "[" ":" l0mtype "|" tyind ":" "]"
///    "(" "->" ")"
///    "(" ltmtype ")"
///    "{" mreclist "}"
///    "|" mvarlist "|"
///    "(" ")"
///    "intV"
///    "boolV"
///    "exists" id "." l1mtype
///    l1mtype "@" l1mtype
///    l1mtype "@" "?"
///    "^" id "." l1mtype
///    "stringV"
///    "`" l0expr "`"
auto Parser::ParseType() -> MonoTypePtr {
  switch (Tok.getKind()) {
  case tok::identifier:
    break;
  case tok::less:
    Parser::ConsumeToken();
    if (Tok.is(tok::identifier)) {

    } else {
      // error
    }
    Parser::ConsumeToken();

    if (Tok.is(tok::greater)) {

    } else {
      // error
    }
    Parser::ConsumeToken();
    break;
  case tok::l_square: {
    ConsumeToken();
    llvm::SmallVector<MonoTypePtr, 10> AccTy;
    while (1) {
      auto Type = ParseType();
      if (Type) {
      }
      ConsumeToken();
      if (Tok.is(tok::r_square)) {
        break;
      }
    }
    ExpectTokenKind(tok::r_square);
    break;
  }
  case tok::l_paren:
    ConsumeToken();
    if (Tok.is(tok::arrow)) {
      ConsumeToken();
      ExpectTokenKind(tok::r_paren);
    }
    if (Tok.is(tok::r_paren)) {
      ConsumeToken();
    }
    break;
  case tok::l_brace:
    break;
  case tok::kw_exists:
    Parser::ConsumeToken();
    // Parser::ParseIdentifierExpr();
    if (Tok.isNot(tok::period)) {
    }
    Parser::ConsumeToken();
    Parser::ParseType();
    break;
  case tok::caret:
    break;
  // TODO: missing "`"
  default:
    break;
  }
}

/// type-predicate:
///    id [l1mtype]+
///    "{" l1mtype "*" lm1type "}" "=" l1mtype
///    "{" id ":" l1mtype "*" l1mtype "}" "=" l1mtype
///    "(" l1mtype "*" l1mtype ")" "=" l1mtype
///    l1mtype "==" l1mtype
///    l1mtype "!=" l1mtype
///    l1mtype "~" lm1type
///    l1mtype "=" "{" l1mtype "*" l1mtype "}"
///    l1mtype "=" "{" id ":" l1mtype "*" l1mtype "}"
///    l1mtype "=" "(" l1mtype "*" l1mtype ")"
///    l1mtype "." recfieldname "::" l1mtype
///    l1mtype "." recfieldname "<-" l1mtype
///    l1mtype "/" l1mtype "::" l1mtype
///    l1mtype "/" l1mtype "<-" l1mtype
///    l1mtype "=" "|" l1mtype "+" l1mtype "|"
///    l1mtype "=" "|" id ":" l1mtype "+" l1mtype "|"
///    "|" l1mtype "+" l1mtype "|" "=" l1mtype
///    "|" id ":" l1mtype "+" l1mtype "|" "=" l1mtype
///    "|" id ":" l0mtype "|" "::" l1mtype
///    "|" l1mtype "/" l0mtype "|" "::" l1mtype
///    l1mtype "++" l1mtype "=" l1mtype
auto Parser::ParseTypePredicate() -> ConstraintPtr {
  if (Tok.is(tok::l_brace)) {
    ExpectTokenKind(tok::l_brace);

    // record field constraint
    if (Tok.is(tok::identifier)) {
      if (Tok.is(tok::colon)) {
      }
    }

    auto T0 = ParseType();
    ExpectTokenKind(tok::star);
    auto T1 = ParseType();
    ExpectTokenKind(tok::r_brace);
    ExpectTokenKind(tok::equal);
    auto T2 = ParseType();
    return std::make_shared<Constraint>(
        RecordDeconstructor::constraintName(),
        list(tlong(0), tlong(0), T2, freshTypeVar(), T0, T1));

  } else if (Tok.is(tok::pipe)) {
    // Not implemented
  } else if (Tok.is(tok::l_paren)) {
    ExpectTokenKind(tok::l_paren);
    auto T0 = ParseType();
    ExpectTokenKind(tok::star);
    auto T1 = ParseType();
    ExpectTokenKind(tok::r_paren);
    ExpectTokenKind(tok::equal);
    auto T2 = ParseType();
    return std::make_shared<Constraint>(
        RecordDeconstructor::constraintName(),
        list(tlong(0), tlong(1), T2, freshTypeVar(), T0, T1));
  }
}

/// class-definition:
///    "class" constraints "=>" id names
///    "class" constraints "=>" id names "|" fundeps
///    "class" constraints "=>" id names "|" fundeps "where" class_members
///    "class" constraints "=>" id names where class_members
///    "class" id names
///    "class" id names "|" fundeps
///    "class" id names "|" fundeps "where" class_members
///    "class" id names "where" class_members
auto Parser::ParseClassDef() -> bool {
  Parser::ConsumeToken(); // consume class keyword

  auto ParseTypeConstraints = [this]() {
    Parser::ConsumeToken(); // consume l_paren

    // tpred ["," tpred]*
    Parser::ParseTypePredicate();
    while (Tok.is(tok::colon)) {
      Parser::ConsumeToken();
      Parser::ParseTypePredicate();
    }

    return;
  };
  auto ParseFunctionalDependencies = [this]() {};
  auto ParseClassMembers = [this]() {};

  if (Tok.getKind() == tok::l_paren) {
    ParseTypeConstraints();

    if (Tok.isNot(tok::doublearrow)) {
    }
    Parser::ConsumeToken();
  }

  // class identifier
  if (Tok.isNot(tok::identifier)) {
  }
  Parser::ConsumeToken();

  while (Tok.getKind() == tok::identifier) {

    Parser::ConsumeToken();
  }

  if (Tok.getKind() == tok::pipe) {
    Parser::ConsumeToken();
    ParseFunctionalDependencies();

    if (Tok.getKind() == tok::kw_where) {
      ParseClassMembers();
    }
  }

  return false;
}

/// module-definition:
///         import-definition
///       | type-definition
///       | data-definition
///       | instance-definition
///       | class-definition
bool Parser::ParseModuleDefs() {
  // Prime Lexer
  Parser::ConsumeToken();
  hobbes::tok::TokenKind kind = Tok.getKind();
  if (kind == tok::kw_import) {
    Parser::ParseImportStatement();
  } else if (kind == tok::kw_type) {
    // ParseTypeDef();
  } else if (kind == tok::kw_data) {
    // ParseDataDef();
  } else if (kind == tok::kw_class) {
    // ParseClassDef();
  } else if (kind == tok::kw_instance) {
    // ParseInstanceDef();
  } else if (kind == tok::identifier) {
    std::vector<Token> idents;
    while (Tok.getKind() == tok::identifier) {
      idents.push_back(Tok);
      Parser::ConsumeToken();
    }
    if (Tok.getKind() == tok::equal) {
      // Lexer.LexToken();
      // ParseL0Expression();
    } else if (idents.size() == 1) {
      // ParseL5Expression();
    } else {
    }
  }
}

bool Parser::ParseModule() {
  Parser::ConsumeToken();
  if (Tok.getKind() == tok::kw_module) {
    Parser::ConsumeToken();
    if (Tok.getKind() == tok::identifier) {
    }
    Parser::ConsumeToken();
    if (Tok.getKind() == tok::kw_where) {
    }
  } else {
  }

  Parser::ParseModuleDefs();
}
}
