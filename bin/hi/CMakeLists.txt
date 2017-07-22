add_executable(hi
	evaluator.C
	funcdefs.C
	main.C
	www.C
	www.H
)

target_link_libraries(hi
  ${hobbes_libs}
  ncurses
  readline
  ${sys_libs})
