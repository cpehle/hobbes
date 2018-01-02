add_executable(hit
	evaluator.C
	funcdefs.C
	main.C
	www.C
	www.H
)

target_link_libraries(hit
  ${hobbes_libs}
  ncurses
  linenoise
  TH
  ${sys_libs})

install(TARGETS hit DESTINATION "bin")

