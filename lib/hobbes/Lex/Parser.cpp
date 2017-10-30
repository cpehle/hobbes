#include "hobbes/Lex/Token.h"

namespace hobbes {

void parseImportDeclaration() {}

void parseTypeDeclaration() {}

void parseClassDeclaration() {}

/*
  def : importdef
      | tydef
      | vartybind
      | classdef
      | instdef
      | [id]* '=' l0expr
 */
void parseDeclaration() {
  switch (token().Kind) {
    tok::import : parseImportDeclaration();
    break;
    tok::type : tok::data : parseTypeDeclaration();
    break;
    tok::class : parseClassDeclaration();
    break;
    tok::instance : parseInstanceDeclaration();
    break;
  }
}
}
