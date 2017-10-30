add_executable(hobbes-test
  Arrays.C
  Compiler.C
  Definitions.C
  Existentials.C
  Main.C
  Matching.C
  Net.C
  Objects.C
  Prelude.C
  Recursives.C
  Storage.C
  Structs.C
  TypeInf.C
  Variants.C
  LexerTest.cpp
  )

target_link_libraries(hobbes-test ${hobbes_libs} ${llvm_libs} ncurses ${sys_libs})
add_test(hobbes-test hobbes-test)
