cmake_minimum_required(VERSION 3.4.1)
project(hobbes)
set (CMAKE_CXX_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

if (${LLVM_PACKAGE_VERSION} VERSION_LESS "3.6")
  set(jit_lib jit)
else()
  set(jit_lib mcjit)
endif()
# 'x86' module should have covered 'x86asmparser' and 'x86codegen' already, but using 'x86' alone didn't work
llvm_map_components_to_libnames(llvm_libs x86asmparser x86codegen x86 ipo ${jit_lib})

set(HOBBES_MAIN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HOBBES_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(HOBBES_MAIN_INCLUDE_DIR ${HOBBES_MAIN_SRC_DIR}/include)
set(HOBBES_INCLUDE_DIR ${HOBBES_BINARY_DIR}/include)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories(BEFORE
  ${HOBBES_MAIN_INCLUDE_DIR}
  ${HOBBES_INCLUDE_DIR}
  )

set(hobbes_libs hobbesdb hobbeseval hobbesevents hobbesipc hobbeslang hobbesparse hobbesread hobbesutil hobbesboot)

if(UNIX AND NOT APPLE)
   set(LINUX TRUE)
 endif()

if(APPLE)
  set(sys_libs pthread dl)
endif()
if(LINUX)
  set(sys_libs pthread dl rt)
endif()

enable_testing()
add_subdirectory("test")
add_subdirectory("include")
add_subdirectory("lib")
add_subdirectory("bin")
add_subdirectory("examples")
add_subdirectory("third_party")



