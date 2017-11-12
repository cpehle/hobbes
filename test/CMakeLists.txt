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
  Lexer.cpp
  Parser.cpp
  )

target_link_libraries(hobbes-test ${hobbes_libs} ${llvm_libs} "/usr/local/lib/llvm-3.7/lib/libclangBasic.a" "/usr/local/lib/llvm-3.7/lib/libclangFrontend.a" "/usr/local/lib/llvm-3.7/lib/libclangEdit.a" "/usr/local/lib/llvm-3.7/lib/libclangLex.a" ncurses ${sys_libs}) 
add_test(hobbes-test hobbes-test)
