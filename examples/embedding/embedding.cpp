#include <iostream>
#include <stdexcept>
#include <hobbes/hobbes.H>

#include "linenoise/linenoise.h"

int main() {
  hobbes::cc c;

  char * read_line;
  while ((read_line = linenoise("> ")) != NULL) {
    auto line = std::string(read_line);    
    if (line == ":q") break;

    try {
      c.compileFn<void()>("print(" + line + ")")();
    } catch (std::exception& ex) {
      std::cout << "*** " << ex.what();
    }

    linenoiseHistoryAdd(read_line);
    std::cout << std::endl;
    linenoiseFree(read_line);
    hobbes::resetMemoryPool();
  }

  return 0;
}
