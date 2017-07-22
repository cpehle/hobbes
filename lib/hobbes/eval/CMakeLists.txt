add_library(hobbeseval
  cc.C
  cexpr.C
  cmodule.C
  ctype.C
  func.C
  funcdefs.C
  jitcc.C
  )

target_link_libraries(hobbeseval
  hobbeslang
  hobbesipc
  hobbesdb
  hobbesutil
  hobbesboot
  ${llvm_libs}
  )

target_include_directories(hobbeseval
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/eval/
  )
