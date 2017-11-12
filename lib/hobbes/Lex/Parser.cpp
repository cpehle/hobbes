#include "hobbes/Lex/Parser.h"
#include "hobbes/Lex/Lexer.h"
#include "hobbes/Lex/Token.h"
#include "hobbes/lang/expr.H"
#include "hobbes/lang/module.H"
#include "hobbes/lang/pat/pattern.H"
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

Parser::Parser(class Lexer &Lex) : Lexer(Lex) {
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
  operators.insert({"in", {30, Assoc::left}});

  operators.insert({"+", {40, Assoc::left}});
  operators.insert({"-", {40, Assoc::left}});
  operators.insert({"++", {40, Assoc::left}});

  operators.insert({"*", {50, Assoc::left}});
  operators.insert({"/", {50, Assoc::left}});
  operators.insert({"%", {50, Assoc::left}});
}

void Parser::ConsumeToken() { Lexer.LexToken(Tok); }

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

std::shared_ptr<MImport> Parser::ParseImportStatement() {
  ConsumeToken();
  llvm::Twine QualID;
  ParseQualifiedId(QualID);
  std::cout << QualID.str();
  return std::make_shared<MImport>(
      "", QualID.str(), LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
}

bool ParseNameSeq() {}

bool ParseTypeDeclaration() { ParseNameSeq(); }

auto Parser::GetBinopPrecedence(Token& tok) -> std::pair<int, Parser::Assoc> {  
  if (tok.getKind() <= tok::comment_single ) {
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

ExprPtr Parser::ParseParenExpr() {
  ConsumeToken();
  auto Val = Parser::ParseExpression();
  if (!Val) {
    return nullptr;
  }
  if (Tok.isNot(tok::r_paren)) {
    // Error;
  }
  ConsumeToken();
  return Val;
}

/// refutablep: "boolV"
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
///           | "{" recpatfields "}"
///           | id
PatternPtr Parser::ParsePattern() {

  if (Tok.is(tok::l_square)) {

  } else if (Tok.is(tok::pipe)) {
    ConsumeToken(); // consume '|'
    if (Tok.is(tok::identifier)) {
      auto Id = Tok.getLiteralData();
      ConsumeToken();
      if (Tok.is(tok::equal)) {
        auto Pattern = Parser::ParsePattern();
        if (Tok.isNot(tok::pipe)) {
        }
        return std::make_shared<MatchVariant>(
            Id, Pattern, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
      }

      if (Tok.isNot(tok::pipe)) {
        // Error
      }

      return std::make_shared<MatchVariant>(
          Id, std::make_shared<MatchLiteral>(
                  std::make_shared<Unit>(
                      LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
                  LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
          LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
    } else if (Tok.is(tok::numeric_constant)) {

    } else {
      // Error
    }
  } else if (Tok.is(tok::l_paren)) {

  } else if (Tok.is(tok::l_brace)) {

  } else if (Tok.is(tok::identifier)) {
    auto Id = Tok.getLiteralData();
    ConsumeToken();
    // Actions.ActOnPatternVar(Tok.getLiteralData());
    return std::make_shared<MatchAny>(
        Id, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  }

  return nullptr;
}

PatternPtr Parser::ParseIrrefutablePattern() {
  if (Tok.is(tok::identifier)) {
    auto Id = Tok.getLiteralData();
    ConsumeToken();
    return std::make_shared<MatchAny>(
        Id, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::l_paren)) {
    ConsumeToken(); // consume '('
    std::vector<PatternPtr> Patterns;
    while (true) {
      auto Pattern = Parser::ParsePattern();
      if (!Pattern) {
        // Error
      }

      Patterns.push_back(Pattern);

      if (Tok.is(tok::l_paren)) {
        break;
      }

      if (Tok.isNot(tok::colon)) {
        // Error
      }
      ConsumeToken();
    }
    ConsumeToken(); // consume ')'
    // return Action.ActOnIrrefutablePattern
    return pickNestedPat(&Patterns,
                         LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  } else if (Tok.is(tok::l_brace)) {
    return nullptr;
  } else {
    // Error
    return nullptr;
  }
}

ExprPtr Parser::ParseIdentifierExpr() {
  auto IdName = Tok.getLiteralData();

  ConsumeToken(); // eat the identifier

  if (Tok.isNot(tok::l_paren)) {
    // this is a variable
    return std::make_shared<Var>(
        IdName, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  }

  // this is a call
  ConsumeToken(); // eat '('
  // llvm::SmallVector<ExprPtr, 10> Args;
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

      if (Tok.isNot(tok::colon)) {
        // Error
        // return LogError("Expected ')' or ',' in argument list");
      }
      ConsumeToken(); // eat ','
    }
  }

  // eat ')'
  ConsumeToken();
  return std::make_shared<App>(
      var(IdName, LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))), Args,
      LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  // return Actions.ActOnApp();
}

/// match expression:
///    'match' expr 'with' [ '|' pattern row ]+
///
/// pattern row:
///     pattern ['where' expr] '->' expr
ExprPtr Parser::ParseMatchExpr() {
  ConsumeToken(); // consume 'match'

  // expressions to be matched
  llvm::SmallVector<ExprPtr, 5> MatchExprs;

  while (true) {
    auto Match = Parser::ParseExpression();
    if (!Match) {
      // Error
      return nullptr;
    }
    MatchExprs.push_back(Match);
    if (Tok.is(tok::kw_where)) {
      break;
    }
  }
  // consume 'where'
  ConsumeToken();

  // parse pattern rows
  llvm::SmallVector<std::shared_ptr<PatternRow>, 10> PatternRows;

  while (Tok.is(tok::pipe)) {
    // llvm::SmallVector<std::shared_ptr<Pattern>, 10> Patterns;
    std::vector<std::shared_ptr<Pattern>> Patterns;
    while (true) {
      auto Pattern = Parser::ParsePattern();
      if (!Pattern) {
        // Error
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
        // Error
      }
    }
    if (Tok.isNot(tok::arrow)) {
      // Error
    }
    ConsumeToken(); // consume arrow
    auto PatternRowBody = Parser::ParseExpression();
    if (!PatternRowBody) {
      // Error
    }
    PatternRows.push_back(
        std::make_shared<PatternRow>(Patterns, WhereExpr, PatternRowBody));
  }
  // Actions.ActOnMatch(MatchExpressions,
  // Actions.ActOnPatternRows(PatternRows));

  // compileMatch(compiler, MatchExprs, normPatternRules(PatternRows, ), );
}

/// parse a let expression
///
///
ExprPtr Parser::ParseLetExpr() {
  ConsumeToken(); // consume let

  llvm::SmallVector<LetBinding, 10> Bindings;

  while (true) {
    auto Pattern = Parser::ParseIrrefutablePattern();

    if (Tok.isNot(tok::equal)) {
      //      return LogError("Expected equal");
    }
    ConsumeToken();
    auto Exp = Parser::ParseExpression();
    Bindings.push_back(std::pair<PatternPtr, ExprPtr>{Pattern, Exp});

    if (Tok.is(tok::kw_in)) {
      break;
    }

    if (Tok.isNot(tok::semi)) {
    }
    ConsumeToken();

    if (Tok.is(tok::kw_in)) {
      break;
    }
  }
  ConsumeToken(); // consume "in"

  auto Body = Parser::ParsePrimaryExpression();
  if (!Body) {
    return nullptr;
  }

  // return Actions.ActOnLet(Bindings, Body);
  // compileNestedLetMatch(, Bindings, Body, LexicallyAnnotated::make(Pos(0,0),
  // Pos(0,0))));
  return nullptr;
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
ExprPtr Parser::ParseIfExpr() {
  ConsumeToken(); // consume 'if'

  auto Cond = ParseExpression();
  if (!Cond) {
    return nullptr;
  }

  if (Tok.isNot(tok::kw_then)) {
    // Error
    // return LogError("expected then");
  }
  ConsumeToken(); // consume 'then'

  auto Then = ParseExpression();
  if (!Then) {
    return nullptr;
  }

  if (Tok.isNot(tok::kw_else)) {
    // Error
    // return LogError("expected else");
  }
  ConsumeToken(); // consume 'else'

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

ExprPtr Parser::ParseNumericConstant() {
  long Lit;
  Tok.getLiteralData().getAsInteger(10, Lit);
  ConsumeToken();
  return std::make_shared<Int>(Lit,
                               LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
}

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

ExprPtr Parser::ParseBinopRHS(ExprPtr LHS, int MinPrec) {
  while (1) {    
    // If this was an operator token we should handle the error
    // here
    auto [NextTokenPrec, Assoc] = GetBinopPrecedence(Tok);
    
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
      auto [NextTokenPrec, Assoc] = GetBinopPrecedence(Tok);
    }

    auto OldLHS = std::move(LHS);
    LHS = std::make_shared<App>(
        var(Tok.getLiteralData().str(),
            LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0))),
        list(std::move(OldLHS), std::move(RHS)),
        LexicallyAnnotated::make(Pos(0, 0), Pos(0, 0)));
  }
}

/// expression
///   ::= primary binoprhs
///
ExprPtr Parser::ParseExpression() {
  auto LHS = Parser::ParsePrimaryExpression();
  if (!LHS) {
    return nullptr;
  }
  return Parser::ParseBinopRHS(std::move(LHS), 0);
}

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
auto Parser::ParseType() -> bool {
  switch (Tok.getKind()) {
  case tok::identifier:
    break;
  case tok::less:
    break;
  case tok::l_square:
    Parser::ConsumeToken();
    if (Tok.is(tok::r_square)) {

    } else if (Tok.is(tok::colon)) {

    } else {
    }
    break;
  case tok::l_paren:
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

///    id [l1mtype]+
///    "{" l1mtype "*" lm1type "}"
///    "{" id ":" l1mtype "*" l1mtypee "}"
///    "(" l1mtype "*" l1mtype ")"
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
auto Parser::ParseTypePredicate() -> bool {}

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

bool Parser::ParseModuleDefs() {
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
      // Lexer.LexToken();
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
    std::cout << "parse module header" << std::endl;
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
