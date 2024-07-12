//===-- PrimateExpandPseudoInsts.cpp - Expand pseudo instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "Primate.h"
#include "PrimateInstrInfo.h"
#include "PrimateTargetMachine.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define Primate_EXPAND_PSEUDO_NAME "Primate pseudo instruction expansion pass"

namespace {

class PrimateExpandPseudo : public MachineFunctionPass {
public:
  const PrimateInstrInfo *TII;
  static char ID;

  PrimateExpandPseudo() : MachineFunctionPass(ID) {
    initializePrimateExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return Primate_EXPAND_PSEUDO_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandAuipcInstPair(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI,
                           unsigned FlagsHi, unsigned SecondOpcode);
  bool expandLoadLocalAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadGlobalAddress(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI,
                               MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadAddress(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI,
                         MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSIEAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSGDAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandVSetVL(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandVMSET_VMCLR(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI, unsigned Opcode);
  bool expandVSPILL(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandVRELOAD(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
};

char PrimateExpandPseudo::ID = 0;

bool PrimateExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const PrimateInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool PrimateExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool PrimateExpandPseudo::expandLoadGlobalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  unsigned SecondOpcode = MBB.getParent()->getSubtarget<PrimateSubtarget>().is64Bit() ? Primate::LD : Primate::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, PrimateII::MO_GOT_HI,
                             SecondOpcode);
}


bool PrimateExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  // PrimateInstrInfo::getInstSizeInBytes hard-codes the number of expanded
  // instructions for each pseudo, and must be updated when adding new pseudos
  // or changing existing ones.
  switch (MBBI->getOpcode()) {
  case Primate::PseudoLLA:
    return expandLoadLocalAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLGA:
    return expandLoadGlobalAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLA:
    return expandLoadAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLA_TLS_IE:
    return expandLoadTLSIEAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLA_TLS_GD:
    return expandLoadTLSGDAddress(MBB, MBBI, NextMBBI);
  }

  return false;
}

bool PrimateExpandPseudo::expandAuipcInstPair(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI, unsigned FlagsHi,
    unsigned SecondOpcode) {
  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  Register DestReg = MI.getOperand(0).getReg();
  const MachineOperand &Symbol = MI.getOperand(1);

  MachineBasicBlock *NewMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Tell AsmPrinter that we unconditionally want the symbol of this label to be
  // emitted.
  NewMBB->setLabelMustBeEmitted();

  MF->insert(++MBB.getIterator(), NewMBB);

  BuildMI(NewMBB, DL, TII->get(Primate::AUIPC), DestReg)
      .addDisp(Symbol, 0, FlagsHi);
  BuildMI(NewMBB, DL, TII->get(SecondOpcode), DestReg)
      .addReg(DestReg)
      .addMBB(NewMBB, PrimateII::MO_PCREL_LO);

  // Move all the rest of the instructions to NewMBB.
  NewMBB->splice(NewMBB->end(), &MBB, std::next(MBBI), MBB.end());
  // Update machine-CFG edges.
  NewMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  // Make the original basic block fall-through to the new.
  MBB.addSuccessor(NewMBB);

  // Make sure live-ins are correctly attached to this new basic block.
  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *NewMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();
  return true;
}

bool PrimateExpandPseudo::expandLoadLocalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, PrimateII::MO_PCREL_HI,
                             Primate::ADDI);
}

bool PrimateExpandPseudo::expandLoadAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  unsigned SecondOpcode;
  unsigned FlagsHi;
  if (MF->getTarget().isPositionIndependent()) {
    const auto &STI = MF->getSubtarget<PrimateSubtarget>();
    SecondOpcode = STI.is64Bit() ? Primate::LD : Primate::LW;
    FlagsHi = PrimateII::MO_GOT_HI;
  } else {
    SecondOpcode = Primate::ADDI;
    FlagsHi = PrimateII::MO_PCREL_HI;
  }
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, FlagsHi, SecondOpcode);
}

bool PrimateExpandPseudo::expandLoadTLSIEAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  const auto &STI = MF->getSubtarget<PrimateSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? Primate::LD : Primate::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, PrimateII::MO_TLS_GOT_HI,
                             SecondOpcode);
}

bool PrimateExpandPseudo::expandLoadTLSGDAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, PrimateII::MO_TLS_GD_HI,
                             Primate::ADDI);
}

bool PrimateExpandPseudo::expandVSetVL(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MBBI) {
  llvm_unreachable("Expanding a VSetVL on Primate should not be possible");
}

bool PrimateExpandPseudo::expandVMSET_VMCLR(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          unsigned Opcode) {
  llvm_unreachable("Should never see a VMSET_VMCLR");
}

bool PrimateExpandPseudo::expandVSPILL(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MBBI) {
  llvm_unreachable("Should never see a VSPILL");
}

bool PrimateExpandPseudo::expandVRELOAD(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI) {
  llvm_unreachable("Should never see a VRELOAD");
}

} // end of anonymous namespace

INITIALIZE_PASS(PrimateExpandPseudo, "primate-expand-pseudo",
                Primate_EXPAND_PSEUDO_NAME, false, false)
namespace llvm {

FunctionPass *createPrimateExpandPseudoPass() { return new PrimateExpandPseudo(); }

} // end of namespace llvm
