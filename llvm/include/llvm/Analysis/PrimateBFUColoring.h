#ifndef LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H
#define LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <llvm/CodeGen/ISDOpcodes.h>
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include <llvm/IR/PassManager.h>
#include "llvm/IR/ValueMap.h"
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

#define DEBUG_TYPE "primate-bfu-coloring"

namespace llvm {

class PrimateBFUColoring : public PassInfoMixin<PrimateBFUColoring > {
private:
  struct ISDOperation {
    ISD::NodeType Opcode; // Opcode
    std::string OPName;     // Operation Name
    // ValueMap<Instruction *, StringRef> Dependencies;
    
    SmallVector<std::string> Operands;
    SmallVector<Instruction *> Dependencies;

    std::string dump() {
      std::string OperationStr;
      raw_string_ostream OpStrOut(OperationStr);
      OpStrOut << "(" << OPName;
      
      LLVM_DEBUG(dbgs() << "ISD Node has " << Operands.size() << " Operands:\n");
      for (auto &OP : Operands) {
        LLVM_DEBUG(dbgs() << " > " << OP << "\n");
        OpStrOut << " " << OP;
      }

      OpStrOut << ")";

      return OperationStr;
    }
  };

  ValueMap<Instruction *, ISDOperation *> *ISDOps;

public:
  // PrimateBFUColoring() {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager& AM);
  // void getAnalysisUsage(AnalysisUsage &AU) const;
  
  static char ID;

private:
  bool isBFU(Function &F);
  void colorSubBFUs(Function &F);
  void opcodeToISD(unsigned int OP, ISDOperation *ISDOP);
};

} // namespace llvm

#endif
