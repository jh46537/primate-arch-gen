#ifndef LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H
#define LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/CodeGen/ISDOpcodes.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG_TYPE "primate-bfu-coloring"

namespace llvm {

class PrimateBFUColoring : public PassInfoMixin<PrimateBFUColoring > {

public:
  // PrimateBFUColoring() {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager& AM);
  // void getAnalysisUsage(AnalysisUsage &AU) const;
  
  static char ID;

private:
  bool isBFU(Function &F);
  void colorSubBFUs(Function &F);
  ISD::NodeType opcodeToISD(unsigned int OP);
};

} // namespace llvm


#endif
