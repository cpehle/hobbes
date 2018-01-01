include_directories(TH)
add_subdirectory(TH)

add_subdirectory(linenoise)

# add_subdirectory(nanogui)

if(BUILD_LLVM)
add_subdirectory(llvm)
include_directories(llvm/include/)
add_compile_options(-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -frtti)
endif()
