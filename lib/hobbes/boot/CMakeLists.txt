add_library(hobbesboot
  gen/boot.C
  )

target_link_libraries(hobbesboot
  hobbeseval
  hobbesread
  )

target_include_directories(hobbesboot
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/boot/gen
  )

