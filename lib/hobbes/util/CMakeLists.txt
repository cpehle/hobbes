add_library(hobbesutil
  lannotation.C
  region.C
  str.C
  )

target_include_directories(hobbesutil
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/util/
  )
  
install(TARGETS hobbesutil DESTINATION "lib")
