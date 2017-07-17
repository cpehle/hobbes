
all: binding.cpp
	c++ -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -pthread -std=c++14 -I ../../include/ -I /usr/local/opt/llvm@3.7/lib/llvm-3.7/include/ binding.cpp -o binding -L ../../build/ -lhobbes -ldl -lncurses -lz -L /usr/local/opt/llvm@3.7/lib/llvm-3.7/lib/ `llvm-config-3.7 --libs x86asmparser x86codegen x86 mcjit passes`
