add_executable(hit
	evaluator.C
	funcdefs.C
	main.C
	www.C
	www.H
)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

target_link_libraries(hit
  ${hobbes_libs}
  ncurses
  linenoise
  TH
  glfw
  ${GLFW_LIBRARIES}
  ${sys_libs})

install(TARGETS hit DESTINATION "bin")

