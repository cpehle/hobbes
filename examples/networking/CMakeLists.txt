add_executable(net
  net.cpp
  )

target_link_libraries(net ${hobbes_libs} ${sys_libs})

