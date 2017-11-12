#pragma once
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Diagnostic.h"

namespace hobbes {

class CompilerInstance {

  /// The diagnostics engine instance.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics;
  /// The file manager.
  llvm::IntrusiveRefCntPtr<clang::FileManager> FileMgr;
  
};
}
