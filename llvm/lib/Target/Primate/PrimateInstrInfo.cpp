//===-- PrimateInstrInfo.cpp - Primate Instruction Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Primate implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "PrimateInstrInfo.h"
#include "MCTargetDesc/PrimateMatInt.h"
#include "Primate.h"
#include "PrimateMachineFunctionInfo.h"
#include "PrimateSubtarget.h"
#include "PrimateTargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define GEN_CHECK_COMPRESS_INSTR
#include "PrimateGenCompressInstEmitter.inc"

#include "PrimateGenDFAPacketizer.inc"

#define GET_INSTRINFO_CTOR_DTOR
#include "PrimateGenInstrInfo.inc"

namespace llvm {
namespace PrimateVPseudosTable {

using namespace Primate;

#define GET_PrimateVPseudosTable_IMPL
#include "PrimateGenSearchableTables.inc"

} // namespace PrimateVPseudosTable
} // namespace llvm

PrimateInstrInfo::PrimateInstrInfo(PrimateSubtarget &STI)
    : PrimateGenInstrInfo(Primate::ADJCALLSTACKDOWN, Primate::ADJCALLSTACKUP),
      STI(STI) {}

MCInst PrimateInstrInfo::getNop() const {
  if (STI.getFeatureBits()[Primate::FeatureStdExtC])
    return MCInstBuilder(Primate::C_NOP);
  return MCInstBuilder(Primate::ADDI)
      .addReg(Primate::X0)
      .addReg(Primate::X0)
      .addImm(0);
}

unsigned PrimateInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case Primate::LB:
  case Primate::LBU:
  case Primate::LH:
  case Primate::LHU:
  case Primate::FLH:
  case Primate::LW:
  case Primate::FLW:
  case Primate::LWU:
  case Primate::LD:
  case Primate::FLD:
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

unsigned PrimateInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case Primate::SB:
  case Primate::SH:
  case Primate::SW:
  case Primate::FSH:
  case Primate::FSW:
  case Primate::SD:
  case Primate::FSD:
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

static bool forwardCopyWillClobberTuple(unsigned DstReg, unsigned SrcReg,
                                        unsigned NumRegs) {
  // We really want the positive remainder mod 32 here, that happens to be
  // easily obtainable with a mask.
  return ((DstReg - SrcReg) & 0x1f) < NumRegs;
}

void PrimateInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 const DebugLoc &DL, MCRegister DstReg,
                                 MCRegister SrcReg, bool KillSrc) const {
  if (Primate::GPRRegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(Primate::ADDI), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(0);
    return;
  }

  // FPR->FPR copies and VR->VR copies.
  unsigned Opc;
  bool IsScalableVector = true;
  unsigned NF = 1;
  unsigned LMul = 1;
  unsigned SubRegIdx = Primate::sub_vrm1_0;
  if (Primate::FPR16RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::FSGNJ_H;
    IsScalableVector = false;
  } else if (Primate::FPR32RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::FSGNJ_S;
    IsScalableVector = false;
  } else if (Primate::FPR64RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::FSGNJ_D;
    IsScalableVector = false;
  } else if (Primate::VRRegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
  } else if (Primate::VRM2RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV2R_V;
  } else if (Primate::VRM4RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV4R_V;
  } else if (Primate::VRM8RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV8R_V;
  } else if (Primate::VRN2M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 2;
    LMul = 1;
  } else if (Primate::VRN2M2RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV2R_V;
    SubRegIdx = Primate::sub_vrm2_0;
    NF = 2;
    LMul = 2;
  } else if (Primate::VRN2M4RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV4R_V;
    SubRegIdx = Primate::sub_vrm4_0;
    NF = 2;
    LMul = 4;
  } else if (Primate::VRN3M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 3;
    LMul = 1;
  } else if (Primate::VRN3M2RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV2R_V;
    SubRegIdx = Primate::sub_vrm2_0;
    NF = 3;
    LMul = 2;
  } else if (Primate::VRN4M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 4;
    LMul = 1;
  } else if (Primate::VRN4M2RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV2R_V;
    SubRegIdx = Primate::sub_vrm2_0;
    NF = 4;
    LMul = 2;
  } else if (Primate::VRN5M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 5;
    LMul = 1;
  } else if (Primate::VRN6M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 6;
    LMul = 1;
  } else if (Primate::VRN7M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 7;
    LMul = 1;
  } else if (Primate::VRN8M1RegClass.contains(DstReg, SrcReg)) {
    Opc = Primate::PseudoVMV1R_V;
    SubRegIdx = Primate::sub_vrm1_0;
    NF = 8;
    LMul = 1;
  } else {
    llvm_unreachable("Impossible reg-to-reg copy");
  }

  if (IsScalableVector) {
    if (NF == 1) {
      BuildMI(MBB, MBBI, DL, get(Opc), DstReg)
          .addReg(SrcReg, getKillRegState(KillSrc));
    } else {
      const TargetRegisterInfo *TRI = STI.getRegisterInfo();

      int I = 0, End = NF, Incr = 1;
      unsigned SrcEncoding = TRI->getEncodingValue(SrcReg);
      unsigned DstEncoding = TRI->getEncodingValue(DstReg);
      if (forwardCopyWillClobberTuple(DstEncoding, SrcEncoding, NF * LMul)) {
        I = NF - 1;
        End = -1;
        Incr = -1;
      }

      for (; I != End; I += Incr) {
        BuildMI(MBB, MBBI, DL, get(Opc), TRI->getSubReg(DstReg, SubRegIdx + I))
            .addReg(TRI->getSubReg(SrcReg, SubRegIdx + I),
                    getKillRegState(KillSrc));
      }
    }
  } else {
    BuildMI(MBB, MBBI, DL, get(Opc), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addReg(SrcReg, getKillRegState(KillSrc));
  }
}

void PrimateInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register SrcReg, bool IsKill, int FI,
                                         const TargetRegisterClass *RC,
                                         const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();

  unsigned Opcode;
  bool IsScalableVector = true;
  bool IsZvlsseg = true;
  if (Primate::GPRRegClass.hasSubClassEq(RC)) {
    Opcode = TRI->getRegSizeInBits(Primate::GPRRegClass) == 32 ?
             Primate::SW : Primate::SD;
    IsScalableVector = false;
  } else if (Primate::FPR16RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::FSH;
    IsScalableVector = false;
  } else if (Primate::FPR32RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::FSW;
    IsScalableVector = false;
  } else if (Primate::FPR64RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::FSD;
    IsScalableVector = false;
  } else if (Primate::VRRegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVSPILL_M1;
    IsZvlsseg = false;
  } else if (Primate::VRM2RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVSPILL_M2;
    IsZvlsseg = false;
  } else if (Primate::VRM4RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVSPILL_M4;
    IsZvlsseg = false;
  } else if (Primate::VRM8RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVSPILL_M8;
    IsZvlsseg = false;
  } else if (Primate::VRN2M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL2_M1;
  else if (Primate::VRN2M2RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL2_M2;
  else if (Primate::VRN2M4RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL2_M4;
  else if (Primate::VRN3M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL3_M1;
  else if (Primate::VRN3M2RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL3_M2;
  else if (Primate::VRN4M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL4_M1;
  else if (Primate::VRN4M2RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL4_M2;
  else if (Primate::VRN5M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL5_M1;
  else if (Primate::VRN6M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL6_M1;
  else if (Primate::VRN7M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL7_M1;
  else if (Primate::VRN8M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVSPILL8_M1;
  else
    llvm_unreachable("Can't store this register to stack slot");

  if (IsScalableVector) {
    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
        MemoryLocation::UnknownSize, MFI.getObjectAlign(FI));

    MFI.setStackID(FI, TargetStackID::ScalableVector);
    auto MIB = BuildMI(MBB, I, DL, get(Opcode))
                   .addReg(SrcReg, getKillRegState(IsKill))
                   .addFrameIndex(FI)
                   .addMemOperand(MMO);
    if (IsZvlsseg) {
      // For spilling/reloading Zvlsseg registers, append the dummy field for
      // the scaled vector length. The argument will be used when expanding
      // these pseudo instructions.
      MIB.addReg(Primate::X0);
    }
  } else {
    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
        MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

    BuildMI(MBB, I, DL, get(Opcode))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addFrameIndex(FI)
        .addImm(0)
        .addMemOperand(MMO);
  }
}

void PrimateInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          Register DstReg, int FI,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();

  unsigned Opcode;
  bool IsScalableVector = true;
  bool IsZvlsseg = true;
  if (Primate::GPRRegClass.hasSubClassEq(RC)) {
    Opcode = TRI->getRegSizeInBits(Primate::GPRRegClass) == 32 ?
             Primate::LW : Primate::LD;
    IsScalableVector = false;
  } else if (Primate::FPR16RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::FLH;
    IsScalableVector = false;
  } else if (Primate::FPR32RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::FLW;
    IsScalableVector = false;
  } else if (Primate::FPR64RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::FLD;
    IsScalableVector = false;
  } else if (Primate::VRRegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVRELOAD_M1;
    IsZvlsseg = false;
  } else if (Primate::VRM2RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVRELOAD_M2;
    IsZvlsseg = false;
  } else if (Primate::VRM4RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVRELOAD_M4;
    IsZvlsseg = false;
  } else if (Primate::VRM8RegClass.hasSubClassEq(RC)) {
    Opcode = Primate::PseudoVRELOAD_M8;
    IsZvlsseg = false;
  } else if (Primate::VRN2M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD2_M1;
  else if (Primate::VRN2M2RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD2_M2;
  else if (Primate::VRN2M4RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD2_M4;
  else if (Primate::VRN3M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD3_M1;
  else if (Primate::VRN3M2RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD3_M2;
  else if (Primate::VRN4M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD4_M1;
  else if (Primate::VRN4M2RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD4_M2;
  else if (Primate::VRN5M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD5_M1;
  else if (Primate::VRN6M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD6_M1;
  else if (Primate::VRN7M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD7_M1;
  else if (Primate::VRN8M1RegClass.hasSubClassEq(RC))
    Opcode = Primate::PseudoVRELOAD8_M1;
  else
    llvm_unreachable("Can't load this register from stack slot");

  if (IsScalableVector) {
    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
        MemoryLocation::UnknownSize, MFI.getObjectAlign(FI));

    MFI.setStackID(FI, TargetStackID::ScalableVector);
    auto MIB = BuildMI(MBB, I, DL, get(Opcode), DstReg)
                   .addFrameIndex(FI)
                   .addMemOperand(MMO);
    if (IsZvlsseg) {
      // For spilling/reloading Zvlsseg registers, append the dummy field for
      // the scaled vector length. The argument will be used when expanding
      // these pseudo instructions.
      MIB.addReg(Primate::X0);
    }
  } else {
    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
        MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

    BuildMI(MBB, I, DL, get(Opcode), DstReg)
        .addFrameIndex(FI)
        .addImm(0)
        .addMemOperand(MMO);
  }
}

void PrimateInstrInfo::movImm(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI,
                            const DebugLoc &DL, Register DstReg, uint64_t Val,
                            MachineInstr::MIFlag Flag) const {
  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  Register SrcReg = Primate::X0;
  Register Result = MRI.createVirtualRegister(&Primate::GPRRegClass);
  unsigned Num = 0;

  if (!STI.is64Bit() && !isInt<32>(Val))
    report_fatal_error("Should only materialize 32-bit constants for PR32");

  PrimateMatInt::InstSeq Seq =
      PrimateMatInt::generateInstSeq(Val, STI.getFeatureBits());
  assert(!Seq.empty());

  for (PrimateMatInt::Inst &Inst : Seq) {
    // Write the final result to DstReg if it's the last instruction in the Seq.
    // Otherwise, write the result to the temp register.
    if (++Num == Seq.size())
      Result = DstReg;

    if (Inst.Opc == Primate::LUI) {
      BuildMI(MBB, MBBI, DL, get(Primate::LUI), Result)
          .addImm(Inst.Imm)
          .setMIFlag(Flag);
    } else if (Inst.Opc == Primate::ADDUW) {
      BuildMI(MBB, MBBI, DL, get(Primate::ADDUW), Result)
          .addReg(SrcReg, RegState::Kill)
          .addReg(Primate::X0)
          .setMIFlag(Flag);
    } else {
      BuildMI(MBB, MBBI, DL, get(Inst.Opc), Result)
          .addReg(SrcReg, RegState::Kill)
          .addImm(Inst.Imm)
          .setMIFlag(Flag);
    }
    // Only the first instruction has X0 as its source.
    SrcReg = Result;
  }
}

// The contents of values added to Cond are not examined outside of
// PrimateInstrInfo, giving us flexibility in what to push to it. For Primate, we
// push BranchOpcode, Reg1, Reg2.
static void parseCondBranch(MachineInstr &LastInst, MachineBasicBlock *&Target,
                            SmallVectorImpl<MachineOperand> &Cond) {
  // Block ends with fall-through condbranch.
  assert(LastInst.getDesc().isConditionalBranch() &&
         "Unknown conditional branch");
  Target = LastInst.getOperand(2).getMBB();
  Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode()));
  Cond.push_back(LastInst.getOperand(0));
  Cond.push_back(LastInst.getOperand(1));
}

static unsigned getOppositeBranchOpcode(int Opc) {
  switch (Opc) {
  default:
    llvm_unreachable("Unrecognized conditional branch");
  case Primate::BEQ:
    return Primate::BNE;
  case Primate::BNE:
    return Primate::BEQ;
  case Primate::BLT:
    return Primate::BGE;
  case Primate::BGE:
    return Primate::BLT;
  case Primate::BLTU:
    return Primate::BGEU;
  case Primate::BGEU:
    return Primate::BLTU;
  }
}

bool PrimateInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the number of terminators and find the first unconditional or
  // indirect branch.
  MachineBasicBlock::iterator FirstUncondOrIndirectBr = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse(); J != MBB.rend() && isUnpredicatedTerminator(*J);
       J++) {
    NumTerminators++;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch()) {
      FirstUncondOrIndirectBr = J.getReverse();
    }
  }

  // If AllowModify is true, we can erase any terminators after
  // FirstUncondOrIndirectBR.
  if (AllowModify && FirstUncondOrIndirectBr != MBB.end()) {
    while (std::next(FirstUncondOrIndirectBr) != MBB.end()) {
      std::next(FirstUncondOrIndirectBr)->eraseFromParent();
      NumTerminators--;
    }
    I = FirstUncondOrIndirectBr;
  }

  // We can't handle blocks that end in an indirect branch.
  if (I->getDesc().isIndirectBranch())
    return true;

  // We can't handle blocks with more than 2 terminators.
  if (NumTerminators > 2)
    return true;

  // Handle a single unconditional branch.
  if (NumTerminators == 1 && I->getDesc().isUnconditionalBranch()) {
    TBB = getBranchDestBlock(*I);
    return false;
  }

  // Handle a single conditional branch.
  if (NumTerminators == 1 && I->getDesc().isConditionalBranch()) {
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  // Handle a conditional branch followed by an unconditional branch.
  if (NumTerminators == 2 && std::prev(I)->getDesc().isConditionalBranch() &&
      I->getDesc().isUnconditionalBranch()) {
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = getBranchDestBlock(*I);
    return false;
  }

  // Otherwise, we can't handle this.
  return true;
}

unsigned PrimateInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return 0;

  if (!I->getDesc().isUnconditionalBranch() &&
      !I->getDesc().isConditionalBranch())
    return 0;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();

  I = MBB.end();

  if (I == MBB.begin())
    return 1;
  --I;
  if (!I->getDesc().isConditionalBranch())
    return 1;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  return 2;
}

// Inserts a branch into the end of the specific MachineBasicBlock, returning
// the number of instructions inserted.
unsigned PrimateInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 3 || Cond.size() == 0) &&
         "Primate branch conditions have two components!");

  // Unconditional branch.
  if (Cond.empty()) {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(Primate::PseudoBR)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  // Either a one or two-way conditional branch.
  unsigned Opc = Cond[0].getImm();
  MachineInstr &CondMI =
      *BuildMI(&MBB, DL, get(Opc)).add(Cond[1]).add(Cond[2]).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(CondMI);

  // One-way conditional branch.
  if (!FBB)
    return 1;

  // Two-way conditional branch.
  MachineInstr &MI = *BuildMI(&MBB, DL, get(Primate::PseudoBR)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(MI);
  return 2;
}

unsigned PrimateInstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                              MachineBasicBlock &DestBB,
                                              const DebugLoc &DL,
                                              int64_t BrOffset,
                                              RegScavenger *RS) const {
  assert(RS && "RegScavenger required for long branching");
  assert(MBB.empty() &&
         "new block should be inserted for expanding unconditional branch");
  assert(MBB.pred_size() == 1);

  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();

  if (!isInt<32>(BrOffset))
    report_fatal_error(
        "Branch offsets outside of the signed 32-bit range not supported");

  // FIXME: A virtual register must be used initially, as the register
  // scavenger won't work with empty blocks (SIInstrInfo::insertIndirectBranch
  // uses the same workaround).
  Register ScratchReg = MRI.createVirtualRegister(&Primate::GPRRegClass);
  auto II = MBB.end();

  MachineInstr &MI = *BuildMI(MBB, II, DL, get(Primate::PseudoJump))
                          .addReg(ScratchReg, RegState::Define | RegState::Dead)
                          .addMBB(&DestBB, PrimateII::MO_CALL);

  RS->enterBasicBlockEnd(MBB);
  unsigned Scav = RS->scavengeRegisterBackwards(Primate::GPRRegClass,
                                                MI.getIterator(), false, 0);
  MRI.replaceRegWith(ScratchReg, Scav);
  MRI.clearVirtRegs();
  RS->setRegUsed(Scav);
  return 8;
}

bool PrimateInstrInfo::expandPostRAPseudo(MachineInstr& MI) const {
  if (MI.getOpcode() == Primate::PseudoInputDone) {
    MachineBasicBlock *MBB = MI.getParent();
    BuildMI(*MBB, MI.getIterator(), MI.getDebugLoc(), get(Primate::INPUT_DONE), Primate::X0)
            .addReg(Primate::X0)
            .addImm(0);
    MI.eraseFromBundle();
    return true;
  }
  // if (MI.getOpcode() == Primate::PseudoInsert) {
  //   MachineBasicBlock *MBB = MI.getParent();
  // //  MachineInstrBuilder builder(&MI);
  //   BuildMI(*MBB, MI.getIterator(), MI.getDebugLoc(), get(Primate::INSERT), MI.getOperand(0).getReg())
  //           .addReg(MI.getOperand(2).getReg())
  //           .addImm(MI.getOperand(3).getImm());
  //   MI.eraseFromBundle();
  //   return true;
  // }
  return false;
}

bool PrimateInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert((Cond.size() == 3) && "Invalid branch condition!");
  Cond[0].setImm(getOppositeBranchOpcode(Cond[0].getImm()));
  return false;
}

MachineBasicBlock *
PrimateInstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert(MI.getDesc().isBranch() && "Unexpected opcode!");
  // The branch target is always the last operand.
  int NumOp = MI.getNumExplicitOperands();
  return MI.getOperand(NumOp - 1).getMBB();
}

bool PrimateInstrInfo::isBranchOffsetInRange(unsigned BranchOp,
                                           int64_t BrOffset) const {
  unsigned XLen = STI.getXLen();
  // Ideally we could determine the supported branch offset from the
  // PrimateII::FormMask, but this can't be used for Pseudo instructions like
  // PseudoBR.
  switch (BranchOp) {
  default:
    llvm_unreachable("Unexpected opcode!");
  case Primate::BEQ:
  case Primate::BNE:
  case Primate::BLT:
  case Primate::BGE:
  case Primate::BLTU:
  case Primate::BGEU:
    return isIntN(13, BrOffset);
  case Primate::JAL:
  case Primate::PseudoBR:
    return isIntN(21, BrOffset);
  case Primate::PseudoJump:
    return isIntN(32, SignExtend64(BrOffset + 0x800, XLen));
  }
}

unsigned PrimateInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();

  switch (Opcode) {
  default: {
    if (MI.getParent() && MI.getParent()->getParent()) {
      const auto MF = MI.getMF();
      const auto &TM = static_cast<const PrimateTargetMachine &>(MF->getTarget());
      const MCRegisterInfo &MRI = *TM.getMCRegisterInfo();
      const MCSubtargetInfo &STI = *TM.getMCSubtargetInfo();
      const PrimateSubtarget &ST = MF->getSubtarget<PrimateSubtarget>();
      if (isCompressibleInst(MI, &ST, MRI, STI))
        return 2;
    }
    return get(Opcode).getSize();
  }
  case Primate::BUNDLE:
    llvm_unreachable("got a bundle");
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  // These values are determined based on PrimateExpandAtomicPseudoInsts,
  // PrimateExpandPseudoInsts and PrimateMCCodeEmitter, depending on where the
  // pseudos are expanded.
  case Primate::PseudoCALLReg:
  case Primate::PseudoCALL:
  case Primate::PseudoJump:
  case Primate::PseudoTAIL:
  case Primate::PseudoLLA:
  case Primate::PseudoLA:
  case Primate::PseudoLA_TLS_IE:
  case Primate::PseudoLA_TLS_GD:
    return 8;
  case Primate::PseudoAtomicLoadNand32:
  case Primate::PseudoAtomicLoadNand64:
    return 20;
  case Primate::PseudoMaskedAtomicSwap32:
  case Primate::PseudoMaskedAtomicLoadAdd32:
  case Primate::PseudoMaskedAtomicLoadSub32:
    return 28;
  case Primate::PseudoMaskedAtomicLoadNand32:
    return 32;
  case Primate::PseudoMaskedAtomicLoadMax32:
  case Primate::PseudoMaskedAtomicLoadMin32:
    return 44;
  case Primate::PseudoMaskedAtomicLoadUMax32:
  case Primate::PseudoMaskedAtomicLoadUMin32:
    return 36;
  case Primate::PseudoCmpXchg32:
  case Primate::PseudoCmpXchg64:
    return 16;
  case Primate::PseudoMaskedCmpXchg32:
    return 32;
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction &MF = *MI.getParent()->getParent();
    const auto &TM = static_cast<const PrimateTargetMachine &>(MF.getTarget());
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *TM.getMCAsmInfo());
  }
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
  case Primate::PseudoVRELOAD8_M1: {
    // The values are determined based on expandVSPILL and expandVRELOAD that
    // expand the pseudos depending on NF.
    unsigned NF = isPRVSpillForZvlsseg(Opcode)->first;
    return 4 * (2 * NF - 1);
  }
  }
}

bool PrimateInstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case Primate::FSGNJ_D:
  case Primate::FSGNJ_S:
    // The canonical floating-point move is fsgnj rd, rs, rs.
    return MI.getOperand(1).isReg() && MI.getOperand(2).isReg() &&
           MI.getOperand(1).getReg() == MI.getOperand(2).getReg();
  case Primate::ADDI:
  case Primate::ORI:
  case Primate::XORI:
    return (MI.getOperand(1).isReg() &&
            MI.getOperand(1).getReg() == Primate::X0) ||
           (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}

Optional<DestSourcePair>
PrimateInstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.isMoveReg())
    return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
  switch (MI.getOpcode()) {
  default:
    break;
  case Primate::ADDI:
    // Operand 1 can be a frameindex but callers expect registers
    if (MI.getOperand(1).isReg() && MI.getOperand(2).isImm() &&
        MI.getOperand(2).getImm() == 0)
      return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
    break;
  case Primate::FSGNJ_D:
  case Primate::FSGNJ_S:
    // The canonical floating-point move is fsgnj rd, rs, rs.
    if (MI.getOperand(1).isReg() && MI.getOperand(2).isReg() &&
        MI.getOperand(1).getReg() == MI.getOperand(2).getReg())
      return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
    break;
  }
  return None;
}

bool PrimateInstrInfo::verifyInstruction(const MachineInstr &MI,
                                       StringRef &ErrInfo) const {
  const MCInstrInfo *MCII = STI.getInstrInfo();
  MCInstrDesc const &Desc = MCII->get(MI.getOpcode());

  for (auto &OI : enumerate(Desc.operands())) {
    unsigned OpType = OI.value().OperandType;
    if (OpType >= PrimateOp::OPERAND_FIRST_Primate_IMM &&
        OpType <= PrimateOp::OPERAND_LAST_Primate_IMM) {
      const MachineOperand &MO = MI.getOperand(OI.index());
      if (MO.isImm()) {
        int64_t Imm = MO.getImm();
        bool Ok;
        switch (OpType) {
        default:
          llvm_unreachable("Unexpected operand type");
        case PrimateOp::OPERAND_UIMM4:
          Ok = isUInt<4>(Imm);
          break;
        case PrimateOp::OPERAND_UIMM5:
          Ok = isUInt<5>(Imm);
          break;
        case PrimateOp::OPERAND_UIMM12:
          Ok = isUInt<12>(Imm);
          break;
        case PrimateOp::OPERAND_SIMM12:
          Ok = isInt<12>(Imm);
          break;
        case PrimateOp::OPERAND_UIMM20:
          Ok = isUInt<20>(Imm);
          break;
        case PrimateOp::OPERAND_UIMMLOG2XLEN:
          if (STI.getTargetTriple().isArch64Bit())
            Ok = isUInt<6>(Imm);
          else
            Ok = isUInt<5>(Imm);
          break;
        }
        if (!Ok) {
          ErrInfo = "Invalid immediate";
          return false;
        }
      }
    }
  }

  return true;
}

// Return true if get the base operand, byte offset of an instruction and the
// memory width. Width is the size of memory that is being loaded/stored.
bool PrimateInstrInfo::getMemOperandWithOffsetWidth(
    const MachineInstr &LdSt, const MachineOperand *&BaseReg, int64_t &Offset,
    unsigned &Width, const TargetRegisterInfo *TRI) const {
  if (!LdSt.mayLoadOrStore())
    return false;

  // Here we assume the standard Primate ISA, which uses a base+offset
  // addressing mode. You'll need to relax these conditions to support custom
  // load/stores instructions.
  if (LdSt.getNumExplicitOperands() != 3)
    return false;
  if (!LdSt.getOperand(1).isReg() || !LdSt.getOperand(2).isImm())
    return false;

  if (!LdSt.hasOneMemOperand())
    return false;

  Width = (*LdSt.memoperands_begin())->getSize();
  BaseReg = &LdSt.getOperand(1);
  Offset = LdSt.getOperand(2).getImm();
  return true;
}

DFAPacketizer *PrimateInstrInfo::CreateTargetScheduleState(
    const TargetSubtargetInfo &STI) const {
  const InstrItineraryData *II = STI.getInstrItineraryData();
  return static_cast<const PrimateSubtarget&>(STI).createDFAPacketizer(II);
}

bool PrimateInstrInfo::areMemAccessesTriviallyDisjoint(
    const MachineInstr &MIa, const MachineInstr &MIb) const {
  assert(MIa.mayLoadOrStore() && "MIa must be a load or store.");
  assert(MIb.mayLoadOrStore() && "MIb must be a load or store.");

  if (MIa.hasUnmodeledSideEffects() || MIb.hasUnmodeledSideEffects() ||
      MIa.hasOrderedMemoryRef() || MIb.hasOrderedMemoryRef())
    return false;

  // Retrieve the base register, offset from the base register and width. Width
  // is the size of memory that is being loaded/stored (e.g. 1, 2, 4).  If
  // base registers are identical, and the offset of a lower memory access +
  // the width doesn't overlap the offset of a higher memory access,
  // then the memory accesses are different.
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();
  const MachineOperand *BaseOpA = nullptr, *BaseOpB = nullptr;
  int64_t OffsetA = 0, OffsetB = 0;
  unsigned int WidthA = 0, WidthB = 0;
  if (getMemOperandWithOffsetWidth(MIa, BaseOpA, OffsetA, WidthA, TRI) &&
      getMemOperandWithOffsetWidth(MIb, BaseOpB, OffsetB, WidthB, TRI)) {
    if (BaseOpA->isIdenticalTo(*BaseOpB)) {
      int LowOffset = std::min(OffsetA, OffsetB);
      int HighOffset = std::max(OffsetA, OffsetB);
      int LowWidth = (LowOffset == OffsetA) ? WidthA : WidthB;
      if (LowOffset + LowWidth <= HighOffset)
        return true;
    }
  }
  return false;
}

std::pair<unsigned, unsigned>
PrimateInstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  const unsigned Mask = PrimateII::MO_DIRECT_FLAG_MASK;
  return std::make_pair(TF & Mask, TF & ~Mask);
}

ArrayRef<std::pair<unsigned, const char *>>
PrimateInstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  using namespace PrimateII;
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_CALL, "primate-call"},
      {MO_PLT, "primate-plt"},
      {MO_LO, "primate-lo"},
      {MO_HI, "primate-hi"},
      {MO_PCREL_LO, "primate-pcrel-lo"},
      {MO_PCREL_HI, "primate-pcrel-hi"},
      {MO_GOT_HI, "primate-got-hi"},
      {MO_TPREL_LO, "primate-tprel-lo"},
      {MO_TPREL_HI, "primate-tprel-hi"},
      {MO_TPREL_ADD, "primate-tprel-add"},
      {MO_TLS_GOT_HI, "primate-tls-got-hi"},
      {MO_TLS_GD_HI, "primate-tls-gd-hi"}};
  return makeArrayRef(TargetFlags);
}
bool PrimateInstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();

  // Can F be deduplicated by the linker? If it can, don't outline from it.
  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;

  // Don't outline from functions with section markings; the program could
  // expect that all the code is in the named section.
  if (F.hasSection())
    return false;

  // It's safe to outline from MF.
  return true;
}

bool PrimateInstrInfo::isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                                            unsigned &Flags) const {
  // More accurate safety checking is done in getOutliningCandidateInfo.
  return true;
}

// Enum values indicating how an outlined call should be constructed.
enum MachineOutlinerConstructionID {
  MachineOutlinerDefault
};

outliner::OutlinedFunction PrimateInstrInfo::getOutliningCandidateInfo(
    std::vector<outliner::Candidate> &RepeatedSequenceLocs) const {

  // First we need to filter out candidates where the X5 register (IE t0) can't
  // be used to setup the function call.
  auto CannotInsertCall = [](outliner::Candidate &C) {
    const TargetRegisterInfo *TRI = C.getMF()->getSubtarget().getRegisterInfo();

    C.initLRU(*TRI);
    LiveRegUnits LRU = C.LRU;
    return !LRU.available(Primate::X5);
  };

  llvm::erase_if(RepeatedSequenceLocs, CannotInsertCall);

  // If the sequence doesn't have enough candidates left, then we're done.
  if (RepeatedSequenceLocs.size() < 2)
    return outliner::OutlinedFunction();

  unsigned SequenceSize = 0;

  auto I = RepeatedSequenceLocs[0].front();
  auto E = std::next(RepeatedSequenceLocs[0].back());
  for (; I != E; ++I)
    SequenceSize += getInstSizeInBytes(*I);

  // call t0, function = 8 bytes.
  unsigned CallOverhead = 8;
  for (auto &C : RepeatedSequenceLocs)
    C.setCallInfo(MachineOutlinerDefault, CallOverhead);

  // jr t0 = 4 bytes, 2 bytes if compressed instructions are enabled.
  unsigned FrameOverhead = 4;
  if (RepeatedSequenceLocs[0].getMF()->getSubtarget()
          .getFeatureBits()[Primate::FeatureStdExtC])
    FrameOverhead = 2;

  return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize,
                                    FrameOverhead, MachineOutlinerDefault);
}

outliner::InstrType
PrimateInstrInfo::getOutliningType(MachineBasicBlock::iterator &MBBI,
                                 unsigned Flags) const {
  MachineInstr &MI = *MBBI;
  MachineBasicBlock *MBB = MI.getParent();
  const TargetRegisterInfo *TRI =
      MBB->getParent()->getSubtarget().getRegisterInfo();

  // Positions generally can't safely be outlined.
  if (MI.isPosition()) {
    // We can manually strip out CFI instructions later.
    if (MI.isCFIInstruction())
      return outliner::InstrType::Invisible;

    return outliner::InstrType::Illegal;
  }

  // Don't trust the user to write safe inline assembly.
  if (MI.isInlineAsm())
    return outliner::InstrType::Illegal;

  // We can't outline branches to other basic blocks.
  if (MI.isTerminator() && !MBB->succ_empty())
    return outliner::InstrType::Illegal;

  // We need support for tail calls to outlined functions before return
  // statements can be allowed.
  if (MI.isReturn())
    return outliner::InstrType::Illegal;

  // Don't allow modifying the X5 register which we use for return addresses for
  // these outlined functions.
  if (MI.modifiesRegister(Primate::X5, TRI) ||
      MI.getDesc().hasImplicitDefOfPhysReg(Primate::X5))
    return outliner::InstrType::Illegal;

  // Make sure the operands don't reference something unsafe.
  for (const auto &MO : MI.operands())
    if (MO.isMBB() || MO.isBlockAddress() || MO.isCPI())
      return outliner::InstrType::Illegal;

  // Don't allow instructions which won't be materialized to impact outlining
  // analysis.
  if (MI.isMetaInstruction())
    return outliner::InstrType::Invisible;

  return outliner::InstrType::Legal;
}

void PrimateInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {

  // Strip out any CFI instructions
  bool Changed = true;
  while (Changed) {
    Changed = false;
    auto I = MBB.begin();
    auto E = MBB.end();
    for (; I != E; ++I) {
      if (I->isCFIInstruction()) {
        I->removeFromParent();
        Changed = true;
        break;
      }
    }
  }

  MBB.addLiveIn(Primate::X5);

  // Add in a return instruction to the end of the outlined frame.
  MBB.insert(MBB.end(), BuildMI(MF, DebugLoc(), get(Primate::JALR))
      .addReg(Primate::X0, RegState::Define)
      .addReg(Primate::X5)
      .addImm(0));
}

MachineBasicBlock::iterator PrimateInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, const outliner::Candidate &C) const {

  // Add in a call instruction to the outlined function at the given location.
  It = MBB.insert(It,
                  BuildMI(MF, DebugLoc(), get(Primate::PseudoCALLReg), Primate::X5)
                      .addGlobalAddress(M.getNamedValue(MF.getName()), 0,
                                        PrimateII::MO_CALL));
  return It;
}

// clang-format off
#define CASE_VFMA_OPCODE_COMMON(OP, TYPE, LMUL)                                \
  Primate::PseudoV##OP##_##TYPE##_##LMUL##_COMMUTABLE

#define CASE_VFMA_OPCODE_LMULS(OP, TYPE)                                       \
  CASE_VFMA_OPCODE_COMMON(OP, TYPE, MF8):                                      \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, MF4):                                 \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, MF2):                                 \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M1):                                  \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M2):                                  \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M4):                                  \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M8)

#define CASE_VFMA_SPLATS(OP)                                                   \
  CASE_VFMA_OPCODE_LMULS(OP, VF16):                                            \
  case CASE_VFMA_OPCODE_LMULS(OP, VF32):                                       \
  case CASE_VFMA_OPCODE_LMULS(OP, VF64)
// clang-format on

bool PrimateInstrInfo::findCommutedOpIndices(const MachineInstr &MI,
                                           unsigned &SrcOpIdx1,
                                           unsigned &SrcOpIdx2) const {
  const MCInstrDesc &Desc = MI.getDesc();
  if (!Desc.isCommutable())
    return false;

  switch (MI.getOpcode()) {
  case CASE_VFMA_SPLATS(FMADD):
  case CASE_VFMA_SPLATS(FMSUB):
  case CASE_VFMA_SPLATS(FMACC):
  case CASE_VFMA_SPLATS(FMSAC):
  case CASE_VFMA_SPLATS(FNMADD):
  case CASE_VFMA_SPLATS(FNMSUB):
  case CASE_VFMA_SPLATS(FNMACC):
  case CASE_VFMA_SPLATS(FNMSAC):
  case CASE_VFMA_OPCODE_LMULS(FMACC, VV):
  case CASE_VFMA_OPCODE_LMULS(FMSAC, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMACC, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMSAC, VV):
  case CASE_VFMA_OPCODE_LMULS(MADD, VX):
  case CASE_VFMA_OPCODE_LMULS(NMSUB, VX):
  case CASE_VFMA_OPCODE_LMULS(MACC, VX):
  case CASE_VFMA_OPCODE_LMULS(NMSAC, VX):
  case CASE_VFMA_OPCODE_LMULS(MACC, VV):
  case CASE_VFMA_OPCODE_LMULS(NMSAC, VV): {
    // For these instructions we can only swap operand 1 and operand 3 by
    // changing the opcode.
    unsigned CommutableOpIdx1 = 1;
    unsigned CommutableOpIdx2 = 3;
    if (!fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, CommutableOpIdx1,
                              CommutableOpIdx2))
      return false;
    return true;
  }
  case CASE_VFMA_OPCODE_LMULS(FMADD, VV):
  case CASE_VFMA_OPCODE_LMULS(FMSUB, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMADD, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMSUB, VV):
  case CASE_VFMA_OPCODE_LMULS(MADD, VV):
  case CASE_VFMA_OPCODE_LMULS(NMSUB, VV): {
    // For these instructions we have more freedom. We can commute with the
    // other multiplicand or with the addend/subtrahend/minuend.

    // Any fixed operand must be from source 1, 2 or 3.
    if (SrcOpIdx1 != CommuteAnyOperandIndex && SrcOpIdx1 > 3)
      return false;
    if (SrcOpIdx2 != CommuteAnyOperandIndex && SrcOpIdx2 > 3)
      return false;

    // It both ops are fixed one must be the tied source.
    if (SrcOpIdx1 != CommuteAnyOperandIndex &&
        SrcOpIdx2 != CommuteAnyOperandIndex && SrcOpIdx1 != 1 && SrcOpIdx2 != 1)
      return false;

    // Look for two different register operands assumed to be commutable
    // regardless of the FMA opcode. The FMA opcode is adjusted later if
    // needed.
    if (SrcOpIdx1 == CommuteAnyOperandIndex ||
        SrcOpIdx2 == CommuteAnyOperandIndex) {
      // At least one of operands to be commuted is not specified and
      // this method is free to choose appropriate commutable operands.
      unsigned CommutableOpIdx1 = SrcOpIdx1;
      if (SrcOpIdx1 == SrcOpIdx2) {
        // Both of operands are not fixed. Set one of commutable
        // operands to the tied source.
        CommutableOpIdx1 = 1;
      } else if (SrcOpIdx1 == CommuteAnyOperandIndex) {
        // Only one of the operands is not fixed.
        CommutableOpIdx1 = SrcOpIdx2;
      }

      // CommutableOpIdx1 is well defined now. Let's choose another commutable
      // operand and assign its index to CommutableOpIdx2.
      unsigned CommutableOpIdx2;
      if (CommutableOpIdx1 != 1) {
        // If we haven't already used the tied source, we must use it now.
        CommutableOpIdx2 = 1;
      } else {
        Register Op1Reg = MI.getOperand(CommutableOpIdx1).getReg();

        // The commuted operands should have different registers.
        // Otherwise, the commute transformation does not change anything and
        // is useless. We use this as a hint to make our decision.
        if (Op1Reg != MI.getOperand(2).getReg())
          CommutableOpIdx2 = 2;
        else
          CommutableOpIdx2 = 3;
      }

      // Assign the found pair of commutable indices to SrcOpIdx1 and
      // SrcOpIdx2 to return those values.
      if (!fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, CommutableOpIdx1,
                                CommutableOpIdx2))
        return false;
    }

    return true;
  }
  }

  return TargetInstrInfo::findCommutedOpIndices(MI, SrcOpIdx1, SrcOpIdx2);
}

#define CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, LMUL)               \
  case Primate::PseudoV##OLDOP##_##TYPE##_##LMUL##_COMMUTABLE:                   \
    Opc = Primate::PseudoV##NEWOP##_##TYPE##_##LMUL##_COMMUTABLE;                \
    break;

#define CASE_VFMA_CHANGE_OPCODE_LMULS(OLDOP, NEWOP, TYPE)                      \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF8)                      \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF4)                      \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF2)                      \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M1)                       \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M2)                       \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M4)                       \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M8)

#define CASE_VFMA_CHANGE_OPCODE_SPLATS(OLDOP, NEWOP)                           \
  CASE_VFMA_CHANGE_OPCODE_LMULS(OLDOP, NEWOP, VF16)                            \
  CASE_VFMA_CHANGE_OPCODE_LMULS(OLDOP, NEWOP, VF32)                            \
  CASE_VFMA_CHANGE_OPCODE_LMULS(OLDOP, NEWOP, VF64)

MachineInstr *PrimateInstrInfo::commuteInstructionImpl(MachineInstr &MI,
                                                     bool NewMI,
                                                     unsigned OpIdx1,
                                                     unsigned OpIdx2) const {
  auto cloneIfNew = [NewMI](MachineInstr &MI) -> MachineInstr & {
    if (NewMI)
      return *MI.getParent()->getParent()->CloneMachineInstr(&MI);
    return MI;
  };

  switch (MI.getOpcode()) {
  case CASE_VFMA_SPLATS(FMACC):
  case CASE_VFMA_SPLATS(FMADD):
  case CASE_VFMA_SPLATS(FMSAC):
  case CASE_VFMA_SPLATS(FMSUB):
  case CASE_VFMA_SPLATS(FNMACC):
  case CASE_VFMA_SPLATS(FNMADD):
  case CASE_VFMA_SPLATS(FNMSAC):
  case CASE_VFMA_SPLATS(FNMSUB):
  case CASE_VFMA_OPCODE_LMULS(FMACC, VV):
  case CASE_VFMA_OPCODE_LMULS(FMSAC, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMACC, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMSAC, VV):
  case CASE_VFMA_OPCODE_LMULS(MADD, VX):
  case CASE_VFMA_OPCODE_LMULS(NMSUB, VX):
  case CASE_VFMA_OPCODE_LMULS(MACC, VX):
  case CASE_VFMA_OPCODE_LMULS(NMSAC, VX):
  case CASE_VFMA_OPCODE_LMULS(MACC, VV):
  case CASE_VFMA_OPCODE_LMULS(NMSAC, VV): {
    // It only make sense to toggle these between clobbering the
    // addend/subtrahend/minuend one of the multiplicands.
    assert((OpIdx1 == 1 || OpIdx2 == 1) && "Unexpected opcode index");
    assert((OpIdx1 == 3 || OpIdx2 == 3) && "Unexpected opcode index");
    unsigned Opc;
    switch (MI.getOpcode()) {
      default:
        llvm_unreachable("Unexpected opcode");
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMACC, FMADD)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMADD, FMACC)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMSAC, FMSUB)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMSUB, FMSAC)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMACC, FNMADD)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMADD, FNMACC)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMSAC, FNMSUB)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMSUB, FNMSAC)
      CASE_VFMA_CHANGE_OPCODE_LMULS(FMACC, FMADD, VV)
      CASE_VFMA_CHANGE_OPCODE_LMULS(FMSAC, FMSUB, VV)
      CASE_VFMA_CHANGE_OPCODE_LMULS(FNMACC, FNMADD, VV)
      CASE_VFMA_CHANGE_OPCODE_LMULS(FNMSAC, FNMSUB, VV)
      CASE_VFMA_CHANGE_OPCODE_LMULS(MACC, MADD, VX)
      CASE_VFMA_CHANGE_OPCODE_LMULS(MADD, MACC, VX)
      CASE_VFMA_CHANGE_OPCODE_LMULS(NMSAC, NMSUB, VX)
      CASE_VFMA_CHANGE_OPCODE_LMULS(NMSUB, NMSAC, VX)
      CASE_VFMA_CHANGE_OPCODE_LMULS(MACC, MADD, VV)
      CASE_VFMA_CHANGE_OPCODE_LMULS(NMSAC, NMSUB, VV)
    }

    auto &WorkingMI = cloneIfNew(MI);
    WorkingMI.setDesc(get(Opc));
    return TargetInstrInfo::commuteInstructionImpl(WorkingMI, /*NewMI=*/false,
                                                   OpIdx1, OpIdx2);
  }
  case CASE_VFMA_OPCODE_LMULS(FMADD, VV):
  case CASE_VFMA_OPCODE_LMULS(FMSUB, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMADD, VV):
  case CASE_VFMA_OPCODE_LMULS(FNMSUB, VV):
  case CASE_VFMA_OPCODE_LMULS(MADD, VV):
  case CASE_VFMA_OPCODE_LMULS(NMSUB, VV): {
    assert((OpIdx1 == 1 || OpIdx2 == 1) && "Unexpected opcode index");
    // If one of the operands, is the addend we need to change opcode.
    // Otherwise we're just swapping 2 of the multiplicands.
    if (OpIdx1 == 3 || OpIdx2 == 3) {
      unsigned Opc;
      switch (MI.getOpcode()) {
        default:
          llvm_unreachable("Unexpected opcode");
        CASE_VFMA_CHANGE_OPCODE_LMULS(FMADD, FMACC, VV)
        CASE_VFMA_CHANGE_OPCODE_LMULS(FMSUB, FMSAC, VV)
        CASE_VFMA_CHANGE_OPCODE_LMULS(FNMADD, FNMACC, VV)
        CASE_VFMA_CHANGE_OPCODE_LMULS(FNMSUB, FNMSAC, VV)
        CASE_VFMA_CHANGE_OPCODE_LMULS(MADD, MACC, VV)
        CASE_VFMA_CHANGE_OPCODE_LMULS(NMSUB, NMSAC, VV)
      }

      auto &WorkingMI = cloneIfNew(MI);
      WorkingMI.setDesc(get(Opc));
      return TargetInstrInfo::commuteInstructionImpl(WorkingMI, /*NewMI=*/false,
                                                     OpIdx1, OpIdx2);
    }
    // Let the default code handle it.
    break;
  }
  }

  return TargetInstrInfo::commuteInstructionImpl(MI, NewMI, OpIdx1, OpIdx2);
}

#undef CASE_VFMA_CHANGE_OPCODE_SPLATS
#undef CASE_VFMA_CHANGE_OPCODE_LMULS
#undef CASE_VFMA_CHANGE_OPCODE_COMMON
#undef CASE_VFMA_SPLATS
#undef CASE_VFMA_OPCODE_LMULS
#undef CASE_VFMA_OPCODE_COMMON

// clang-format off
#define CASE_WIDEOP_OPCODE_COMMON(OP, LMUL)                                    \
  Primate::PseudoV##OP##_##LMUL##_TIED

#define CASE_WIDEOP_OPCODE_LMULS(OP)                                           \
  CASE_WIDEOP_OPCODE_COMMON(OP, MF8):                                          \
  case CASE_WIDEOP_OPCODE_COMMON(OP, MF4):                                     \
  case CASE_WIDEOP_OPCODE_COMMON(OP, MF2):                                     \
  case CASE_WIDEOP_OPCODE_COMMON(OP, M1):                                      \
  case CASE_WIDEOP_OPCODE_COMMON(OP, M2):                                      \
  case CASE_WIDEOP_OPCODE_COMMON(OP, M4)
// clang-format on

#define CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, LMUL)                             \
  case Primate::PseudoV##OP##_##LMUL##_TIED:                                     \
    NewOpc = Primate::PseudoV##OP##_##LMUL;                                      \
    break;

#define CASE_WIDEOP_CHANGE_OPCODE_LMULS(OP)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF8)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF4)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF2)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, M1)                                     \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, M2)                                     \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, M4)

MachineInstr *PrimateInstrInfo::convertToThreeAddress(
    MachineFunction::iterator &MBB, MachineInstr &MI, LiveVariables *LV) const {
  switch (MI.getOpcode()) {
  default:
    break;
  case CASE_WIDEOP_OPCODE_LMULS(FWADD_WV):
  case CASE_WIDEOP_OPCODE_LMULS(FWSUB_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WADD_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WADDU_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WSUB_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WSUBU_WV): {
    // clang-format off
    unsigned NewOpc;
    switch (MI.getOpcode()) {
    default:
      llvm_unreachable("Unexpected opcode");
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(FWADD_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(FWSUB_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WADD_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WADDU_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WSUB_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WSUBU_WV)
    }
    //clang-format on

    MachineInstrBuilder MIB = BuildMI(*MBB, MI, MI.getDebugLoc(), get(NewOpc))
                                  .add(MI.getOperand(0))
                                  .add(MI.getOperand(1))
                                  .add(MI.getOperand(2))
                                  .add(MI.getOperand(3))
                                  .add(MI.getOperand(4));
    MIB.copyImplicitOps(MI);

    if (LV) {
      unsigned NumOps = MI.getNumOperands();
      for (unsigned I = 1; I < NumOps; ++I) {
        MachineOperand &Op = MI.getOperand(I);
        if (Op.isReg() && Op.isKill())
          LV->replaceKillInstruction(Op.getReg(), MI, *MIB);
      }
    }

    return MIB;
  }
  }

  return nullptr;
}

#undef CASE_WIDEOP_CHANGE_OPCODE_LMULS
#undef CASE_WIDEOP_CHANGE_OPCODE_COMMON
#undef CASE_WIDEOP_OPCODE_LMULS
#undef CASE_WIDEOP_OPCODE_COMMON

Register PrimateInstrInfo::getVLENFactoredAmount(MachineFunction &MF,
                                               MachineBasicBlock &MBB,
                                               MachineBasicBlock::iterator II,
                                               const DebugLoc &DL,
                                               int64_t Amount,
                                               MachineInstr::MIFlag Flag) const {
  assert(Amount > 0 && "There is no need to get VLEN scaled value.");
  assert(Amount % 8 == 0 &&
         "Reserve the stack by the multiple of one vector size.");

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const PrimateInstrInfo *TII = MF.getSubtarget<PrimateSubtarget>().getInstrInfo();
  int64_t NumOfVReg = Amount / 8;

  Register VL = MRI.createVirtualRegister(&Primate::GPRRegClass);
  BuildMI(MBB, II, DL, TII->get(Primate::PseudoReadVLENB), VL)
    .setMIFlag(Flag);
  assert(isInt<32>(NumOfVReg) &&
         "Expect the number of vector registers within 32-bits.");
  if (isPowerOf2_32(NumOfVReg)) {
    uint32_t ShiftAmount = Log2_32(NumOfVReg);
    if (ShiftAmount == 0)
      return VL;
    BuildMI(MBB, II, DL, TII->get(Primate::SLLI), VL)
        .addReg(VL, RegState::Kill)
        .addImm(ShiftAmount)
        .setMIFlag(Flag);
  } else if (isPowerOf2_32(NumOfVReg - 1)) {
    Register ScaledRegister = MRI.createVirtualRegister(&Primate::GPRRegClass);
    uint32_t ShiftAmount = Log2_32(NumOfVReg - 1);
    BuildMI(MBB, II, DL, TII->get(Primate::SLLI), ScaledRegister)
        .addReg(VL)
        .addImm(ShiftAmount)
        .setMIFlag(Flag);
    BuildMI(MBB, II, DL, TII->get(Primate::ADD), VL)
        .addReg(ScaledRegister, RegState::Kill)
        .addReg(VL, RegState::Kill)
        .setMIFlag(Flag);
  } else if (isPowerOf2_32(NumOfVReg + 1)) {
    Register ScaledRegister = MRI.createVirtualRegister(&Primate::GPRRegClass);
    uint32_t ShiftAmount = Log2_32(NumOfVReg + 1);
    BuildMI(MBB, II, DL, TII->get(Primate::SLLI), ScaledRegister)
        .addReg(VL)
        .addImm(ShiftAmount)
        .setMIFlag(Flag);
    BuildMI(MBB, II, DL, TII->get(Primate::SUB), VL)
        .addReg(ScaledRegister, RegState::Kill)
        .addReg(VL, RegState::Kill)
        .setMIFlag(Flag);
  } else {
    Register N = MRI.createVirtualRegister(&Primate::GPRRegClass);
    if (!isInt<12>(NumOfVReg))
      movImm(MBB, II, DL, N, NumOfVReg);
    else {
      BuildMI(MBB, II, DL, TII->get(Primate::ADDI), N)
          .addReg(Primate::X0)
          .addImm(NumOfVReg)
          .setMIFlag(Flag);
    }
    if (!MF.getSubtarget<PrimateSubtarget>().hasStdExtM())
      MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
          MF.getFunction(),
          "M-extension must be enabled to calculate the vscaled size/offset."});
    BuildMI(MBB, II, DL, TII->get(Primate::MUL), VL)
        .addReg(VL, RegState::Kill)
        .addReg(N, RegState::Kill)
        .setMIFlag(Flag);
  }

  return VL;
}

static bool isPRVWholeLoadStore(unsigned Opcode) {
  switch (Opcode) {
  default:
    return false;
  case Primate::VS1R_V:
  case Primate::VS2R_V:
  case Primate::VS4R_V:
  case Primate::VS8R_V:
  case Primate::VL1RE8_V:
  case Primate::VL2RE8_V:
  case Primate::VL4RE8_V:
  case Primate::VL8RE8_V:
  case Primate::VL1RE16_V:
  case Primate::VL2RE16_V:
  case Primate::VL4RE16_V:
  case Primate::VL8RE16_V:
  case Primate::VL1RE32_V:
  case Primate::VL2RE32_V:
  case Primate::VL4RE32_V:
  case Primate::VL8RE32_V:
  case Primate::VL1RE64_V:
  case Primate::VL2RE64_V:
  case Primate::VL4RE64_V:
  case Primate::VL8RE64_V:
    return true;
  }
}

bool PrimateInstrInfo::isPRVSpill(const MachineInstr &MI, bool CheckFIs) const {
  // PRV lacks any support for immediate addressing for stack addresses, so be
  // conservative.
  unsigned Opcode = MI.getOpcode();
  if (!PrimateVPseudosTable::getPseudoInfo(Opcode) &&
      !isPRVWholeLoadStore(Opcode) && !isPRVSpillForZvlsseg(Opcode))
    return false;
  return !CheckFIs || any_of(MI.operands(), [](const MachineOperand &MO) {
    return MO.isFI();
  });
}

Optional<std::pair<unsigned, unsigned>>
PrimateInstrInfo::isPRVSpillForZvlsseg(unsigned Opcode) const {
  switch (Opcode) {
  default:
    return None;
  case Primate::PseudoVSPILL2_M1:
  case Primate::PseudoVRELOAD2_M1:
    return std::make_pair(2u, 1u);
  case Primate::PseudoVSPILL2_M2:
  case Primate::PseudoVRELOAD2_M2:
    return std::make_pair(2u, 2u);
  case Primate::PseudoVSPILL2_M4:
  case Primate::PseudoVRELOAD2_M4:
    return std::make_pair(2u, 4u);
  case Primate::PseudoVSPILL3_M1:
  case Primate::PseudoVRELOAD3_M1:
    return std::make_pair(3u, 1u);
  case Primate::PseudoVSPILL3_M2:
  case Primate::PseudoVRELOAD3_M2:
    return std::make_pair(3u, 2u);
  case Primate::PseudoVSPILL4_M1:
  case Primate::PseudoVRELOAD4_M1:
    return std::make_pair(4u, 1u);
  case Primate::PseudoVSPILL4_M2:
  case Primate::PseudoVRELOAD4_M2:
    return std::make_pair(4u, 2u);
  case Primate::PseudoVSPILL5_M1:
  case Primate::PseudoVRELOAD5_M1:
    return std::make_pair(5u, 1u);
  case Primate::PseudoVSPILL6_M1:
  case Primate::PseudoVRELOAD6_M1:
    return std::make_pair(6u, 1u);
  case Primate::PseudoVSPILL7_M1:
  case Primate::PseudoVRELOAD7_M1:
    return std::make_pair(7u, 1u);
  case Primate::PseudoVSPILL8_M1:
  case Primate::PseudoVRELOAD8_M1:
    return std::make_pair(8u, 1u);
  }
}
