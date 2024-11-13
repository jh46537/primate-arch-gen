#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/PrimateBFUColoring.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <utility>

using namespace llvm;


PreservedAnalyses PrimateBFUColoring::run(Function &F, 
                                          FunctionAnalysisManager& AM) {

  LLVM_DEBUG(dbgs() << "\n==========================\n"
                    << "Here in PrimateBFUColoring\n\n");
  
  if (isBFU(F)) {
    LLVM_DEBUG(dbgs() << "Found BFU Function:\n"; F.dump(););
    LLVM_DEBUG(dbgs() << "\nCreate new ISD Op map\n");
    ISDOps = new ValueMap<Instruction *, ISDOperation *>();
    
    colorSubBFUs(F);
    
    LLVM_DEBUG(dbgs() << "\nDelete ISD Op map\n");
    delete ISDOps;
  }
  else {
    LLVM_DEBUG(dbgs() << F.getName() << "is NOT a BFU function\n\n");
  }

  LLVM_DEBUG(dbgs() << "\nBye from PrimateBFUColoring" 
                    << "\n==========================\n\n");

  return PreservedAnalyses::all();
}

bool PrimateBFUColoring::isBFU(Function &F) {
  MDNode* PMD = F.getMetadata("primate");
  if (PMD && 
      dyn_cast<MDString>(PMD->getOperand(0))->getString() == "blue") {
    LLVM_DEBUG(dbgs() << "Dump BFU metadata: "; PMD->dump(); dbgs() << "\n");
    
    std::string BFUName;
    raw_string_ostream NameStream(BFUName);
    PMD->getOperand(2)->print(NameStream);
    
    // remove the !" and " from the name
    // BFUName.erase(BFUName.begin(), BFUName.begin() + 1);
    // BFUName.erase(BFUName.end());
    
    LLVM_DEBUG(dbgs() << "\nBFU name: " << BFUName << "\n\n");

    return true;
  }
  return false;
}

void PrimateBFUColoring::colorSubBFUs(Function &F) {
  LLVM_DEBUG(dbgs() << "\nPopulate ISD Op map\n\n");
  for (auto &BB : F) {
    LLVM_DEBUG(dbgs() << "Basic Block: " << BB.getName() << "\n");
    for (auto &I : BB) {
      // "New Pattern" for tablegen
      std::pair<Instruction *, ISDOperation *> NP(&I, new ISDOperation);
      
      opcodeToISD(I.getOpcode(), NP.second);

      LLVM_DEBUG(dbgs() << "Current instruction: "; I.dump());

      if (NP.second->Opcode == ISD::DELETED_NODE) {
        LLVM_DEBUG(dbgs() << "Pattern for this instr is not supported yet\n\n"); 
        continue;
      }

      LLVM_DEBUG(dbgs() << "ISD Opname: " << NP.second->OPName << "\n");

      for (auto &OP : I.operands()) {
        LLVM_DEBUG(dbgs() << "op num " << OP.getOperandNo() << ":\n"); 

        bool IsDependency = false; // is the current operand an instr dependency?
        ISDOperation *DISD = nullptr;

        if (auto *IOP = dyn_cast<Instruction>(OP)) {
          DISD = (*ISDOps)[IOP];
          if (DISD && DISD->Opcode != ISD::DELETED_NODE) {
            LLVM_DEBUG(dbgs() << "This op is an instruction!\n");
            NP.second->Dependencies.push_back(IOP);
            IsDependency = true;
          }
        }

        std::string NewOP;
        raw_string_ostream NewOPStream(NewOP);
        if (IsDependency) {
          NewOPStream << DISD->dump();
        }
        else {
          switch (OP->getType()->getTypeID()) {
            case 13:
              NewOPStream << "GPR:$rs"; 
              NewOPStream << OP.getOperandNo();
              break;

            // case 15:
            //   dbgs() << "GPR:$rs";
            //   break;

            default:
              NewOPStream << "NULL"; 
              // LLVM_DEBUG(dbgs() << "        Operand is probably not a GPR\n");
          }
        }
        NP.second->Operands.push_back(NewOP);
        LLVM_DEBUG(dbgs() << "Operand Pattern: " << NewOP << "\n\n");
      }
      
      // dbgs() << " >>>     This intruction's has " << I.getNumUses() << " users:\n";
      // for (const auto &U : I.users()) {
      //   dbgs() << " >>>     ";
      //   U->dump();
      //   // dbgs() << " >>>     Use type: ";
      //   // U->getType()->dump();
      //   LLVM_DEBUG(dbgs() << "\n");
      // }

      LLVM_DEBUG(dbgs() << NP.second->dump() << "\n\n");
      ISDOps->insert(NP);
    }
  }
}

void PrimateBFUColoring::opcodeToISD(unsigned int OP, ISDOperation *ISDOP) {
  switch (OP) {
    case Instruction::Ret:  
      ISDOP->OPName = "return"; 
      ISDOP->Opcode = ISD::RETURNADDR;
      break;

    case Instruction::Br:   
      ISDOP->OPName = "branch"; 
      ISDOP->Opcode = ISD::BR;
      break;

    case Instruction::Add:  
      ISDOP->OPName = "add";    
      ISDOP->Opcode = ISD::ADD;
      break;

    case Instruction::Sub:  
      ISDOP->OPName = "sub";    
      ISDOP->Opcode = ISD::SUB;
      break;

    case Instruction::Mul:  
      ISDOP->OPName = "mul";    
      ISDOP->Opcode = ISD::MUL;
      break;

    case Instruction::And:  
      ISDOP->OPName = "and";    
      ISDOP->Opcode = ISD::AND;
      break;

    case Instruction::Or :  
      ISDOP->OPName = "or" ;    
      ISDOP->Opcode = ISD::OR;
      break;

    case Instruction::Xor:  
      ISDOP->OPName = "xor";    
      ISDOP->Opcode = ISD::XOR;
      break;

    case Instruction::Load: 
      ISDOP->OPName = "load";   
      // ISDOP->Opcode = ISD::LOAD;
      ISDOP->Opcode = ISD::DELETED_NODE;
      break;

    case Instruction::Store:
      ISDOP->OPName = "store";  
      ISDOP->Opcode = ISD::STORE;
      break;

    case Instruction::GetElementPtr:
      ISDOP->OPName = "GEP is a lie";  
      ISDOP->Opcode = ISD::DELETED_NODE;
      break;

    default: 
      LLVM_DEBUG(dbgs() << "<Invalid operator>\n"); 
      ISDOP->Opcode = ISD::DELETED_NODE;
  }
}

  // case Instruction::FSub:  return ISD::FSUB;
  // case Instruction::FAdd:  return ISD::FADD;
  // case Instruction::FMul:  return ISD::MUL;
  // case Instruction::UDiv:  return ISD::UDIV;
  // case Instruction::SDiv:  return ISD::SDIV;
  // case Instruction::FDiv:  return ISD::FDIV;
  // case Instruction::URem:  return ISD::UREM;
  // case Instruction::SRem:  return ISD::SREM;
  // case Instruction::FRem:  return ISD::FREM;

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
  // Memory instructions...
  // case Alloca:        return "alloca";
  // case AtomicCmpXchg: return "cmpxchg";
  // case AtomicRMW:     return "atomicrmw";
  // case Fence:         return "fence";

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

char PrimateBFUColoring::ID = 0;

