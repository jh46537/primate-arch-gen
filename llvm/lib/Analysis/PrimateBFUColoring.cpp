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
    colorSubBFUs(F);
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
  for (auto &BB : F) {
    LLVM_DEBUG(dbgs() << "Basic Block: " << BB.getName() << "\n\n"
                      << "Create new ISD Op map\n");
    ISDOps = new ValueMap<Instruction *, ISDOperation *>();
    
    // "Max Complexity Pattern"
    ISDOperation *MCP = nullptr;
    
    for (auto &I : BB) {
      // "New Pattern" for tablegen
      std::pair<Instruction *, ISDOperation *> 
          NP(&I, new ISDOperation(I.getOpcode(), 0));
      
      LLVM_DEBUG(dbgs() << "Current instruction: "; I.dump());

      if (NP.second->opcode() == ISD::DELETED_NODE) {
        LLVM_DEBUG(dbgs() << "Pattern for this instr is not supported yet\n\n"); 
        continue;
      }

      int OPN = 0;
      for (auto &OP : I.operands()) {
        LLVM_DEBUG(dbgs() << "Operand Number " << OPN << ":\n"); 

        // is the current operand an instr dependency?
        bool IsDependency = false; 

        // Dependant ISD Operation for the ISD Op we are currently parsing
        ISDOperation *DISD = nullptr;

        if (auto *IOP = dyn_cast<Instruction>(OP)) {
          DISD = (*ISDOps)[IOP];
          if (DISD && DISD->opcode() != ISD::DELETED_NODE) {
            LLVM_DEBUG(dbgs() << "\tThis op is an instruction!\n");
            IsDependency = true;
          }
        }

        std::string NewOP;
        raw_string_ostream NewOPStream(NewOP);
        if (IsDependency) {
          DISD->print(NewOPStream);
        }
        else {
          switch (OP->getType()->getTypeID()) {
            case 13:
              NewOPStream << "GPR:$rs"; 
              NewOPStream << OPN;
              break;

            default:
              LLVM_DEBUG(dbgs() << "Something bad happened\n");
              NewOPStream << "NULL"; 
          }
        }
        
        NP.second->pushOperand(NewOP);

        if (IsDependency)
          OPN += DISD->numOperands();
        else
          OPN++;

        LLVM_DEBUG(dbgs() << "\tOperand Pattern: " << NewOP << "\n\n");
      }

      LLVM_DEBUG(dbgs() << "ISD Operation Pattern: "; 
                 NP.second->dump(); dbgs() << "\n\n");
      
      if (!MCP ||
          (MCP->complexity() <= NP.second->complexity() &&
           NP.second->opcode() != ISD::DELETED_NODE)) {
        MCP = NP.second;
      }

      ISDOps->insert(NP);
    }
    LLVM_DEBUG(dbgs() << "The ISDOperation that characterizes the current BB "
                         "is the one with the highest complextity\n\n"
                      << "Highest complexity pattern:\t"; MCP->dump());
    LLVM_DEBUG(dbgs() << "\nDelete ISD Op map\n");
    delete ISDOps;
  }
}

ISDOperation::ISDOperation(unsigned int OP, unsigned int C) {
  Complexity = C;
  switch (OP) {
    case Instruction::Add:  
      OPName = "add";    
      Opcode = ISD::ADD;
      break;

    case Instruction::Sub:  
      OPName = "sub";    
      Opcode = ISD::SUB;
      break;

    case Instruction::Mul:  
      OPName = "mul";    
      Opcode = ISD::MUL;
      break;

    case Instruction::And:  
      OPName = "and";    
      Opcode = ISD::AND;
      break;

    case Instruction::Or :  
      OPName = "or" ;    
      Opcode = ISD::OR;
      break;

    case Instruction::Xor:  
      OPName = "xor";    
      Opcode = ISD::XOR;
      break;

    // case Instruction::Ret:  
    //   OPName = "return"; 
    //   // Opcode = ISD::RETURNADDR;
    //   Opcode = ISD::DELETED_NODE;
    //   break;

    // case Instruction::Br:   
    //   OPName = "branch"; 
    //   Opcode = ISD::BR;
    //   break;

    // case Instruction::Load: 
    //   OPName = "load";   
    //   // Opcode = ISD::LOAD;
    //   Opcode = ISD::DELETED_NODE;
    //   break;

    // case Instruction::Store:
    //   OPName = "store";  
    //   // Opcode = ISD::STORE;
    //   Opcode = ISD::DELETED_NODE;
    //   break;

    // case Instruction::GetElementPtr:
    //   OPName = "GEP is a lie";  
    //   Opcode = ISD::DELETED_NODE;
    //   break;

    default: 
      // LLVM_DEBUG(dbgs() << "<Invalid operator>\n"); 
      OPName = "skipped_node";  
      Opcode = ISD::DELETED_NODE;
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

