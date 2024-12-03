#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/PrimateBFUColoring.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_ostream.h"
#include <utility>

using namespace llvm;

PreservedAnalyses PrimateBFUColoring::run(Function &F, 
                                          FunctionAnalysisManager& AM) {
  LLVM_DEBUG(dbgs() << "Hello from PrimateBFUColoring\n\n");
  auto BP = BFUPatternInfo::create(F);  
  if (BP) {
    createBFUPatterns(F, BP.get());
    std::error_code EC;
    StringRef File = "bfu_list.yaml";
    raw_fd_ostream OS(File, EC);
    yaml::Output YamlFileOut(OS);
    YamlFileOut << *BP;
  }
  return PreservedAnalyses::all();
}

void PrimateBFUColoring::createBFUPatterns(Function &F, BFUPatternInfo *BPI) {
  for (auto &BB : F) {
    LLVM_DEBUG(dbgs() << "Basic Block: " << BB.getName() << "\n");
    ISDOperationMap = new ValueMap<Instruction *, ISDOperation *>();
    ImmNum = 0;
    
    ISDOperation *MCP = nullptr; // "Max Complexity Pattern"
    
    for (auto &I : BB) {
      LLVM_DEBUG(dbgs() << "Current instruction: "; I.dump());
      std::pair<Instruction *, ISDOperation *> 
          NP(&I, new ISDOperation(I.getOpcode(), 0)); // "New Pattern" for tablegen

      NP.second->InstName = BPI->BFUname + std::string(BB.getName());
      
      if (NP.second->opcode() == ISD::DELETED_NODE) {
        LLVM_DEBUG(dbgs() << "Pattern for this instruction is not supported\n"); 
        continue;
      }

      if (I.getOpcode() == Instruction::GetElementPtr) {
        processGEP(NP);
      }
      else {
        processISD(NP);
      }

      LLVM_DEBUG(dbgs() << "ISD Operation Pattern: "; NP.second->dump());
      ISDOperationMap->insert(NP);
      
      if (!MCP || (MCP->complexity() < NP.second->complexity() &&
                   NP.second->opcode() != ISD::DELETED_NODE))
        MCP = NP.second;
    }
    LLVM_DEBUG(dbgs() << "Highest complexity pattern: "; 
               MCP->dump(); dbgs() << "\n");

    // This whole block might feel veru weird and hacky, and that's because it is!
    raw_string_ostream PatternStream(MCP->Pattern);
    MCP->print(PatternStream);
    BPI->InstrList.push_back(MCP);
    delete ISDOperationMap;
  }
}

void 
PrimateBFUColoring::processISD(std::pair<Instruction *, ISDOperation *> &P) {
  int OPN = 0;
  for (auto &OP : P.first->operands()) {
    LLVM_DEBUG(dbgs() << "Operand Number " << OPN << ": "; OP->dump());

    bool IsDependency = false;    // Is the current operand an instr dependency?
    ISDOperation *DISD = nullptr; // Dependant ISD Operation for current operation
    std::string NewOP;            // New Operand for the current ISD operation
    raw_string_ostream NewOPStream(NewOP);

    if (auto *IOP = dyn_cast<Instruction>(OP)) {
      DISD = (*ISDOperationMap)[IOP];
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
          LLVM_DEBUG(dbgs() << "Pointers are WIP!\n");
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
PrimateBFUColoring::processGEP(std::pair<Instruction *, ISDOperation *> &P) {
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
        
    // case Instruction::ICmp:           
    //   OPName =  "icmp";
    //   // Opcode = ISD::
    //   break;
    // case Instruction::FCmp:
        // OPname "fcmp";

    // Marking GEP as a GlobalAddress leaf node likely is not entirely correct
    // however, it works for the use case of this pass
    case Instruction::GetElementPtr:
      // OPName = "GEP is a lie";  
      OPName = "";  
      Opcode = ISD::GlobalAddress;
      break;

    default: 
      OPName = "skipped_node";  
      Opcode = ISD::DELETED_NODE;
  }
}

void ISDOperation::print(raw_ostream &ROS) {
  if (Opcode != ISD::GlobalAddress) {
    ROS << "(";
    ROS << OPName;
  }
  
  for (auto &OP : Operands)
    ROS << " " << OP;
  
  if (Opcode != ISD::GlobalAddress) 
    ROS << ")";
}

void ISDOperation::dump() {
  print(dbgs()); dbgs () << "\n";
}

