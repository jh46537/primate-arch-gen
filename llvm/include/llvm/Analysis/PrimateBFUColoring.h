#ifndef LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H
#define LLVM_ANALYSIS_PRIMATE_PRIMATEBFUCOLORING_H

#include "llvm/ADT/SmallVector.h"
#include <algorithm>
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
#include <memory>
#include <string>

#define DEBUG_TYPE "primate-bfu-coloring"

namespace llvm {

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
  std::string Pattern;
  
  // Print the ISel pattern of the ISD operation to a raw ostream. This isn't
  // intended to be read, so I'm not bothering with making the output pretty
  void print(raw_ostream &ROS);
  void dump();

  inline ISD::NodeType opcode()           { return Opcode;          }
  inline std::string name()               { return OPName;          }
  inline size_t numOperands()             { return Operands.size(); }
  inline unsigned int complexity()        { return Complexity;      }
  inline void pushOperand(std::string OP) { Operands.push_back(OP); }
  inline void compInrc(unsigned int C)    { Complexity += C;        }
};

// I converted this from struct -> class because I wanted to make a private constructor
// so that in the Pass Run func I could be all fancy and have a "create" function that
// would return nullptr if you try to create BFUPatternInfo on a function that isn't a
// BFU. However, because of the god damn, yaml mapping, it didn't work, and now I'm too
// lazy to turn this back into a struct. Why to live.
class BFUPatternInfo {
public:
  std::string BFUname;

  // Why didn't I make some kind of std::pair, or just create a yaml template
  // for ISDOperation? That's because Doing so broke the yaml template for this
  // class and it makes me want to commit self harm
  SmallVector<std::string> InterfaceList;
  SmallVector<ISDOperation *> InstrList;
  
  BFUPatternInfo() {
    BFUname = "";
    InterfaceList.push_back("io");
  }
  
  // DO NOT CALL THIS FUCKING CONSTRUCTOR. This is supposed to be a private constructor.
  // But making this private causes the yaml template to shit the bed.
  BFUPatternInfo(Function &F, MDNode *PMD) {
    auto *MDOP = dyn_cast<MDString>(PMD->getOperand(1));  // BFU Name is always operand 1
    BFUname = std::string(MDOP->getString());
    InterfaceList.push_back("io");
  }

  static std::shared_ptr<BFUPatternInfo> create(Function &F) {
    MDNode* MD = F.getMetadata("primate");
    if (MD && 
        dyn_cast<MDString>(MD->getOperand(0))->getString() == "blue") {
      LLVM_DEBUG(dbgs() << "Found BFU Function:\n");
      return std::make_shared<BFUPatternInfo>(F, MD);
    }
    LLVM_DEBUG(dbgs() << F.getName() << "is NOT a BFU function\n\n");
    return nullptr;
  }
};

class PrimateBFUColoring : public PassInfoMixin<PrimateBFUColoring > {
  ValueMap<Instruction *, ISDOperation *> *ISDOperationMap;
  int ImmNum;

public:
  PrimateBFUColoring() {} 
  PreservedAnalyses run(Function &F, FunctionAnalysisManager& AM);
  void getAnalysisUsage(AnalysisUsage &AU) const;

  static char ID;

private:
  void createBFUPatterns(Function &F, BFUPatternInfo *BPI);
  void processISD(std::pair<Instruction *, ISDOperation *> &P);
  void processGEP(std::pair<Instruction *, ISDOperation *> &P);

#ifdef _DEBUG
  // Print derived type of an operand. See DerivedTypes.h file. I have no idea if
  // this ifdef _DEBUG even fucking works.
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

// This has been both a blessing and a curse. LLVM Yaml is really easy to work with,
// however you have to tune your classes to output to it easily. For example, all of
// fields would usually be private in a class, but since we want to print them in the
// Yaml output, we have them as public. What's that? Why don't I make BFUPatternInfo
// a struct so that everything is public by default?
namespace yaml {

template<>
struct MappingTraits<BFUPatternInfo> {
  static void mapping(IO &io, BFUPatternInfo &info) {
    io.mapRequired("bfu_name",     info.BFUname);
    io.mapRequired("interfaces",   info.InterfaceList);
    io.mapRequired("instructions", info.InstrList);
  }
};

template<>
struct MappingTraits<ISDOperation> {
  static void mapping(IO &io, ISDOperation &info) {
    io.mapRequired("sub_bfu_instruction", info.InstName);
    io.mapRequired("pattern",             info.Pattern);
  }
};

template <>
struct SequenceTraits<SmallVector<ISDOperation *>> {
  static size_t size(IO &io, SmallVector<ISDOperation *> &list) { 
    return list.size(); 
  }
  
  static 
  ISDOperation &element(IO &io, SmallVector<ISDOperation *> &list, 
                        size_t index) {
    return *list[index];
  }
};

} // namespace yaml

} // namespace llvm

#endif
