add_library(linenoise
  linenoise.cpp
  )


target_include_directories(linenoise
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../)
