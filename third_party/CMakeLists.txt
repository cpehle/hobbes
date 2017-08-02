add_subdirectory(linenoise)

if(BUILD_LLVM)
add_subdirectory(llvm)
include_directories(llvm/include/)
add_compile_options(-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -frtti)
endif()
