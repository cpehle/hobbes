add_library(hobbesparse
  data.C
  grammar.C
  lalr.C
  parser.C
  terminal.C
  )

target_include_directories(hobbesparse
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/parse/
  )

