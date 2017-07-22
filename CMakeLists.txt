cmake_minimum_required(VERSION 3.4.1)
project(hobbes)
set (CMAKE_CXX_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# not sure how to select just the necessary components across all LLVM versions
# for now, pull in everything and hope that the linker will sort it out
# maybe this should be an OS/LLVM switch?
llvm_map_components_to_libnames(llvm_libs all)

set(HOBBES_MAIN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HOBBES_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(HOBBES_MAIN_INCLUDE_DIR ${HOBBES_MAIN_SRC_DIR}/include)
set(HOBBES_INCLUDE_DIR ${HOBBES_BINARY_DIR}/include)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

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

#install(TARGETS ${hobbes_libs} DESTINATION "lib")
#install(TARGETS hi hog DESTINATION "bin")
#install(DIRECTORY "include/hobbes" DESTINATION "include")
