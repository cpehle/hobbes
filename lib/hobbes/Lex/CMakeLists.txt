add_library(hobbeslex
    Lexer.cpp
)

target_include_directories(hobbeslex
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/Lex/
  )

install(TARGETS hobbeslex DESTINATION "lib")
