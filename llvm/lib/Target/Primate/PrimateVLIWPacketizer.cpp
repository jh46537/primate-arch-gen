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
#include "llvm/IR/DebugLoc.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCInstrDesc.h"
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
    dbgs() << "starting packetizing on MB:\n ";
    MB.print(dbgs());
    dbgs() << "===========================\n ";
    Packetizer.PacketizeMIs(&MB, MB.begin(), MB.end());
    printf("");
  }
  return true;
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
  unsigned Idx = 0;
  for (MachineInstr *MI : CurrentPacketMIs) {
    unsigned R = ResourceTracker->getUsedResources(Idx++);
    unsigned slotIdx = llvm::countTrailingZeros(R);  // convert bitvector to ID; assume single bit set
    MI->setSlotIdx(slotIdx);
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
  MachineInstr &MIFirst = *CurrentPacketMIs.front();
  finalizeBundle(*MBB, MIFirst.getIterator(), MI.getInstrIterator());
  CurrentPacketMIs.clear();
  ResourceTracker->clearResources();
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
    dbgs() << "Legal to packetize:\n\t";
    SUI->getInstr()->print(dbgs());
    dbgs() << "\t";
    SUJ->getInstr()->print(dbgs());
    dbgs() << "\t due to unrelated instrs\n";
    return true;
  }

  // if SUI IS a successor to SUJ, then we should check the kind of successor
  // if the dependency between SUI and SUJ is a data then we can packetize. otherwise we cannot.
  for (unsigned i = 0; i < SUJ->Succs.size(); ++i) {
    if (SUJ->Succs[i].getSUnit() != SUI)
      continue;

    dbgs() << "Illegal to packetize:\n\t";
    SUI->getInstr()->print(dbgs());
    dbgs() << "\t";
    SUJ->getInstr()->print(dbgs());
    
    SDep::Kind DepType = SUJ->Succs[i].getKind();
    switch(DepType) {
    case SDep::Kind::Data:
      dbgs() << "\tDue to data dep\n";
      return false;
    case SDep::Kind::Anti:
      dbgs() << "\tDue to anti dep\n";
      return false;
    case SDep::Kind::Output:
      dbgs() << "\tDue to output dep\n";
      return false;
    case SDep::Kind::Order:
      dbgs() << "\tDue to other ordering dep\n";
      return false;
    }
  }
  dbgs() << "Legal to packetize:\n\t";
  SUI->getInstr()->print(dbgs());
  dbgs() << "\t";
  SUJ->getInstr()->print(dbgs());
  dbgs() << "\tDue to no deps\n";
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
