#include "hobbes/hobbes.H"

#include "linenoise/linenoise.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/CommandLine.h"

#include "TH/TH.h"

#include <iostream>
#include <stdexcept>

int applyDiscreteWeighting(int x, int y) { return x * y; }


namespace {

llvm::cl::OptionCategory HobbesEmbeddingCategory("Hobbes Embedding");
llvm::cl::extrahelp HobbesEmbeddingExtraHelp(R"(
   This is an example that demonstrates embedding of Hobbes into C++
)");
llvm::cl::opt<unsigned> VerbosityOption("verbosity", llvm::cl::init(2),
                                        llvm::cl::desc("Set the verbosity"),
                                        llvm::cl::cat(HobbesEmbeddingCategory));
}

int main(int argc, char *argv[]) {
  // Option Parsing
  llvm::cl::HideUnrelatedOptions(HobbesEmbeddingCategory);
  llvm::cl::SetVersionPrinter(
      []() { std::cout << "hobbes version 0.1" << std::endl; });
  llvm::cl::ParseCommandLineOptions(argc, argv, "embedding");

  // Create Hobbes Context
  hobbes::cc c;

  c.bind("applyDiscreteWeighting", &applyDiscreteWeighting);

  c.bind("dumpModule", memberfn(&hobbes::cc::dumpModule));
  c.bind("compiler", &c);

  
  char *read_line;
  while ((read_line = linenoise("> ")) != NULL) {
    auto line = std::string(read_line);
    if (line == ":q")
      break;

    try {
      c.compileFn<void()>("print(" + line + ")")();
    } catch (std::exception &ex) {
      std::cout << "*** " << ex.what();
    }

    linenoiseHistoryAdd(read_line);
    std::cout << std::endl;
    hobbes::resetMemoryPool();
  }

  return 0;
}
