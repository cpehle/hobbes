#pragma once

#include "hobbes/Lex/Lexer.h"
#include "hobbes/lang/type.H"
#include "hobbes/lang/module.H"
#include "hobbes/lang/expr.H"
#include "hobbes/lang/pat/pattern.H"

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/SourceMgr.h>
#include <memory>

namespace hobbes {

class Parser {
  enum class Assoc {
      right,
      left,   
  };  
  llvm::StringMap<std::pair<int, Assoc>> operators;
  
public:
  Parser(Lexer& lexer);  
  Lexer Lexer;
  /// Current Token we are looking at
  Token Tok;
  bool ParseModule();
  bool ParseModuleDefs();
  bool ParseClassDef();
  std::shared_ptr<MImport> ParseImportStatement();
  bool ParseType();
  bool ParseTypePredicate();
  bool ParseQualifiedId(llvm::Twine& QualID);  
  void ConsumeToken();

  std::pair<int, Assoc> GetBinopPrecedence(Token& tok);  
  ExprPtr ParseBinopRHS(ExprPtr LHS, int MinPrec);
  ExprPtr ParsePrimaryExpression();
  ExprPtr ParseParenExpr();
  ExprPtr ParseIfExpr();
  ExprPtr ParseIdentifierExpr();
  ExprPtr ParseExpression();
  ExprPtr ParseMatchExpr();
  ExprPtr ParseLetExpr();
  ExprPtr ParseNumericConstant();
  
  PatternPtr ParsePattern();
  PatternPtr ParseIrrefutablePattern();
};
}
