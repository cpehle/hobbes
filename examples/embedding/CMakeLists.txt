cmake_minimum_required(VERSION 3.4.1)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs all)


add_executable(embedding embedding.cpp)

target_link_libraries(embedding hobbes ${llvm_libs})

