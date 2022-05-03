//===-- PrimateRegisterInfo.cpp - Primate Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Primate implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "PrimateRegisterInfo.h"
#include "Primate.h"
#include "PrimateMachineFunctionInfo.h"
#include "PrimateSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "PrimateGenRegisterInfo.inc"

using namespace llvm;

static_assert(Primate::X1 == Primate::X0 + 1, "Register list not consecutive");
static_assert(Primate::X31 == Primate::X0 + 31, "Register list not consecutive");
static_assert(Primate::F1_H == Primate::F0_H + 1, "Register list not consecutive");
static_assert(Primate::F31_H == Primate::F0_H + 31,
              "Register list not consecutive");
static_assert(Primate::F1_F == Primate::F0_F + 1, "Register list not consecutive");
static_assert(Primate::F31_F == Primate::F0_F + 31,
              "Register list not consecutive");
static_assert(Primate::F1_D == Primate::F0_D + 1, "Register list not consecutive");
static_assert(Primate::F31_D == Primate::F0_D + 31,
              "Register list not consecutive");
static_assert(Primate::V1 == Primate::V0 + 1, "Register list not consecutive");
static_assert(Primate::V31 == Primate::V0 + 31, "Register list not consecutive");

PrimateRegisterInfo::PrimateRegisterInfo(unsigned HwMode)
    : PrimateGenRegisterInfo(Primate::X1, /*DwarfFlavour*/0, /*EHFlavor*/0,
                           /*PC*/0, HwMode) {}

const MCPhysReg *
PrimateRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  auto &Subtarget = MF->getSubtarget<PrimateSubtarget>();
  if (MF->getFunction().getCallingConv() == CallingConv::GHC)
    return CSR_NoRegs_SaveList;
  if (MF->getFunction().hasFnAttribute("interrupt")) {
    if (Subtarget.hasStdExtD())
      return CSR_XLEN_F64_Interrupt_SaveList;
    if (Subtarget.hasStdExtF())
      return CSR_XLEN_F32_Interrupt_SaveList;
    return CSR_Interrupt_SaveList;
  }

  switch (Subtarget.getTargetABI()) {
  default:
    llvm_unreachable("Unrecognized ABI");
  case PrimateABI::ABI_ILP32:
  case PrimateABI::ABI_LP64:
    return CSR_ILP32_LP64_SaveList;
  case PrimateABI::ABI_ILP32F:
  case PrimateABI::ABI_LP64F:
    return CSR_ILP32F_LP64F_SaveList;
  case PrimateABI::ABI_ILP32D:
  case PrimateABI::ABI_LP64D:
    return CSR_ILP32D_LP64D_SaveList;
  }
}

BitVector PrimateRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  const PrimateFrameLowering *TFI = getFrameLowering(MF);
  BitVector Reserved(getNumRegs());

  // Mark any registers requested to be reserved as such
  for (size_t Reg = 0; Reg < getNumRegs(); Reg++) {
    if (MF.getSubtarget<PrimateSubtarget>().isRegisterReservedByUser(Reg))
      markSuperRegs(Reserved, Reg);
  }

  // Use markSuperRegs to ensure any register aliases are also reserved
  markSuperRegs(Reserved, Primate::X0); // zero
  markSuperRegs(Reserved, Primate::X2); // sp
  markSuperRegs(Reserved, Primate::X3); // gp
  markSuperRegs(Reserved, Primate::X4); // tp
  if (TFI->hasFP(MF))
    markSuperRegs(Reserved, Primate::X8); // fp
  // Reserve the base register if we need to realign the stack and allocate
  // variable-sized objects at runtime.
  if (TFI->hasBP(MF))
    markSuperRegs(Reserved, PrimateABI::getBPReg()); // bp

  // V registers for code generation. We handle them manually.
  markSuperRegs(Reserved, Primate::VL);
  markSuperRegs(Reserved, Primate::VTYPE);
  markSuperRegs(Reserved, Primate::VXSAT);
  markSuperRegs(Reserved, Primate::VXRM);

  // Floating point environment registers.
  markSuperRegs(Reserved, Primate::FRM);
  markSuperRegs(Reserved, Primate::FFLAGS);
  markSuperRegs(Reserved, Primate::FCSR);

  assert(checkAllSuperRegsMarked(Reserved));
  return Reserved;
}

bool PrimateRegisterInfo::isAsmClobberable(const MachineFunction &MF,
                                         MCRegister PhysReg) const {
  return !MF.getSubtarget<PrimateSubtarget>().isRegisterReservedByUser(PhysReg);
}

bool PrimateRegisterInfo::isConstantPhysReg(MCRegister PhysReg) const {
  return PhysReg == Primate::X0;
}

const uint32_t *PrimateRegisterInfo::getNoPreservedMask() const {
  return CSR_NoRegs_RegMask;
}

// Frame indexes representing locations of CSRs which are given a fixed location
// by save/restore libcalls.
static const std::map<unsigned, int> FixedCSRFIMap = {
  {/*ra*/  Primate::X1,   -1},
  {/*s0*/  Primate::X8,   -2},
  {/*s1*/  Primate::X9,   -3},
  {/*s2*/  Primate::X18,  -4},
  {/*s3*/  Primate::X19,  -5},
  {/*s4*/  Primate::X20,  -6},
  {/*s5*/  Primate::X21,  -7},
  {/*s6*/  Primate::X22,  -8},
  {/*s7*/  Primate::X23,  -9},
  {/*s8*/  Primate::X24,  -10},
  {/*s9*/  Primate::X25,  -11},
  {/*s10*/ Primate::X26,  -12},
  {/*s11*/ Primate::X27,  -13}
};

bool PrimateRegisterInfo::hasReservedSpillSlot(const MachineFunction &MF,
                                             Register Reg,
                                             int &FrameIdx) const {
  const auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();
  if (!PRFI->useSaveRestoreLibCalls(MF))
    return false;

  auto FII = FixedCSRFIMap.find(Reg);
  if (FII == FixedCSRFIMap.end())
    return false;

  FrameIdx = FII->second;
  return true;
}

void PrimateRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected non-zero SPAdj value");

  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const PrimateInstrInfo *TII = MF.getSubtarget<PrimateSubtarget>().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register FrameReg;
  StackOffset Offset =
      getFrameLowering(MF)->getFrameIndexReference(MF, FrameIndex, FrameReg);
  bool IsPRVSpill = TII->isPRVSpill(MI, /*CheckFIs*/ false);
  if (!IsPRVSpill)
    Offset += StackOffset::getFixed(MI.getOperand(FIOperandNum + 1).getImm());

  if (!isInt<32>(Offset.getFixed())) {
    report_fatal_error(
        "Frame offsets outside of the signed 32-bit range not supported");
  }

  MachineBasicBlock &MBB = *MI.getParent();
  bool FrameRegIsKill = false;

  // If required, pre-compute the scalable factor amount which will be used in
  // later offset computation. Since this sequence requires up to two scratch
  // registers -- after which one is made free -- this grants us better
  // scavenging of scratch registers as only up to two are live at one time,
  // rather than three.
  Register ScalableFactorRegister;
  unsigned ScalableAdjOpc = Primate::ADD;
  if (Offset.getScalable()) {
    int64_t ScalableValue = Offset.getScalable();
    if (ScalableValue < 0) {
      ScalableValue = -ScalableValue;
      ScalableAdjOpc = Primate::SUB;
    }
    // 1. Get vlenb && multiply vlen with the number of vector registers.
    ScalableFactorRegister =
        TII->getVLENFactoredAmount(MF, MBB, II, DL, ScalableValue);
  }

  if (!isInt<12>(Offset.getFixed())) {
    // The offset won't fit in an immediate, so use a scratch register instead
    // Modify Offset and FrameReg appropriately
    Register ScratchReg = MRI.createVirtualRegister(&Primate::GPRRegClass);
    TII->movImm(MBB, II, DL, ScratchReg, Offset.getFixed());
    if (MI.getOpcode() == Primate::ADDI && !Offset.getScalable()) {
      BuildMI(MBB, II, DL, TII->get(Primate::ADD), MI.getOperand(0).getReg())
        .addReg(FrameReg)
        .addReg(ScratchReg, RegState::Kill);
      MI.eraseFromParent();
      return;
    }
    BuildMI(MBB, II, DL, TII->get(Primate::ADD), ScratchReg)
        .addReg(FrameReg)
        .addReg(ScratchReg, RegState::Kill);
    Offset = StackOffset::get(0, Offset.getScalable());
    FrameReg = ScratchReg;
    FrameRegIsKill = true;
  }

  if (!Offset.getScalable()) {
    // Offset = (fixed offset, 0)
    MI.getOperand(FIOperandNum)
        .ChangeToRegister(FrameReg, false, false, FrameRegIsKill);
    if (!IsPRVSpill)
      MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset.getFixed());
    else {
      if (Offset.getFixed()) {
        Register ScratchReg = MRI.createVirtualRegister(&Primate::GPRRegClass);
        BuildMI(MBB, II, DL, TII->get(Primate::ADDI), ScratchReg)
          .addReg(FrameReg, getKillRegState(FrameRegIsKill))
          .addImm(Offset.getFixed());
        MI.getOperand(FIOperandNum)
          .ChangeToRegister(ScratchReg, false, false, true);
      }
    }
  } else {
    // Offset = (fixed offset, scalable offset)
    // Step 1, the scalable offset, has already been computed.
    assert(ScalableFactorRegister &&
           "Expected pre-computation of scalable factor in earlier step");

    // 2. Calculate address: FrameReg + result of multiply
    if (MI.getOpcode() == Primate::ADDI && !Offset.getFixed()) {
      BuildMI(MBB, II, DL, TII->get(ScalableAdjOpc), MI.getOperand(0).getReg())
          .addReg(FrameReg, getKillRegState(FrameRegIsKill))
          .addReg(ScalableFactorRegister, RegState::Kill);
      MI.eraseFromParent();
      return;
    }
    Register VL = MRI.createVirtualRegister(&Primate::GPRRegClass);
    BuildMI(MBB, II, DL, TII->get(ScalableAdjOpc), VL)
        .addReg(FrameReg, getKillRegState(FrameRegIsKill))
        .addReg(ScalableFactorRegister, RegState::Kill);

    if (IsPRVSpill && Offset.getFixed()) {
      // Scalable load/store has no immediate argument. We need to add the
      // fixed part into the load/store base address.
      BuildMI(MBB, II, DL, TII->get(Primate::ADDI), VL)
          .addReg(VL)
          .addImm(Offset.getFixed());
    }

    // 3. Replace address register with calculated address register
    MI.getOperand(FIOperandNum).ChangeToRegister(VL, false, false, true);
    if (!IsPRVSpill)
      MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset.getFixed());
  }

  auto ZvlssegInfo = TII->isPRVSpillForZvlsseg(MI.getOpcode());
  if (ZvlssegInfo) {
    Register VL = MRI.createVirtualRegister(&Primate::GPRRegClass);
    BuildMI(MBB, II, DL, TII->get(Primate::PseudoReadVLENB), VL);
    uint32_t ShiftAmount = Log2_32(ZvlssegInfo->second);
    if (ShiftAmount != 0)
      BuildMI(MBB, II, DL, TII->get(Primate::SLLI), VL)
          .addReg(VL)
          .addImm(ShiftAmount);
    // The last argument of pseudo spilling opcode for zvlsseg is the length of
    // one element of zvlsseg types. For example, for vint32m2x2_t, it will be
    // the length of vint32m2_t.
    MI.getOperand(FIOperandNum + 1).ChangeToRegister(VL, /*isDef=*/false);
  }
}

Register PrimateRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = getFrameLowering(MF);
  return TFI->hasFP(MF) ? Primate::X8 : Primate::X2;
}

const uint32_t *
PrimateRegisterInfo::getCallPreservedMask(const MachineFunction & MF,
                                        CallingConv::ID CC) const {
  auto &Subtarget = MF.getSubtarget<PrimateSubtarget>();

  if (CC == CallingConv::GHC)
    return CSR_NoRegs_RegMask;
  switch (Subtarget.getTargetABI()) {
  default:
    llvm_unreachable("Unrecognized ABI");
  case PrimateABI::ABI_ILP32:
  case PrimateABI::ABI_LP64:
    return CSR_ILP32_LP64_RegMask;
  case PrimateABI::ABI_ILP32F:
  case PrimateABI::ABI_LP64F:
    return CSR_ILP32F_LP64F_RegMask;
  case PrimateABI::ABI_ILP32D:
  case PrimateABI::ABI_LP64D:
    return CSR_ILP32D_LP64D_RegMask;
  }
}

const TargetRegisterClass *
PrimateRegisterInfo::getLargestLegalSuperClass(const TargetRegisterClass *RC,
                                             const MachineFunction &) const {
  if (RC == &Primate::VMV0RegClass)
    return &Primate::VRRegClass;
  return RC;
}
