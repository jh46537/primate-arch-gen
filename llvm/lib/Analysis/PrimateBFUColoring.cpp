#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/PrimateBFUColoring.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
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
    
    std::string BFUListEntry;
    raw_string_ostream NameStream(BFUListEntry);
    PMD->getOperand(2)->print(NameStream);
    
    // remove the !" and " from the name
    BFUListEntry.erase(BFUListEntry.begin(), BFUListEntry.begin() + 2);
    BFUListEntry.erase(BFUListEntry.end() - 1, BFUListEntry.end());
    NameStream << "\n{\n\tio\n}\n";
    LLVM_DEBUG(dbgs() << "bfu_list.txt entry:\n" << BFUListEntry << "\n");

    return true;
  }
  return false;
}

void PrimateBFUColoring::colorSubBFUs(Function &F) {
  for (auto &BB : F) {
    LLVM_DEBUG(dbgs() << "Basic Block: " << BB.getName() << "\n"
                      << "Create new ISD Op map\n");
    ISDOps = new ValueMap<Instruction *, ISDOperation *>();
    ImmNum = 0;
    
    // "Max Complexity Pattern"
    ISDOperation *MCP = nullptr;
    
    for (auto &I : BB) {
      // "New Pattern" for tablegen
      std::pair<Instruction *, ISDOperation *> 
          NP(&I, new ISDOperation(I.getOpcode(), 0));
      
      LLVM_DEBUG(dbgs() << "Current instruction: "; I.dump());
      
      if (NP.second->opcode() == ISD::DELETED_NODE) {
        LLVM_DEBUG(dbgs() << "Pattern for this instruction is not supported\n"); 
        continue;
      }

      if (I.getOpcode() == Instruction::GetElementPtr) {
        createGEPPatt(NP);
      }
      else {
        createISDPatt(NP);
      }

      LLVM_DEBUG(dbgs() << "ISD Operation Pattern: "; NP.second->dump());
      ISDOps->insert(NP);
      
      if (!MCP || (MCP->complexity() < NP.second->complexity() &&
                   NP.second->opcode() != ISD::DELETED_NODE))
        MCP = NP.second;
    }
    LLVM_DEBUG(dbgs() << "The ISDOperation that characterizes the current BB "
                         "is the one with the highest complextity\n\n"
                      << "Highest complexity pattern:\n"; MCP->dump());
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

    // TODO: Maybe I should change this ctor to have Instruction as an input
    //       because just knowing that it's a load is not enough to characterize
    //       all cases!
    case Instruction::Load: 
      OPName = "extract";   
      Opcode = ISD::LOAD;
      break;

    case Instruction::Store:
      OPName = "insert";  
      Opcode = ISD::STORE;
      break;

    // Marking GEP as a GlobalAddress leaf node likely is not entirely correct
    // however, it works for the use case of this pass
    case Instruction::GetElementPtr:
      // OPName = "GEP is a lie";  
      OPName = "";  
      Opcode = ISD::GlobalAddress;
      break;

    default: 
      // LLVM_DEBUG(dbgs() << "<Invalid operator>\n"); 
      OPName = "skipped_node";  
      Opcode = ISD::DELETED_NODE;
  }
}

void 
PrimateBFUColoring::createISDPatt(std::pair<Instruction *, ISDOperation *> &P) {
  int OPN = 0;
  for (auto &OP : P.first->operands()) {
    LLVM_DEBUG(dbgs() << "Operand Number " << OPN << ": "; OP->dump());

    bool IsDependency = false;    // Is the current operand an instr dependency?
    ISDOperation *DISD = nullptr; // Dependant ISD Operation for current operation
    std::string NewOP;            // New Operand for the current ISD operation
    raw_string_ostream NewOPStream(NewOP);

    if (auto *IOP = dyn_cast<Instruction>(OP)) {
      DISD = (*ISDOps)[IOP];
      if (DISD && DISD->opcode() != ISD::DELETED_NODE) {
        LLVM_DEBUG(dbgs() << "\tThis op is an instruction!\n");
        IsDependency = true;
      }
    }

    if (IsDependency) {
      DISD->print(NewOPStream);
    }
    else {
      switch (OP->getType()->getTypeID()) {
        // TODO: Need to differentiate bw GPR inputs and imm inputs
        case Type::IntegerTyID:
          NewOPStream << "GPR:$rs" << OPN; 
          break;

        case Type::PointerTyID:
          LLVM_DEBUG(dbgs() << "Pointers are WIP!\n");

          // I'm not sure which one is more correct in the case:
          // for example, if we have the following IR:
          // {
          //    %a = getelementptr inbounds %struct.MAC_input_t, ptr %vec_in, i32 0, i32 0
          //    %0 = load i32, ptr %a, align 4
          // }
          // 
          // IR operation "load" only ever has 1 operand (ignore the align 4)
          // So using OPN and OP.getOperandNo() are identical. However, I'm
          // not sure if this captures all cases

          // NewOPStream << "BaseAddr:$rs" << OP.getOperandNo(); 
          NewOPStream << "BaseAddr:$rs" << OPN; 
          break;

        default:
          LLVM_DEBUG(dbgs() << "Unsupported operand type encountered!\n";
                     printDerviedType(OP->getType()->getTypeID()));
          NewOPStream << "NULL"; 
      }
    }
    LLVM_DEBUG(dbgs() << "\tOperand Pattern: " << NewOP << "\n");
    P.second->pushOperand(NewOP);
    if (IsDependency)
      OPN += DISD->numOperands();
    else
      OPN++;
  }
  P.second->compInrc(OPN);
}

void 
PrimateBFUColoring::createGEPPatt(std::pair<Instruction *, ISDOperation *> &P) {
  for (auto &OP : P.first->operands()) {
    LLVM_DEBUG(dbgs() << "Curr GEP operand: "; OP->dump());
    LLVM_DEBUG(dbgs() << "Curr GEP operand type: "; 
               printDerviedType(OP->getType()->getTypeID()));

    if (OP.getOperandNo() == 1)
      continue;

    std::string NewOP;
    raw_string_ostream NewOPStream(NewOP);
    
    switch (OP->getType()->getTypeID()) {
      case Type::IntegerTyID:
        NewOPStream << "simm12:$imm" << ImmNum; 
        ImmNum++;
        break;
        
      // For GEP, we only ever have one pointer arg
      case Type::PointerTyID:
        NewOPStream << "BaseAddr:$rs0"; 
        break;

      default:
        LLVM_DEBUG(dbgs() << "Unsupported operand type encountered!\n";
                   printDerviedType(OP->getType()->getTypeID()));
        NewOPStream << "NULL"; 
    }
    LLVM_DEBUG(dbgs() << "\tOperand Pattern: " << NewOP << "\n");
    P.second->pushOperand(NewOP);
  }
}

char PrimateBFUColoring::ID = 0;

