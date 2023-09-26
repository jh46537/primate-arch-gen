//===-- PrimateVLIWPacketizer.cpp - Primate VLIW packetizer -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements a simple VLIW packetizer using DFA. The packetizer works on
// machine basic blocks. For each instruction I in BB, the packetizer consults
// the DFA to see if machine resources are available to execute I. If so, the
// packetizer checks if I depends on any instruction J in the current packet.
// If no dependency is found, I is added to current packet and machine resource
// is marked as taken. If any dependency is found, a target API call is made to
// prune the dependence.
//
//===----------------------------------------------------------------------===//

#include "PrimateVLIWPacketizer.h"
#include "Primate.h"
#include "PrimateInstrInfo.h"
#include "PrimateRegisterInfo.h"
#include "PrimateSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <iterator>

using namespace llvm;

#define DEBUG_TYPE "primate-packetizer"

static cl::opt<bool> DisablePacketizer("disable-primate-packetizer", cl::Hidden,
  cl::ZeroOrMore, cl::init(false),
  cl::desc("Disable Primate packetizer pass"));

namespace llvm {

FunctionPass *createPrimatePacketizer();
void initializePrimatePacketizerPass(PassRegistry&);

} // end namespace llvm

namespace {

  class PrimatePacketizer : public MachineFunctionPass {
  public:
    static char ID;

    PrimatePacketizer() : MachineFunctionPass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
      AU.addRequired<AAResultsWrapperPass>();
      AU.addRequired<MachineBranchProbabilityInfo>();
      AU.addRequired<MachineDominatorTree>();
      AU.addRequired<MachineLoopInfo>();
      AU.addPreserved<MachineDominatorTree>();
      AU.addPreserved<MachineLoopInfo>();
      MachineFunctionPass::getAnalysisUsage(AU);
    }

    StringRef getPassName() const override { return "Primate Packetizer"; }
    bool runOnMachineFunction(MachineFunction &Fn) override;

    MachineFunctionProperties getRequiredProperties() const override {
      return MachineFunctionProperties().set(
          MachineFunctionProperties::Property::NoVRegs);
    }

  private:
    const PrimateInstrInfo *PII = nullptr;
    const PrimateRegisterInfo *PRI = nullptr;
  };

} // end anonymous namespace

char PrimatePacketizer::ID = 0;

INITIALIZE_PASS_BEGIN(PrimatePacketizer, "primate-packetizer",
                      "Primate Packetizer", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_DEPENDENCY(MachineBranchProbabilityInfo)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_END(PrimatePacketizer, "primate-packetizer",
                    "Primate Packetizer", false, false)

PrimatePacketizerList::PrimatePacketizerList(MachineFunction &MF,
      MachineLoopInfo &MLI, AAResults *AA,
      const MachineBranchProbabilityInfo *MBPI)
    : VLIWPacketizerList(MF, MLI, AA), MBPI(MBPI), MLI(&MLI) {
  PII = MF.getSubtarget<PrimateSubtarget>().getInstrInfo();
  PRI = MF.getSubtarget<PrimateSubtarget>().getRegisterInfo();
}

bool PrimatePacketizer::runOnMachineFunction(MachineFunction &MF) {
  auto &PST = MF.getSubtarget<PrimateSubtarget>();
  PII = PST.getInstrInfo();
  PRI = PST.getRegisterInfo();
  auto &MLI = getAnalysis<MachineLoopInfo>();
  auto *AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
  auto *MBPI = &getAnalysis<MachineBranchProbabilityInfo>();

  // Instantiate the packetizer.
  PrimatePacketizerList Packetizer(MF, MLI, AA, MBPI);

  // DFA state table should not be empty.
  assert(Packetizer.getResourceTracker() && "Empty DFA table!");

  // Loop over all of the basic blocks.
  for (auto &MB : MF) {
    // TODO(ahsu): fix scheduling boundary
    printf("");
    LLVM_DEBUG({
      dbgs() << "starting packetizing on MB:\n ";
      MB.print(dbgs());
      dbgs() << "===========================\n ";
    });
    Packetizer.PacketizeMIs(&MB, MB.begin(), MB.end());
    printf("");
  }
  return true;
}

bool PrimatePacketizerList::insertBypassOps(MachineInstr* br_inst, llvm::SmallVector<MachineInstr*, 2>& generated_bypass_instrs) {
  // Create copy of ResourceTracker to try insertion
  DFAPacketizer TryResourceTracker{ *ResourceTracker };

  for (auto& operand : br_inst->uses()) {
    // skip non-reg
    if (!operand.isReg())
      continue;
    
    // check if someone already generates this operand
    bool operand_generated = false;
    for (auto& otherMI : CurrentPacketMIs) {
      // skip the branch subinstruction itself
      if (otherMI == br_inst)
        continue;
      for (auto& otherOperand : otherMI->defs()) {
        // skip non-reg
        if (!otherOperand.isReg())
          continue;
        // if the reg indices match, the producer has been found
        if (otherOperand.getReg() == operand.getReg()) {
          operand_generated = true;
        }
      }
    }
    if(operand_generated) {
      LLVM_DEBUG({dbgs() << "PrimatePacketizerList::endPacket found gernerator for "; operand.dump();});
      continue;
    }

    // no one generated this operand. attempt to a bypass op.
    LLVM_DEBUG(dbgs() << "PrimatePacketizerList::endPacket No producer op for branch instr. Attempt bypass op.\n");
    MachineInstr* bypass_op = BuildMI(*br_inst->getParent(), br_inst, llvm::DebugLoc(), PII->get(Primate::ADDI), operand.getReg())
              .addReg(operand.getReg())
              .addImm(0);
    bool ResourceAvail = TryResourceTracker.canReserveResources(*bypass_op);
    if (!ResourceAvail) {
      // no room. set up packet for next iterations. 
      // 1) Need to put all the bypass ops into the BB above the Branch.
      // 2) Move the end of packet pointer to before the bypass.
      // 3) end packet as normal WITHOUT the branch 
      // 4) insert the bypasses into the current packet.
      // 5) return to main loop
      
      // bypass_op->print(dbgs());
      // LLVM_DEBUG(dbgs() << "============ Current Packet ============\n");
      // br_inst->getParent()->dump();
      // LLVM_DEBUG(dbgs() << "============ Bypass Op Basic Block ============\n");
      // bypass_op->getParent()->dump();
      LLVM_DEBUG(dbgs() << "PrimatePacketizerList::endPacket cannot insert bypass_instr! no resources!\n");
      generated_bypass_instrs.push_back(bypass_op);
      return true; // push the br and bypasses to the next packet.
    }
    else {
      LLVM_DEBUG({dbgs() << "PrimatePacketizerList::endPacket Bypass instr inserted for operand: ";  operand.dump(); });
      TryResourceTracker.reserveResources(*bypass_op);
      generated_bypass_instrs.push_back(bypass_op);
    }
  }
  return false;
}

MachineBasicBlock::iterator
PrimatePacketizerList::addToPacket(MachineInstr &MI) {
  MachineBasicBlock::iterator MII = MI.getIterator();
  //MachineBasicBlock *MBB = MI.getParent();
  assert(ResourceTracker->canReserveResources(MI));
  ResourceTracker->reserveResources(MI);
  CurrentPacketMIs.push_back(&MI);
  return MII;
}

void PrimatePacketizerList::endPacket(MachineBasicBlock *MBB,
                                      MachineBasicBlock::iterator MI) {
  // Replace VLIWPacketizerList::endPacket(MBB, EndMI).

  // need to first generate the needed bypass ops
  // generating bypass ops allows the fix up to x0 out the bypasses for free :>
  MachineInstr* packet_breaking_instr = CurrentPacketMIs.back();
  llvm::SmallVector<MachineInstr*, 2> generated_bypass_instrs;
  MachineBasicBlock::iterator old_end_instr = MI;
  bool push_branch_to_next_packet = false;
  if(packet_breaking_instr->isBranch()) {
    push_branch_to_next_packet = insertBypassOps(packet_breaking_instr, generated_bypass_instrs);

    // if we push to the next packet then pop the branch from the back && set the isntr pointer back.
    if(push_branch_to_next_packet) {
      LLVM_DEBUG({dbgs() << "pushing branch to a new packet.\n";});
      CurrentPacketMIs.pop_back();
      --MI;
      for(auto& _: generated_bypass_instrs){
        --MI;
      }
      MI->dump();
    }
    else if(generated_bypass_instrs.size() > 0) { 
      LLVM_DEBUG({dbgs() << "Bypasses fit into same packet: "
          << generated_bypass_instrs.size() << " ops\n";});
      CurrentPacketMIs.pop_back();
      for(auto& bypass_op: generated_bypass_instrs) {
        ResourceTracker->reserveResources(*bypass_op);
        CurrentPacketMIs.push_back(bypass_op);
      }
      CurrentPacketMIs.push_back(packet_breaking_instr);
    }
  }

  unsigned Idx = 0;
  for (MachineInstr *MI : CurrentPacketMIs) {
    unsigned R = ResourceTracker->getUsedResources(Idx++);
    unsigned slotIdx = llvm::countTrailingZeros(R);  // convert bitvector to ID; assume single bit set
    LLVM_DEBUG({dbgs() << "Instruction number " << Idx-1 << " aka: "; 
                MI->dump();
                dbgs() << "used resource: 0x" << R << " Turned to slotIdx: " << slotIdx << "\n";});
    MI->setSlotIdx(slotIdx);
  }

  // in-place fixup for packetized dependent branches
  // there must only be 1 branch per packet
  for (auto& MI : CurrentPacketMIs) {
    // skip non-branches
    if (!MI->isBranch())
      continue;
    // fixup branch operands and potentially kill producer operand defs
    for (auto& operand : MI->uses()) {
      // skip non-reg
      if (!operand.isReg())
        continue;
      // this is a branch reg operand; the producer needs to be in this packet
      // find producer; there must be only 1 producer
      bool found_producer = false;
      for (auto& otherMI : CurrentPacketMIs) {
        // skip the branch subinstruction itself
        if (otherMI == MI)
          continue;
        for (auto& otherOperand : otherMI->defs()) {
          // skip non-reg
          if (!otherOperand.isReg())
            continue;
          // if the reg indices match, the producer has been found
          if (otherOperand.getReg() == operand.getReg()) {
            operand.setReg(Primate::X0 + otherMI->getSlotIdx());
            if (operand.isKill()) {
              otherOperand.setReg(Primate::X0);
            }
            found_producer = true;
            break;
          }
        }
        if (found_producer) {
          break;
        }
      }
      if (!found_producer) {
          LLVM_DEBUG({
            dbgs() << "no gen instr for: "; operand.dump(); dbgs() << ". Packet looks like:\n"; 
            for(auto& temp: CurrentPacketMIs){
              temp->dump(); 
            }
          });
          llvm_unreachable("No generating instr found. Should NEVER happen as failure to add bypasses triggers a packet push.");
      }
    }
  }
  LLVM_DEBUG({
    if (!CurrentPacketMIs.empty()) {
      dbgs() << "Finalizing packet:\n";
      unsigned Idx = 0;
      for (MachineInstr *MI : CurrentPacketMIs) {
        unsigned R = ResourceTracker->getUsedResources(Idx++);
          dbgs() << " * [res:0x" << utohexstr(R) << "] " << *MI;
      }
    }
  });
  //if (CurrentPacketMIs.size() > 1) {
  //  MachineInstr &MIFirst = *CurrentPacketMIs.front();
  //  finalizeBundle(*MBB, MIFirst.getIterator(), MI.getInstrIterator());
  //}
  if (CurrentPacketMIs.size() < 1) {
    if(push_branch_to_next_packet) {
      llvm_unreachable("attempted to packetize an empty packet due to pushing a branch. Should NEVER happen.");
    }
    llvm_unreachable("attempted to packetize an empty packet.");
  }

  MachineInstr &MIFirst = *CurrentPacketMIs.front();
  finalizeBundle(*MBB, MIFirst.getIterator(), MI.getInstrIterator());
  CurrentPacketMIs.clear();
  ResourceTracker->clearResources();

  LLVM_DEBUG({
    dbgs() << "BB after packetizing\n";
  });
  packet_breaking_instr->getParent()->dump();

  // FIXME(amans)
  // if pushing we need to goto a new packet.
  // that packet has to be ended immedietly since bypass ops have no scheduling information.
  if(push_branch_to_next_packet) {
    for(auto& bypasser: generated_bypass_instrs) {
      CurrentPacketMIs.push_back(bypasser);
      ResourceTracker->reserveResources(*bypasser);
    }
    CurrentPacketMIs.push_back(packet_breaking_instr);
    ResourceTracker->reserveResources(*packet_breaking_instr);

    endPacket(MBB, old_end_instr); // bad hack. prevents packing with bypassed branchs. 
  }

  LLVM_DEBUG(dbgs() << "End packet\n");
}

void PrimatePacketizerList::initPacketizerState() {
}

// Ignore bundling of pseudo instructions.
bool PrimatePacketizerList::ignorePseudoInstruction(const MachineInstr &MI,
                                                    const MachineBasicBlock *) {
  // FIXME: ignore END or maybe in isSoloInstruction?
  if (MI.isCFIInstruction())
    return true;

  // We check if MI has any functional units mapped to it. If it doesn't,
  // we ignore the instruction.
  const MCInstrDesc& TID = MI.getDesc();
  auto *IS = ResourceTracker->getInstrItins()->beginStage(TID.getSchedClass());
  return !IS->getUnits();
}

bool PrimatePacketizerList::isSoloInstruction(const MachineInstr &MI) {
  return false;
}

bool PrimatePacketizerList::shouldAddToPacket(const MachineInstr &MI) {
  return true;
}

// SUI is the current instruction that is outside of the current packet.
// SUJ is the current instruction inside the current packet against which that
// SUI will be packetized
bool PrimatePacketizerList::isLegalToPacketizeTogether(SUnit *SUI, SUnit *SUJ) {
  // There no dependency between a prolog instruction and its successor.

  // Need to read in a representation of the uArch and then do it.
  if(SUI->getInstr()->isBranch()) {
    return true;
  }
  
  // if SUI is not a successor to SUJ then we are good always
  if (!SUJ->isSucc(SUI)) {
    LLVM_DEBUG({
      dbgs() << "Legal to packetize:\n\t";
      SUI->getInstr()->print(dbgs());
      dbgs() << "\t";
      SUJ->getInstr()->print(dbgs());
      dbgs() << "\t due to unrelated instrs\n";
    });
    return true;
  }

  // if SUI IS a successor to SUJ, then we should check the kind of successor
  // if the dependency between SUI and SUJ is a data then we can packetize. otherwise we cannot.
  for (unsigned i = 0; i < SUJ->Succs.size(); ++i) {
    if (SUJ->Succs[i].getSUnit() != SUI)
      continue;

    SDep::Kind DepType = SUJ->Succs[i].getKind();
    switch(DepType) {
    case SDep::Kind::Data:
      LLVM_DEBUG({
        dbgs() << "Illegal to packetize:\n\t";
        SUI->getInstr()->print(dbgs());
        dbgs() << "\t";
        SUJ->getInstr()->print(dbgs());
        dbgs() << "\tDue to RAW hazard\n";
      });
      return false;
    // WAR hazards are okay to packetize together since all operands are read before the packet
    //case SDep::Kind::Anti:
    //  dbgs() << "Illegal to packetize:\n\t";
    //  SUI->getInstr()->print(dbgs());
    //  dbgs() << "\t";
    //  SUJ->getInstr()->print(dbgs());
    //  dbgs() << "\tDue to WAR hazard\n";
    //  return false;
    case SDep::Kind::Output:
      LLVM_DEBUG({
        dbgs() << "Illegal to packetize:\n\t";
        SUI->getInstr()->print(dbgs());
        dbgs() << "\t";
        SUJ->getInstr()->print(dbgs());
        dbgs() << "\tDue to WAW hazard\n";
      });
      return false;
    case SDep::Kind::Order:
      LLVM_DEBUG({
        dbgs() << "Illegal to packetize:\n\t";
        SUI->getInstr()->print(dbgs());
        dbgs() << "\t";
        SUJ->getInstr()->print(dbgs());
        dbgs() << "\tDue to other ordering requirement\n";
      });
      return false;
    default:
      ; // do nothing
    }
  }
  LLVM_DEBUG({
    dbgs() << "Legal to packetize:\n\t";
    SUI->getInstr()->print(dbgs());
    dbgs() << "\t";
    SUJ->getInstr()->print(dbgs());
    dbgs() << "\tDue to no deps\n";
  });
  return true;
}

bool PrimatePacketizerList::isLegalToPruneDependencies(SUnit *SUI, SUnit *SUJ) {
  return false;
}

//===----------------------------------------------------------------------===//
//                         Public Constructor Functions
//===----------------------------------------------------------------------===//

FunctionPass *llvm::createPrimatePacketizer() {
  return new PrimatePacketizer();
}
