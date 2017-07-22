add_executable(embedding embedding.cpp)

target_link_libraries(embedding ${hobbes_libs} linenoise LLVMSupport)

