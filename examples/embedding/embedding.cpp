#include "hobbes/hobbes.H"

#include "linenoise-ng/linenoise.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/CommandLine.h"

#include "TH/TH.h"

#include <iostream>
#include <stdexcept>

int applyDiscreteWeighting(int x, int y) { return x * y; }

void bindTensorFuncs(hobbes::cc &ctx) {
  ctx.bind("FloatTensor_add", &THFloatTensor_add);
  ctx.bind("FloatTensor_sub", &THFloatTensor_sub);
  ctx.bind("FloatTensor_mul", &THFloatTensor_mul);
  ctx.bind("FloatTensor_new", &THFloatTensor_new);
  ctx.bind("FloatTensor_newWithSize1d", &THFloatTensor_newWithSize1d);
  ctx.bind("FloatTensor_storage", &THFloatTensor_storage);
  ctx.bind("FloatTensor_zero", &THFloatTensor_zero);
  ctx.bind("FloatTensor_zeros", &THFloatTensor_zeros);
  ctx.bind("FloatTensor_zerosLike", &THFloatTensor_zerosLike);

  ctx.bind("FloatStorage_new", &THFloatStorage_new);
  ctx.bind("FloatStorage_clearFlag", &THFloatStorage_clearFlag);
  ctx.bind("FloatStorage_copy", &THFloatStorage_copy);
  ctx.bind("FloatStorage_copyByte", &THFloatStorage_copyByte);
  ctx.bind("FloatStorage_copyChar", &THFloatStorage_copyChar);
  ctx.bind("FloatStorage_copyDouble", &THFloatStorage_copyDouble);
  ctx.bind("FloatStorage_copyFloat", &THFloatStorage_copyFloat);

  // THFile
  //  ctx.bind("THFile_readFloat", &THFile_readFloat);
  // ctx.bind("THFile_writeFloat", &THFile_writeFloat);

  // ctx.bind("THMemoryFile_longSize", &THMemoryFile_longSize);
  // ctx.bind("THMemoryFile_new", &THMemoryFile_new);
  // ctx.bind("THMemoryFile_newWithStorage", &THMemoryFile_newWithStorage);
  // ctx.bind("THMemoryFile_storage", &THMemoryFile_storage);

  // ctx.bind("THDiskFile_new", &THDiskFile_new);
}

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

  bindTensorFuncs(c);

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
