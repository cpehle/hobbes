#include "hobbes/hobbes.H"
#include "linenoise/linenoise.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <stdexcept>

int applyDiscreteWeighting(int x, int y) { return x * y; }

int main(int argc, char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "embedding");
  // hobbes context  
  hobbes::cc c;
  c.bind("applyDiscreteWeighting", &applyDiscreteWeighting);

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
