add_library(hobbesipc
  nbindings.C
  net.C
  prepl.C
  process.C
  procman.C
)

target_include_directories(
  hobbesipc
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/ipc/
  )
