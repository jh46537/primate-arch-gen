#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/PrimateBFUColoring.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"

using namespace llvm;


PreservedAnalyses PrimateBFUColoring::run(Function &F, 
                                          FunctionAnalysisManager& AM) {
  LLVM_DEBUG(dbgs() << "\n==========================\n"
                    << "Here in PrimateBFUColoring\n\n" 
                    << "Current Function: " << F.getName() << "\n");
  
  if (isBFU(F))
    colorSubBFUs(F);
  else
    dbgs() << F.getName() << "is NOT a BFU function\n\n";

  LLVM_DEBUG(dbgs() << "Dump current function:\n"; F.dump());

  LLVM_DEBUG(dbgs() << "\nBye from PrimateBFUColoring" 
                    << "\n==========================\n\n");

  return PreservedAnalyses::all();
}

bool PrimateBFUColoring::isBFU(Function &F) {
  SmallVector<std::pair<unsigned int, MDNode *>> MDs;
  F.getAllMetadata(MDs);
  for (const auto &MD : MDs) {
    MDNode *MDN = MD.second;
    
    LLVM_DEBUG(dbgs() << "Metadata entry " << MD.first << ": "; MDN->dump());

    for (const auto &OP :MDN->operands()) {
      if (OP.equalsStr("blue"))
        return true;
    }
  }
  return false;
}

void PrimateBFUColoring::colorSubBFUs(Function &F) {
  dbgs() << F.getName() << "is a BFU function!\n";
  for (auto &BB : F) {
    dbgs() << " >>> Basic Block: " << BB.getName() << "\n";
    for (auto &I : BB) {
      dbgs() << " >>>     Current instruction: ";
      I.dump();

      auto CurrOP = opcodeToISD(I.getOpcode());
      dbgs() << " >>>     Operands:\n";
      for (auto &OP : I.operands()) {
        dbgs() << " >>>    ";
        OP->getType()->dump();
        dbgs() << " >>>    Type ID: " << OP->getType()->getTypeID() << "\n";
      }
      
      if (CurrOP == ISD::DELETED_NODE)
        continue;
      if (CurrOP == ISD::BR || CurrOP == ISD::RETURNADDR)
       break;
      
      // for (const auto &U : I.uses()) {
      //   dbgs() << "Use type: ";
      //   U->getType()->dump();
      // }

      dbgs() << " >>> Add metadata... \n";
      I.setMetadata("collapse-to-BFU-call", nullptr);
      dbgs() << " >>> Dump Instr after adding metadata:\t";
      I.dump();
    }
    dbgs() << "\n\n";
  }
}

ISD::NodeType PrimateBFUColoring::opcodeToISD(unsigned int OP) {
  LLVM_DEBUG(dbgs() << " >>>     Current Opcode: ");
  switch (OP) {
    case Instruction::Ret: LLVM_DEBUG(dbgs() << "return\n");  return ISD::RETURNADDR;
    case Instruction::Br:  LLVM_DEBUG(dbgs() << "branch\n");  return ISD::BR;
  // case Switch: return "switch";
  // case IndirectBr: return "indirectbr";
  // case Invoke: return "invoke";
  // case Resume: return "resume";
  // case Unreachable: return "unreachable";
  // case CleanupRet: return "cleanupret";
  // case CatchRet: return "catchret";
  // case CatchPad: return "catchpad";
  // case CatchSwitch: return "catchswitch";
  // case CallBr: return "callbr";

  // Standard unary operators...
  // case FNeg: return "fneg";

    // Standard binary operators...
    case Instruction::Add:  LLVM_DEBUG(dbgs() << "add\n");  return ISD::ADD;
    case Instruction::FAdd: LLVM_DEBUG(dbgs() << "fadd\n"); return ISD::FADD;
    case Instruction::Sub:  LLVM_DEBUG(dbgs() << "sub\n");  return ISD::SUB;
    case Instruction::FSub: LLVM_DEBUG(dbgs() << "fsub\n"); return ISD::FSUB;
    case Instruction::Mul:  LLVM_DEBUG(dbgs() << "mul\n");  return ISD::MUL;
    case Instruction::FMul: LLVM_DEBUG(dbgs() << "fmul\n"); return ISD::MUL;
    case Instruction::UDiv: LLVM_DEBUG(dbgs() << "udiv\n"); return ISD::UDIV;
    case Instruction::SDiv: LLVM_DEBUG(dbgs() << "sdiv\n"); return ISD::SDIV;
    case Instruction::FDiv: LLVM_DEBUG(dbgs() << "fdiv\n"); return ISD::FDIV;
    case Instruction::URem: LLVM_DEBUG(dbgs() << "urem\n"); return ISD::UREM;
    case Instruction::SRem: LLVM_DEBUG(dbgs() << "srem\n"); return ISD::SREM;
    case Instruction::FRem: LLVM_DEBUG(dbgs() << "frem\n"); return ISD::FREM;

  // Logical operators...
    case Instruction::And: LLVM_DEBUG(dbgs() << "and\n"); return ISD::AND;
    case Instruction::Or : LLVM_DEBUG(dbgs() << "or\n");  return ISD::OR;
    case Instruction::Xor: LLVM_DEBUG(dbgs() << "xor\n"); return ISD::XOR;

  // Memory instructions...
  // case Alloca:        return "alloca";
    case Instruction::Load:  LLVM_DEBUG(dbgs() << "load\n"); return ISD::LOAD;
    case Instruction::Store: LLVM_DEBUG(dbgs() << "store\n"); return ISD::STORE;
  // case AtomicCmpXchg: return "cmpxchg";
  // case AtomicRMW:     return "atomicrmw";
  // case Fence:         return "fence";
    case Instruction::GetElementPtr: LLVM_DEBUG(dbgs() << "gep is pain\n"); return ISD::DELETED_NODE;

  // Convert instructions...
  // case Trunc:         return "trunc";
  // case ZExt:          return "zext";
  // case SExt:          return "sext";
  // case FPTrunc:       return "fptrunc";
  // case FPExt:         return "fpext";
  // case FPToUI:        return "fptoui";
  // case FPToSI:        return "fptosi";
  // case UIToFP:        return "uitofp";
  // case SIToFP:        return "sitofp";
  // case IntToPtr:      return "inttoptr";
  // case PtrToInt:      return "ptrtoint";
  // case BitCast:       return "bitcast";
  // case AddrSpaceCast: return "addrspacecast";

  // Other instructions...
  // case ICmp:           return "icmp";
  // case FCmp:           return "fcmp";
  // case PHI:            return "phi";
  // case Select:         return "select";
  // case Call:           return "call";
  // case Shl:            return "shl";
  // case LShr:           return "lshr";
  // case AShr:           return "ashr";
  // case VAArg:          return "va_arg";
  // case ExtractElement: return "extractelement";
  // case InsertElement:  return "insertelement";
  // case ShuffleVector:  return "shufflevector";
  // case ExtractValue:   return "extractvalue";
  // case InsertValue:    return "insertvalue";
  // case LandingPad:     return "landingpad";
  // case CleanupPad:     return "cleanuppad";
  // case Freeze:         return "freeze";

    default: LLVM_DEBUG(dbgs() << "<Invalid operator>\n"); return ISD::DELETED_NODE;
  }
}

char PrimateBFUColoring::ID = 0;

