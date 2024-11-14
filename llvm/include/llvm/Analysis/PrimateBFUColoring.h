#ifndef LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H
#define LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H

#include "llvm/ADT/SmallVector.h"
#include <cstddef>
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

class ISDOperation {
private:
  ISD::NodeType Opcode; 
  std::string  OPName;     // Operation Name
  unsigned int Complexity; // Complexity of ISD Operation's ISel Pattern
  
  SmallVector<std::string> Operands;
  // SmallVector<Instruction *> Dependencies;

public:
  ISDOperation(unsigned int OP, unsigned int Complexity);

  ISD::NodeType opcode()    { return Opcode; }
  size_t numOperands()      { return Operands.size(); }
  unsigned int complexity() { return Complexity; }

  void pushOperand(std::string OP) { Operands.push_back(OP); }
  void compInrc() { Complexity++; }
  void compDecr() { Complexity--; }

  // print the ISel pattern of the ISD operation to a raw ostream
  void print(raw_ostream &ROS) {
    ROS << "(" << OPName;
    for (auto &OP : Operands)
      ROS << " " << OP;
    ROS << ")";
  }

  void dump() {
    print(dbgs()); dbgs () << "\n";
  }
};

class PrimateBFUColoring : public PassInfoMixin<PrimateBFUColoring > {
private:
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
