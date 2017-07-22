add_subdirectory(pgen/)

add_library(hobbesread
  parser.C
  pgen/hexpr.lex.C
  pgen/hexpr.parse.C
  )

target_link_libraries(hobbesread
  hobbesparse
  hobbeslang
  hobbesutil
  )

target_include_directories(hobbesread
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/read
  )

install(TARGETS hobbesread DESTINATION "lib")

