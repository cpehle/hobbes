add_executable(unreliable_storage unreliable_storage.cpp)
target_link_libraries(unreliable_storage ${hobbes_libs} ${sys_libs})

add_executable(reliable_storage reliable_storage.cpp)
target_link_libraries(reliable_storage ${hobbes_libs} ${sys_libs})
