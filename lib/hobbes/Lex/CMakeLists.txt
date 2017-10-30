add_library(hobbesparser
    Parser.cpp
)

add_library(hobbeslex
    Lexer.cpp
)

target_include_directories(hobbesparser
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/Lex/
  )

target_include_directories(hobbeslex
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/Lex/
  )

install(TARGETS hobbeslex DESTINATION "lib")
install(TARGETS hobbesparser DESTINATION "lib")
