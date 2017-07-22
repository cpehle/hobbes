add_library(hobbesdb
  bindings.C
  file.C
  series.C
  signals.C
  )

target_include_directories(hobbesdb
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/db/
  )

install(TARGETS hobbesdb DESTINATION "lib")
