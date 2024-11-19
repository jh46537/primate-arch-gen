#ifndef LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H
#define LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H

#include "llvm/ADT/SmallVector.h"
#include <llvm/CodeGen/ISDOpcodes.h>
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include <llvm/IR/PassManager.h>
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/ValueMap.h"
#include <llvm/Pass.h>
#include "llvm/Support/Debug.h"
#include "llvm/Support/YAMLTraits.h"
#include <llvm/Support/raw_ostream.h>
#include <cstddef>
#include <string>

#define DEBUG_TYPE "primate-bfu-coloring"

namespace llvm {

struct BFUPatternInfo {
  std::string name;
  SmallVector<std::string> interfaces;
  SmallVector<std::string> instructions;
  std::string pattern;

  BFUPatternInfo() { name = ""; }
  
  BFUPatternInfo(MDNode* MD, BasicBlock *BB, std::string Pattern) {
    for (unsigned int I = 0; I < MD->getNumOperands(); I++) {
      auto *MDOP = dyn_cast<MDString>(MD->getOperand(I));
      if (!MDOP)
        break;

      auto CurrOP = MDOP->getString();
      switch (I) {
        case 0:
          continue;
        
        case 1:
          name = std::string(CurrOP);
          break;

        default:
          instructions.push_back(std::string(CurrOP) + 
                                 std::string(BB->getName()));
      }
    }
    interfaces.push_back("io");
    pattern = Pattern;
  }

  void dump() {
    dbgs() << "Entry Name: " << name << "\n";
    
    dbgs() << "Interfaces: ";
    for (auto &I : interfaces)
      dbgs() << I << ", ";
    dbgs() << "\n";
    
    dbgs() << "Instructions: ";
    for (auto &I : instructions)
      dbgs() << I << ", ";
    dbgs() << "\n";
    
    dbgs() << "Pattern: " << pattern << "\n";
  }

};

template<>
struct yaml::MappingTraits<BFUPatternInfo> {
  static void mapping(IO &io, BFUPatternInfo &info) {
    io.mapRequired("name",         info.name);
    io.mapRequired("interfaces",   info.interfaces);
    io.mapRequired("instructions", info.instructions);
    io.mapRequired("patterns",     info.pattern);
  }
};

class ISDOperation {
  ISD::NodeType Opcode; 
  std::string  OPName;     // Operation Name
  unsigned int Complexity; // Complexity of ISD Operation's ISel Pattern
                           // Essentially equal to he number of operands,
                           // plus the complexity of any dependencies
  SmallVector<std::string> Operands;

public:
  ISDOperation(unsigned int OP, unsigned int Complexity);

  std::string InstName;

  inline ISD::NodeType opcode()           { return Opcode; }
  inline std::string name()               { return OPName; }
  inline size_t numOperands()             { return Operands.size(); }
  inline unsigned int complexity()        { return Complexity; }
  inline void pushOperand(std::string OP) { Operands.push_back(OP); }
  inline void compInrc(unsigned int C)    { Complexity += C; }

  // Print the ISel pattern of the ISD operation to a raw ostream. This isn't
  // intended to be read, so I'm not bothering with making the output pretty
  void print(raw_ostream &ROS);
  void dump();
};

class PrimateBFUColoring : public PassInfoMixin<PrimateBFUColoring > {
private:
  ValueMap<Instruction *, ISDOperation *> *ISDOperationMap;
  SmallVector<BFUPatternInfo*> BFUPatterns;
  int ImmNum;

public:
  // PrimateBFUColoring() {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager& AM);
  // void getAnalysisUsage(AnalysisUsage &AU) const;

  static char ID;

private:
  void createBFUPatterns(Function &F);
  void processISD(std::pair<Instruction *, ISDOperation *> &P);
  void processGEP(std::pair<Instruction *, ISDOperation *> &P);

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
