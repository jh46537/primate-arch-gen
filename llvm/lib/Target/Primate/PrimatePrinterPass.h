#pragma once

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
  struct PrimatePrinterPass : public PassInfoMixin<PrimatePrinterPass> {
    PreservedAnalyses run(Module&, ModuleAnalysisManager&);
    static bool isRequired() { return true; }
  };
}
