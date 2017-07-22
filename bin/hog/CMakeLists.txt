add_subdirectory(boot/gen)

add_executable(hog
  batchrecv.C
  batchsend.C
  local.C
  main.C
  session.C
  boot/gen/boot.C
  )

target_link_libraries(hog
  ${hobbes_libs}
  ncurses
  ${sys_libs})
install(TARGETS hog DESTINATION "bin")
