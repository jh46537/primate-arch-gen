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

bool PrimateExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  // PrimateInstrInfo::getInstSizeInBytes hard-codes the number of expanded
  // instructions for each pseudo, and must be updated when adding new pseudos
  // or changing existing ones.
  switch (MBBI->getOpcode()) {
  case Primate::PseudoLLA:
    return expandLoadLocalAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLA:
    return expandLoadAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLA_TLS_IE:
    return expandLoadTLSIEAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoLA_TLS_GD:
    return expandLoadTLSGDAddress(MBB, MBBI, NextMBBI);
  case Primate::PseudoVSETVLI:
  case Primate::PseudoVSETIVLI:
    return expandVSetVL(MBB, MBBI);
  case Primate::PseudoVMCLR_M_B1:
  case Primate::PseudoVMCLR_M_B2:
  case Primate::PseudoVMCLR_M_B4:
  case Primate::PseudoVMCLR_M_B8:
  case Primate::PseudoVMCLR_M_B16:
  case Primate::PseudoVMCLR_M_B32:
  case Primate::PseudoVMCLR_M_B64:
    // vmclr.m vd => vmxor.mm vd, vd, vd
    return expandVMSET_VMCLR(MBB, MBBI, Primate::VMXOR_MM);
  case Primate::PseudoVMSET_M_B1:
  case Primate::PseudoVMSET_M_B2:
  case Primate::PseudoVMSET_M_B4:
  case Primate::PseudoVMSET_M_B8:
  case Primate::PseudoVMSET_M_B16:
  case Primate::PseudoVMSET_M_B32:
  case Primate::PseudoVMSET_M_B64:
    // vmset.m vd => vmxnor.mm vd, vd, vd
    return expandVMSET_VMCLR(MBB, MBBI, Primate::VMXNOR_MM);
  case Primate::PseudoVSPILL2_M1:
  case Primate::PseudoVSPILL2_M2:
  case Primate::PseudoVSPILL2_M4:
  case Primate::PseudoVSPILL3_M1:
  case Primate::PseudoVSPILL3_M2:
  case Primate::PseudoVSPILL4_M1:
  case Primate::PseudoVSPILL4_M2:
  case Primate::PseudoVSPILL5_M1:
  case Primate::PseudoVSPILL6_M1:
  case Primate::PseudoVSPILL7_M1:
  case Primate::PseudoVSPILL8_M1:
    return expandVSPILL(MBB, MBBI);
  case Primate::PseudoVRELOAD2_M1:
  case Primate::PseudoVRELOAD2_M2:
  case Primate::PseudoVRELOAD2_M4:
  case Primate::PseudoVRELOAD3_M1:
  case Primate::PseudoVRELOAD3_M2:
  case Primate::PseudoVRELOAD4_M1:
  case Primate::PseudoVRELOAD4_M2:
  case Primate::PseudoVRELOAD5_M1:
  case Primate::PseudoVRELOAD6_M1:
  case Primate::PseudoVRELOAD7_M1:
  case Primate::PseudoVRELOAD8_M1:
    return expandVRELOAD(MBB, MBBI);
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
  assert(MBBI->getNumExplicitOperands() == 3 && MBBI->getNumOperands() >= 5 &&
         "Unexpected instruction format");

  DebugLoc DL = MBBI->getDebugLoc();

  assert((MBBI->getOpcode() == Primate::PseudoVSETVLI ||
          MBBI->getOpcode() == Primate::PseudoVSETIVLI) &&
         "Unexpected pseudo instruction");
  unsigned Opcode;
  if (MBBI->getOpcode() == Primate::PseudoVSETVLI)
    Opcode = Primate::VSETVLI;
  else
    Opcode = Primate::VSETIVLI;
  const MCInstrDesc &Desc = TII->get(Opcode);
  assert(Desc.getNumOperands() == 3 && "Unexpected instruction format");

  Register DstReg = MBBI->getOperand(0).getReg();
  bool DstIsDead = MBBI->getOperand(0).isDead();
  BuildMI(MBB, MBBI, DL, Desc)
      .addReg(DstReg, RegState::Define | getDeadRegState(DstIsDead))
      .add(MBBI->getOperand(1))  // VL
      .add(MBBI->getOperand(2)); // VType

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool PrimateExpandPseudo::expandVMSET_VMCLR(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          unsigned Opcode) {
  DebugLoc DL = MBBI->getDebugLoc();
  Register DstReg = MBBI->getOperand(0).getReg();
  const MCInstrDesc &Desc = TII->get(Opcode);
  BuildMI(MBB, MBBI, DL, Desc, DstReg)
      .addReg(DstReg, RegState::Undef)
      .addReg(DstReg, RegState::Undef);
  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool PrimateExpandPseudo::expandVSPILL(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MBBI) {
  const TargetRegisterInfo *TRI =
      MBB.getParent()->getSubtarget().getRegisterInfo();
  DebugLoc DL = MBBI->getDebugLoc();
  Register SrcReg = MBBI->getOperand(0).getReg();
  Register Base = MBBI->getOperand(1).getReg();
  Register VL = MBBI->getOperand(2).getReg();
  auto ZvlssegInfo = TII->isPRVSpillForZvlsseg(MBBI->getOpcode());
  if (!ZvlssegInfo)
    return false;
  unsigned NF = ZvlssegInfo->first;
  unsigned LMUL = ZvlssegInfo->second;
  assert(NF * LMUL <= 8 && "Invalid NF/LMUL combinations.");
  unsigned Opcode = Primate::VS1R_V;
  unsigned SubRegIdx = Primate::sub_vrm1_0;
  static_assert(Primate::sub_vrm1_7 == Primate::sub_vrm1_0 + 7,
                "Unexpected subreg numbering");
  if (LMUL == 2) {
    Opcode = Primate::VS2R_V;
    SubRegIdx = Primate::sub_vrm2_0;
    static_assert(Primate::sub_vrm2_3 == Primate::sub_vrm2_0 + 3,
                  "Unexpected subreg numbering");
  } else if (LMUL == 4) {
    Opcode = Primate::VS4R_V;
    SubRegIdx = Primate::sub_vrm4_0;
    static_assert(Primate::sub_vrm4_1 == Primate::sub_vrm4_0 + 1,
                  "Unexpected subreg numbering");
  } else
    assert(LMUL == 1 && "LMUL must be 1, 2, or 4.");

  for (unsigned I = 0; I < NF; ++I) {
    BuildMI(MBB, MBBI, DL, TII->get(Opcode))
        .addReg(TRI->getSubReg(SrcReg, SubRegIdx + I))
        .addReg(Base)
        .addMemOperand(*(MBBI->memoperands_begin()));
    if (I != NF - 1)
      BuildMI(MBB, MBBI, DL, TII->get(Primate::ADD), Base)
          .addReg(Base)
          .addReg(VL);
  }
  MBBI->eraseFromParent();
  return true;
}

bool PrimateExpandPseudo::expandVRELOAD(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI) {
  const TargetRegisterInfo *TRI =
      MBB.getParent()->getSubtarget().getRegisterInfo();
  DebugLoc DL = MBBI->getDebugLoc();
  Register DestReg = MBBI->getOperand(0).getReg();
  Register Base = MBBI->getOperand(1).getReg();
  Register VL = MBBI->getOperand(2).getReg();
  auto ZvlssegInfo = TII->isPRVSpillForZvlsseg(MBBI->getOpcode());
  if (!ZvlssegInfo)
    return false;
  unsigned NF = ZvlssegInfo->first;
  unsigned LMUL = ZvlssegInfo->second;
  assert(NF * LMUL <= 8 && "Invalid NF/LMUL combinations.");
  unsigned Opcode = Primate::VL1RE8_V;
  unsigned SubRegIdx = Primate::sub_vrm1_0;
  static_assert(Primate::sub_vrm1_7 == Primate::sub_vrm1_0 + 7,
                "Unexpected subreg numbering");
  if (LMUL == 2) {
    Opcode = Primate::VL2RE8_V;
    SubRegIdx = Primate::sub_vrm2_0;
    static_assert(Primate::sub_vrm2_3 == Primate::sub_vrm2_0 + 3,
                  "Unexpected subreg numbering");
  } else if (LMUL == 4) {
    Opcode = Primate::VL4RE8_V;
    SubRegIdx = Primate::sub_vrm4_0;
    static_assert(Primate::sub_vrm4_1 == Primate::sub_vrm4_0 + 1,
                  "Unexpected subreg numbering");
  } else
    assert(LMUL == 1 && "LMUL must be 1, 2, or 4.");

  for (unsigned I = 0; I < NF; ++I) {
    BuildMI(MBB, MBBI, DL, TII->get(Opcode),
            TRI->getSubReg(DestReg, SubRegIdx + I))
        .addReg(Base)
        .addMemOperand(*(MBBI->memoperands_begin()));
    if (I != NF - 1)
      BuildMI(MBB, MBBI, DL, TII->get(Primate::ADD), Base)
          .addReg(Base)
          .addReg(VL);
  }
  MBBI->eraseFromParent();
  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(PrimateExpandPseudo, "primate-expand-pseudo",
                Primate_EXPAND_PSEUDO_NAME, false, false)
namespace llvm {

FunctionPass *createPrimateExpandPseudoPass() { return new PrimateExpandPseudo(); }

} // end of namespace llvm
