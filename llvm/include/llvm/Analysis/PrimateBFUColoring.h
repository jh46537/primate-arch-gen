#ifndef LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H
#define LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H

#include "llvm/ADT/SmallVector.h"
#include <llvm/CodeGen/ISDOpcodes.h>
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include <llvm/IR/PassManager.h>
#include "llvm/IR/Type.h"
#include "llvm/IR/ValueMap.h"
#include <llvm/Pass.h>
#include "llvm/Support/Debug.h"
#include <llvm/Support/raw_ostream.h>
#include <cstddef>
#include <string>

#define DEBUG_TYPE "primate-bfu-coloring"

namespace llvm {

class ISDOperation {
private:
  ISD::NodeType Opcode; 
  std::string  OPName;     // Operation Name
  unsigned int Complexity; // Complexity of ISD Operation's ISel Pattern
                           // Essentially equal to he number of operands,
                           // plus the complexity of any dependencies
  SmallVector<std::string> Operands;

public:
  ISDOperation(unsigned int OP, unsigned int Complexity);

  ISD::NodeType opcode()    { return Opcode; }
  size_t numOperands()      { return Operands.size(); }
  unsigned int complexity() { return Complexity; }

  void pushOperand(std::string OP) { Operands.push_back(OP); }
  void compInrc(unsigned int C)    { Complexity += C; }

  // Print the ISel pattern of the ISD operation to a raw ostream. This isn't
  // intended to be read, so I'm not bothering with making the output pretty
  void print(raw_ostream &ROS) {
    if (Opcode != ISD::GlobalAddress) {
      ROS << "(";
      ROS << OPName;
    }
    
    for (auto &OP : Operands)
      ROS << " " << OP;
    
    if (Opcode != ISD::GlobalAddress) 
      ROS << ")";
  }

  void dump() {
    print(dbgs()); dbgs () << "\n";
  }
};

class PrimateBFUColoring : public PassInfoMixin<PrimateBFUColoring > {
private:
  ValueMap<Instruction *, ISDOperation *> *ISDOps;
  int ImmNum;

public:
  // PrimateBFUColoring() {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager& AM);
  // void getAnalysisUsage(AnalysisUsage &AU) const;
  
  static char ID;

private:
  bool isBFU(Function &F);
  void colorSubBFUs(Function &F);
  void createISDPatt(std::pair<Instruction *, ISDOperation *> &P);
  void createGEPPatt(std::pair<Instruction *, ISDOperation *> &P);

#ifdef _DEBUG
  // Print derived type of an operand. See DerivedTypes.h file.
  void printDerviedType(unsigned OPTy) {
    switch (OPTy) {
      ///< Arbitrary bit width integers
      case Type::IntegerTyID: LLVM_DEBUG(dbgs() << "IntegerTyID\n"); break;
      ///< Functions
      case Type::FunctionTyID: LLVM_DEBUG(dbgs() << "FunctionTyID\n"); break;
      ///< Pointers
      case Type::PointerTyID: LLVM_DEBUG(dbgs() << "PointerTyID\n"); break;
      ///< Structures
      case Type::StructTyID: LLVM_DEBUG(dbgs() << "StructTyID\n"); break;
      ///< Arrays
      case Type::ArrayTyID: LLVM_DEBUG(dbgs() << "ArrayTyID\n"); break;
      ///< Fixed width SIMD vector type
      case Type::FixedVectorTyID: LLVM_DEBUG(dbgs() << "FixedVectorTyID\n"); break;
      ///< Scalable SIMD vector type
      case Type::ScalableVectorTyID: LLVM_DEBUG(dbgs() << "ScalableVectorTyID\n"); break;
      ///< Typed pointer used by some GPU targets
      case Type::TypedPointerTyID: LLVM_DEBUG(dbgs() << "TypedPointerTyID\n"); break;
      ///< Target extension type
      case Type::TargetExtTyID: LLVM_DEBUG(dbgs() << "TargetExtTyID\n"); break;
      /// All other types
      default: LLVM_DEBUG(dbgs() << "Don't care about this type\n");
    }
  }
#endif
};

} // namespace llvm

#endif
