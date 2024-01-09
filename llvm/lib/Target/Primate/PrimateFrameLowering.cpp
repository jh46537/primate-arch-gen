//===-- PrimateFrameLowering.cpp - Primate Frame Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Primate implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "PrimateFrameLowering.h"
#include "PrimateMachineFunctionInfo.h"
#include "PrimateSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/MC/MCDwarf.h"

using namespace llvm;

// For now we use x18, a.k.a s2, as pointer to shadow call stack.
// User should explicitly set -ffixed-x18 and not use x18 in their asm.
static void emitSCSPrologue(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            const DebugLoc &DL) {
  if (!MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack))
    return;
  return;

  const auto &STI = MF.getSubtarget<PrimateSubtarget>();
  Register RAReg = STI.getRegisterInfo()->getRARegister();

  // Do not save RA to the SCS if it's not saved to the regular stack,
  // i.e. RA is not at risk of being overwritten.
  std::vector<CalleeSavedInfo> &CSI = MF.getFrameInfo().getCalleeSavedInfo();
  if (std::none_of(CSI.begin(), CSI.end(),
                   [&](CalleeSavedInfo &CSR) { return CSR.getReg() == RAReg; }))
    return;

  Register SCSPReg = PrimateABI::getSCSPReg();

  auto &Ctx = MF.getFunction().getContext();
  if (!STI.isRegisterReservedByUser(SCSPReg)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "x18 not reserved by user for Shadow Call Stack."});
    return;
  }

  const auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();
  if (PRFI->useSaveRestoreLibCalls(MF)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Shadow Call Stack cannot be combined with Save/Restore LibCalls."});
    return;
  }

  const PrimateInstrInfo *TII = STI.getInstrInfo();
  bool IsPR64 = STI.hasFeature(Primate::Feature64Bit);
  int64_t SlotSize = STI.getXLen() / 8;
  // Store return address to shadow call stack
  // s[w|d]  ra, 0(s2)
  // addi    s2, s2, [4|8]
  BuildMI(MBB, MI, DL, TII->get(IsPR64 ? Primate::SD : Primate::SW))
      .addReg(RAReg)
      .addReg(SCSPReg)
      .addImm(0)
      .setMIFlag(MachineInstr::FrameSetup);
  BuildMI(MBB, MI, DL, TII->get(Primate::ADDI))
      .addReg(SCSPReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(SlotSize)
      .setMIFlag(MachineInstr::FrameSetup);
}

static void emitSCSEpilogue(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            const DebugLoc &DL) {
  if (!MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack))
    return;
  return;

  const auto &STI = MF.getSubtarget<PrimateSubtarget>();
  Register RAReg = STI.getRegisterInfo()->getRARegister();

  // See emitSCSPrologue() above.
  std::vector<CalleeSavedInfo> &CSI = MF.getFrameInfo().getCalleeSavedInfo();
  if (std::none_of(CSI.begin(), CSI.end(),
                   [&](CalleeSavedInfo &CSR) { return CSR.getReg() == RAReg; }))
    return;

  Register SCSPReg = PrimateABI::getSCSPReg();

  auto &Ctx = MF.getFunction().getContext();
  if (!STI.isRegisterReservedByUser(SCSPReg)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "x18 not reserved by user for Shadow Call Stack."});
    return;
  }

  const auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();
  if (PRFI->useSaveRestoreLibCalls(MF)) {
    Ctx.diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Shadow Call Stack cannot be combined with Save/Restore LibCalls."});
    return;
  }

  const PrimateInstrInfo *TII = STI.getInstrInfo();
  bool IsPR64 = STI.hasFeature(Primate::Feature64Bit);
  int64_t SlotSize = STI.getXLen() / 8;
  // Load return address from shadow call stack
  // l[w|d]  ra, -[4|8](s2)
  // addi    s2, s2, -[4|8]
  BuildMI(MBB, MI, DL, TII->get(IsPR64 ? Primate::LD : Primate::LW))
      .addReg(RAReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameDestroy);
  BuildMI(MBB, MI, DL, TII->get(Primate::ADDI))
      .addReg(SCSPReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameDestroy);
}

// Get the ID of the libcall used for spilling and restoring callee saved
// registers. The ID is representative of the number of registers saved or
// restored by the libcall, except it is zero-indexed - ID 0 corresponds to a
// single register.
static int getLibCallID(const MachineFunction &MF,
                        const std::vector<CalleeSavedInfo> &CSI) {
  const auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();

  if (CSI.empty() || !PRFI->useSaveRestoreLibCalls(MF))
    return -1;

  Register MaxReg = Primate::NoRegister;
  for (auto &CS : CSI)
    // PrimateRegisterInfo::hasReservedSpillSlot assigns negative frame indexes to
    // registers which can be saved by libcall.
    if (CS.getFrameIdx() < 0)
      MaxReg = std::max(MaxReg.id(), CS.getReg().id());

  if (MaxReg == Primate::NoRegister)
    return -1;

  switch (MaxReg) {
  default:
    llvm_unreachable("Something has gone wrong!");
  case /*s11*/ Primate::X27: return 12;
  case /*s10*/ Primate::X26: return 11;
  case /*s9*/  Primate::X25: return 10;
  case /*s8*/  Primate::X24: return 9;
  case /*s7*/  Primate::X23: return 8;
  case /*s6*/  Primate::X22: return 7;
  case /*s5*/  Primate::X21: return 6;
  case /*s4*/  Primate::X20: return 5;
  case /*s3*/  Primate::X19: return 4;
  case /*s2*/  Primate::X18: return 3;
  case /*s1*/  Primate::X9:  return 2;
  case /*s0*/  Primate::X8:  return 1;
  case /*ra*/  Primate::X1:  return 0;
  }
}

// Get the name of the libcall used for spilling callee saved registers.
// If this function will not use save/restore libcalls, then return a nullptr.
static const char *
getSpillLibCallName(const MachineFunction &MF,
                    const std::vector<CalleeSavedInfo> &CSI) {
  static const char *const SpillLibCalls[] = {
    "__primate_save_0",
    "__primate_save_1",
    "__primate_save_2",
    "__primate_save_3",
    "__primate_save_4",
    "__primate_save_5",
    "__primate_save_6",
    "__primate_save_7",
    "__primate_save_8",
    "__primate_save_9",
    "__primate_save_10",
    "__primate_save_11",
    "__primate_save_12"
  };

  int LibCallID = getLibCallID(MF, CSI);
  if (LibCallID == -1)
    return nullptr;
  return SpillLibCalls[LibCallID];
}

// Get the name of the libcall used for restoring callee saved registers.
// If this function will not use save/restore libcalls, then return a nullptr.
static const char *
getRestoreLibCallName(const MachineFunction &MF,
                      const std::vector<CalleeSavedInfo> &CSI) {
  static const char *const RestoreLibCalls[] = {
    "__primate_restore_0",
    "__primate_restore_1",
    "__primate_restore_2",
    "__primate_restore_3",
    "__primate_restore_4",
    "__primate_restore_5",
    "__primate_restore_6",
    "__primate_restore_7",
    "__primate_restore_8",
    "__primate_restore_9",
    "__primate_restore_10",
    "__primate_restore_11",
    "__primate_restore_12"
  };

  int LibCallID = getLibCallID(MF, CSI);
  if (LibCallID == -1)
    return nullptr;
  return RestoreLibCalls[LibCallID];
}

bool PrimateFrameLowering::hasFP(const MachineFunction &MF) const {
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         RegInfo->hasStackRealignment(MF) || MFI.hasVarSizedObjects() ||
         MFI.isFrameAddressTaken();
}

bool PrimateFrameLowering::hasBP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();

  return MFI.hasVarSizedObjects() && TRI->hasStackRealignment(MF);
}

// Determines the size of the frame and maximum call frame size.
void PrimateFrameLowering::determineFrameLayout(MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // Get the number of bytes to allocate from the FrameInfo.
  uint64_t FrameSize = MFI.getStackSize();

  // Get the alignment.
  Align StackAlign = getStackAlign();

  // Make sure the frame is aligned.
  FrameSize = alignTo(FrameSize, StackAlign);

  // Update frame info.
  MFI.setStackSize(FrameSize);
}

void PrimateFrameLowering::adjustReg(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   const DebugLoc &DL, Register DestReg,
                                   Register SrcReg, int64_t Val,
                                   MachineInstr::MIFlag Flag) const {
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  const PrimateInstrInfo *TII = STI.getInstrInfo();

  if (DestReg == SrcReg && Val == 0)
    return;

  if (isInt<12>(Val)) {
    BuildMI(MBB, MBBI, DL, TII->get(Primate::ADDI), DestReg)
        .addReg(SrcReg)
        .addImm(Val)
        .setMIFlag(Flag);
  } else {
    unsigned Opc = Primate::ADD;
    bool isSub = Val < 0;
    if (isSub) {
      Val = -Val;
      Opc = Primate::SUB;
    }

    Register ScratchReg = MRI.createVirtualRegister(&Primate::GPRRegClass);
    TII->movImm(MBB, MBBI, DL, ScratchReg, Val, Flag);
    BuildMI(MBB, MBBI, DL, TII->get(Opc), DestReg)
        .addReg(SrcReg)
        .addReg(ScratchReg, RegState::Kill)
        .setMIFlag(Flag);
  }
}

// Returns the register used to hold the frame pointer.
static Register getFPReg(const PrimateSubtarget &STI) { return Primate::X8; }

// Returns the register used to hold the stack pointer.
static Register getSPReg(const PrimateSubtarget &STI) { return Primate::X2; }

static SmallVector<CalleeSavedInfo, 8>
getNonLibcallCSI(const MachineFunction &MF,
                 const std::vector<CalleeSavedInfo> &CSI) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  SmallVector<CalleeSavedInfo, 8> NonLibcallCSI;

  for (auto &CS : CSI) {
    int FI = CS.getFrameIdx();
    if (FI >= 0 && MFI.getStackID(FI) == TargetStackID::Default)
      NonLibcallCSI.push_back(CS);
  }

  return NonLibcallCSI;
}

void PrimateFrameLowering::adjustStackForPRV(MachineFunction &MF,
                                           MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator MBBI,
                                           const DebugLoc &DL, int64_t Amount,
                                           MachineInstr::MIFlag Flag) const {
  assert(Amount != 0 && "Did not need to adjust stack pointer for PRV.");

  const PrimateInstrInfo *TII = STI.getInstrInfo();
  Register SPReg = getSPReg(STI);
  unsigned Opc = Primate::ADD;
  if (Amount < 0) {
    Amount = -Amount;
    Opc = Primate::SUB;
  }
  // 1. Multiply the number of v-slots to the length of registers
  Register FactorRegister =
      TII->getVLENFactoredAmount(MF, MBB, MBBI, DL, Amount, Flag);
  // 2. SP = SP - PRV stack size
  BuildMI(MBB, MBBI, DL, TII->get(Opc), SPReg)
      .addReg(SPReg)
      .addReg(FactorRegister, RegState::Kill)
      .setMIFlag(Flag);
}

void PrimateFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  return;
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();
  const PrimateRegisterInfo *RI = STI.getRegisterInfo();
  const PrimateInstrInfo *TII = STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();

  Register FPReg = getFPReg(STI);
  Register SPReg = getSPReg(STI);
  Register BPReg = PrimateABI::getBPReg();

  // Debug location must be unknown since the first debug location is used
  // to determine the end of the prologue.
  DebugLoc DL;

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // Emit prologue for shadow call stack.
  emitSCSPrologue(MF, MBB, MBBI, DL);

  // Since spillCalleeSavedRegisters may have inserted a libcall, skip past
  // any instructions marked as FrameSetup
  while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup))
    ++MBBI;

  // Determine the correct frame layout
  determineFrameLayout(MF);

  // If libcalls are used to spill and restore callee-saved registers, the frame
  // has two sections; the opaque section managed by the libcalls, and the
  // section managed by MachineFrameInfo which can also hold callee saved
  // registers in fixed stack slots, both of which have negative frame indices.
  // This gets even more complicated when incoming arguments are passed via the
  // stack, as these too have negative frame indices. An example is detailed
  // below:
  //
  //  | incoming arg | <- FI[-3]
  //  | libcallspill |
  //  | calleespill  | <- FI[-2]
  //  | calleespill  | <- FI[-1]
  //  | this_frame   | <- FI[0]
  //
  // For negative frame indices, the offset from the frame pointer will differ
  // depending on which of these groups the frame index applies to.
  // The following calculates the correct offset knowing the number of callee
  // saved registers spilt by the two methods.
  if (int LibCallRegs = getLibCallID(MF, MFI.getCalleeSavedInfo()) + 1) {
    // Calculate the size of the frame managed by the libcall. The libcalls are
    // implemented such that the stack will always be 16 byte aligned.
    unsigned LibCallFrameSize = alignTo((STI.getXLen() / 8) * LibCallRegs, 16);
    PRFI->setLibCallStackSize(LibCallFrameSize);
  }

  // FIXME (note copied from Lanai): This appears to be overallocating.  Needs
  // investigation. Get the number of bytes to allocate from the FrameInfo.
  uint64_t StackSize = MFI.getStackSize() + PRFI->getPRVPadding();
  uint64_t RealStackSize = StackSize + PRFI->getLibCallStackSize();
  uint64_t PRVStackSize = PRFI->getPRVStackSize();

  // Early exit if there is no need to allocate on the stack
  if (RealStackSize == 0 && !MFI.adjustsStack() && PRVStackSize == 0)
    return;

  // If the stack pointer has been marked as reserved, then produce an error if
  // the frame requires stack allocation
  if (STI.isRegisterReservedByUser(SPReg))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "Stack pointer required, but has been reserved."});

  uint64_t FirstSPAdjustAmount = getFirstSPAdjustAmount(MF);
  // Split the SP adjustment to reduce the offsets of callee saved spill.
  if (FirstSPAdjustAmount) {
    StackSize = FirstSPAdjustAmount;
    RealStackSize = FirstSPAdjustAmount;
  }

  // Allocate space on the stack if necessary.
  adjustReg(MBB, MBBI, DL, SPReg, SPReg, -StackSize, MachineInstr::FrameSetup);

  // Emit ".cfi_def_cfa_offset RealStackSize"
  unsigned CFIIndex = MF.addFrameInst(
      MCCFIInstruction::cfiDefCfaOffset(nullptr, RealStackSize));
  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
      .addCFIIndex(CFIIndex)
      .setMIFlag(MachineInstr::FrameSetup);

  const auto &CSI = MFI.getCalleeSavedInfo();

  // The frame pointer is callee-saved, and code has been generated for us to
  // save it to the stack. We need to skip over the storing of callee-saved
  // registers as the frame pointer must be modified after it has been saved
  // to the stack, not before.
  // FIXME: assumes exactly one instruction is used to save each callee-saved
  // register.
  std::advance(MBBI, getNonLibcallCSI(MF, CSI).size());

  // Iterate over list of callee-saved registers and emit .cfi_offset
  // directives.
  for (const auto &Entry : CSI) {
    int FrameIdx = Entry.getFrameIdx();
    int64_t Offset;
    // Offsets for objects with fixed locations (IE: those saved by libcall) are
    // simply calculated from the frame index.
    if (FrameIdx < 0)
      Offset = FrameIdx * (int64_t) STI.getXLen() / 8;
    else
      Offset = MFI.getObjectOffset(Entry.getFrameIdx()) -
               PRFI->getLibCallStackSize();
    Register Reg = Entry.getReg();
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
        nullptr, RI->getDwarfRegNum(Reg, true), Offset));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex)
        .setMIFlag(MachineInstr::FrameSetup);
  }

  // Generate new FP.
  if (hasFP(MF)) {
    if (STI.isRegisterReservedByUser(FPReg))
      MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
          MF.getFunction(), "Frame pointer required, but has been reserved."});

    adjustReg(MBB, MBBI, DL, FPReg, SPReg,
              RealStackSize - PRFI->getVarArgsSaveSize(),
              MachineInstr::FrameSetup);

    // Emit ".cfi_def_cfa $fp, PRFI->getVarArgsSaveSize()"
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::cfiDefCfa(
        nullptr, RI->getDwarfRegNum(FPReg, true), PRFI->getVarArgsSaveSize()));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex)
        .setMIFlag(MachineInstr::FrameSetup);
  }

  // Emit the second SP adjustment after saving callee saved registers.
  if (FirstSPAdjustAmount) {
    uint64_t SecondSPAdjustAmount = MFI.getStackSize() - FirstSPAdjustAmount;
    assert(SecondSPAdjustAmount > 0 &&
           "SecondSPAdjustAmount should be greater than zero");
    adjustReg(MBB, MBBI, DL, SPReg, SPReg, -SecondSPAdjustAmount,
              MachineInstr::FrameSetup);

    // If we are using a frame-pointer, and thus emitted ".cfi_def_cfa fp, 0",
    // don't emit an sp-based .cfi_def_cfa_offset
    if (!hasFP(MF)) {
      // Emit ".cfi_def_cfa_offset StackSize"
      unsigned CFIIndex = MF.addFrameInst(
          MCCFIInstruction::cfiDefCfaOffset(nullptr, MFI.getStackSize()));
      BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex)
          .setMIFlag(MachineInstr::FrameSetup);
    }
  }

  if (PRVStackSize)
    adjustStackForPRV(MF, MBB, MBBI, DL, -PRVStackSize,
                      MachineInstr::FrameSetup);

  if (hasFP(MF)) {
    // Realign Stack
    const PrimateRegisterInfo *RI = STI.getRegisterInfo();
    if (RI->hasStackRealignment(MF)) {
      Align MaxAlignment = MFI.getMaxAlign();

      const PrimateInstrInfo *TII = STI.getInstrInfo();
      if (isInt<12>(-(int)MaxAlignment.value())) {
        BuildMI(MBB, MBBI, DL, TII->get(Primate::ANDI), SPReg)
            .addReg(SPReg)
            .addImm(-(int)MaxAlignment.value())
            .setMIFlag(MachineInstr::FrameSetup);
      } else {
        unsigned ShiftAmount = Log2(MaxAlignment);
        Register VR =
            MF.getRegInfo().createVirtualRegister(&Primate::GPRRegClass);
        BuildMI(MBB, MBBI, DL, TII->get(Primate::SRLI), VR)
            .addReg(SPReg)
            .addImm(ShiftAmount)
            .setMIFlag(MachineInstr::FrameSetup);
        BuildMI(MBB, MBBI, DL, TII->get(Primate::SLLI), SPReg)
            .addReg(VR)
            .addImm(ShiftAmount)
            .setMIFlag(MachineInstr::FrameSetup);
      }
      // FP will be used to restore the frame in the epilogue, so we need
      // another base register BP to record SP after re-alignment. SP will
      // track the current stack after allocating variable sized objects.
      if (hasBP(MF)) {
        // move BP, SP
        BuildMI(MBB, MBBI, DL, TII->get(Primate::ADDI), BPReg)
            .addReg(SPReg)
            .addImm(0)
            .setMIFlag(MachineInstr::FrameSetup);
      }
    }
  }
}

void PrimateFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  return;
  const PrimateRegisterInfo *RI = STI.getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();
  Register FPReg = getFPReg(STI);
  Register SPReg = getSPReg(STI);

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // Get the insert location for the epilogue. If there were no terminators in
  // the block, get the last instruction.
  MachineBasicBlock::iterator MBBI = MBB.end();
  DebugLoc DL;
  if (!MBB.empty()) {
    MBBI = MBB.getFirstTerminator();
    if (MBBI == MBB.end())
      MBBI = MBB.getLastNonDebugInstr();
    DL = MBBI->getDebugLoc();

    // If this is not a terminator, the actual insert location should be after the
    // last instruction.
    if (!MBBI->isTerminator())
      MBBI = std::next(MBBI);

    // If callee-saved registers are saved via libcall, place stack adjustment
    // before this call.
    while (MBBI != MBB.begin() &&
           std::prev(MBBI)->getFlag(MachineInstr::FrameDestroy))
      --MBBI;
  }

  const auto &CSI = getNonLibcallCSI(MF, MFI.getCalleeSavedInfo());

  // Skip to before the restores of callee-saved registers
  // FIXME: assumes exactly one instruction is used to restore each
  // callee-saved register.
  auto LastFrameDestroy = MBBI;
  if (!CSI.empty())
    LastFrameDestroy = std::prev(MBBI, CSI.size());

  uint64_t StackSize = MFI.getStackSize() + PRFI->getPRVPadding();
  uint64_t RealStackSize = StackSize + PRFI->getLibCallStackSize();
  uint64_t FPOffset = RealStackSize - PRFI->getVarArgsSaveSize();
  uint64_t PRVStackSize = PRFI->getPRVStackSize();

  // Restore the stack pointer using the value of the frame pointer. Only
  // necessary if the stack pointer was modified, meaning the stack size is
  // unknown.
  if (RI->hasStackRealignment(MF) || MFI.hasVarSizedObjects()) {
    assert(hasFP(MF) && "frame pointer should not have been eliminated");
    adjustReg(MBB, LastFrameDestroy, DL, SPReg, FPReg, -FPOffset,
              MachineInstr::FrameDestroy);
  } else {
    if (PRVStackSize)
      adjustStackForPRV(MF, MBB, LastFrameDestroy, DL, PRVStackSize,
                        MachineInstr::FrameDestroy);
  }

  uint64_t FirstSPAdjustAmount = getFirstSPAdjustAmount(MF);
  if (FirstSPAdjustAmount) {
    uint64_t SecondSPAdjustAmount = MFI.getStackSize() - FirstSPAdjustAmount;
    assert(SecondSPAdjustAmount > 0 &&
           "SecondSPAdjustAmount should be greater than zero");

    adjustReg(MBB, LastFrameDestroy, DL, SPReg, SPReg, SecondSPAdjustAmount,
              MachineInstr::FrameDestroy);
  }

  if (FirstSPAdjustAmount)
    StackSize = FirstSPAdjustAmount;

  // Deallocate stack
  adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackSize, MachineInstr::FrameDestroy);

  // Emit epilogue for shadow call stack.
  emitSCSEpilogue(MF, MBB, MBBI, DL);
}

StackOffset
PrimateFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                           Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();
  const auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();

  // Callee-saved registers should be referenced relative to the stack
  // pointer (positive offset), otherwise use the frame pointer (negative
  // offset).
  const auto &CSI = getNonLibcallCSI(MF, MFI.getCalleeSavedInfo());
  int MinCSFI = 0;
  int MaxCSFI = -1;
  StackOffset Offset;
  auto StackID = MFI.getStackID(FI);

  assert((StackID == TargetStackID::Default ||
          StackID == TargetStackID::ScalableVector) &&
         "Unexpected stack ID for the frame object.");
  if (StackID == TargetStackID::Default) {
    Offset =
        StackOffset::getFixed(MFI.getObjectOffset(FI) - getOffsetOfLocalArea() +
                              MFI.getOffsetAdjustment());
  } else if (StackID == TargetStackID::ScalableVector) {
    Offset = StackOffset::getScalable(MFI.getObjectOffset(FI));
  }

  uint64_t FirstSPAdjustAmount = getFirstSPAdjustAmount(MF);

  if (CSI.size()) {
    MinCSFI = CSI[0].getFrameIdx();
    MaxCSFI = CSI[CSI.size() - 1].getFrameIdx();
  }

  if (FI >= MinCSFI && FI <= MaxCSFI) {
    FrameReg = Primate::X2;

    if (FirstSPAdjustAmount)
      Offset += StackOffset::getFixed(FirstSPAdjustAmount);
    else
      Offset +=
          StackOffset::getFixed(MFI.getStackSize() + PRFI->getPRVPadding());
  } else if (RI->hasStackRealignment(MF) && !MFI.isFixedObjectIndex(FI)) {
    // If the stack was realigned, the frame pointer is set in order to allow
    // SP to be restored, so we need another base register to record the stack
    // after realignment.
    if (hasBP(MF)) {
      FrameReg = PrimateABI::getBPReg();
      // |--------------------------| -- <-- FP
      // | callee-saved registers   | | <----.
      // |--------------------------| --     |
      // | realignment (the size of | |      |
      // | this area is not counted | |      |
      // | in MFI.getStackSize())   | |      |
      // |--------------------------| --     |
      // | Padding after PRV        | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |-- MFI.getStackSize()
      // | PRV objects              | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | Padding before PRV       | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | scalar local variables   | | <----'
      // |--------------------------| -- <-- BP
      // | VarSize objects          | |
      // |--------------------------| -- <-- SP
    } else {
      FrameReg = Primate::X2;
      // |--------------------------| -- <-- FP
      // | callee-saved registers   | | <----.
      // |--------------------------| --     |
      // | realignment (the size of | |      |
      // | this area is not counted | |      |
      // | in MFI.getStackSize())   | |      |
      // |--------------------------| --     |
      // | Padding after PRV        | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |-- MFI.getStackSize()
      // | PRV objects              | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | Padding before PRV       | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | scalar local variables   | | <----'
      // |--------------------------| -- <-- SP
    }
    // The total amount of padding surrounding PRV objects is described by
    // PRV->getPRVPadding() and it can be zero. It allows us to align the PRV
    // objects to 8 bytes.
    if (MFI.getStackID(FI) == TargetStackID::Default) {
      Offset += StackOffset::getFixed(MFI.getStackSize());
      if (FI < 0)
        Offset += StackOffset::getFixed(PRFI->getLibCallStackSize());
    } else if (MFI.getStackID(FI) == TargetStackID::ScalableVector) {
      Offset += StackOffset::get(
          alignTo(MFI.getStackSize() - PRFI->getCalleeSavedStackSize(), 8),
          PRFI->getPRVStackSize());
    }
  } else {
    FrameReg = RI->getFrameRegister(MF);
    if (hasFP(MF)) {
      Offset += StackOffset::getFixed(PRFI->getVarArgsSaveSize());
      if (FI >= 0)
        Offset -= StackOffset::getFixed(PRFI->getLibCallStackSize());
      // When using FP to access scalable vector objects, we need to minus
      // the frame size.
      //
      // |--------------------------| -- <-- FP
      // | callee-saved registers   | |
      // |--------------------------| | MFI.getStackSize()
      // | scalar local variables   | |
      // |--------------------------| -- (Offset of PRV objects is from here.)
      // | PRV objects              |
      // |--------------------------|
      // | VarSize objects          |
      // |--------------------------| <-- SP
      if (MFI.getStackID(FI) == TargetStackID::ScalableVector)
        Offset -= StackOffset::getFixed(MFI.getStackSize());
    } else {
      // When using SP to access frame objects, we need to add PRV stack size.
      //
      // |--------------------------| -- <-- FP
      // | callee-saved registers   | | <----.
      // |--------------------------| --     |
      // | Padding after PRV        | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | PRV objects              | |      |-- MFI.getStackSize()
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | Padding before PRV       | |      |
      // | (not counted in          | |      |
      // | MFI.getStackSize()       | |      |
      // |--------------------------| --     |
      // | scalar local variables   | | <----'
      // |--------------------------| -- <-- SP
      //
      // The total amount of padding surrounding PRV objects is described by
      // PRV->getPRVPadding() and it can be zero. It allows us to align the PRV
      // objects to 8 bytes.
      if (MFI.getStackID(FI) == TargetStackID::Default) {
        if (MFI.isFixedObjectIndex(FI)) {
          Offset += StackOffset::get(MFI.getStackSize() + PRFI->getPRVPadding() 
                        + PRFI->getLibCallStackSize(), PRFI->getPRVStackSize());
        } else {
          Offset += StackOffset::getFixed(MFI.getStackSize());
        }
      } else if (MFI.getStackID(FI) == TargetStackID::ScalableVector) {
        Offset += StackOffset::get(
            alignTo(MFI.getStackSize() - PRFI->getCalleeSavedStackSize(), 8),
            PRFI->getPRVStackSize());
      }
    }
  }

  return Offset;
}

void PrimateFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // Unconditionally spill RA and FP only if the function uses a frame
  // pointer.
  if (hasFP(MF)) {
    SavedRegs.set(Primate::X1);
    SavedRegs.set(Primate::X8);
  }
  // Mark BP as used if function has dedicated base pointer.
  if (hasBP(MF))
    SavedRegs.set(PrimateABI::getBPReg());

  // If interrupt is enabled and there are calls in the handler,
  // unconditionally save all Caller-saved registers and
  // all FP registers, regardless whether they are used.
  MachineFrameInfo &MFI = MF.getFrameInfo();

  if (MF.getFunction().hasFnAttribute("interrupt") && MFI.hasCalls()) {

    static const MCPhysReg CSRegs[] = { Primate::X1,      /* ra */
      Primate::X5, Primate::X6, Primate::X7,                  /* t0-t2 */
      Primate::X10, Primate::X11,                           /* a0-a1, a2-a7 */
      Primate::X12, Primate::X13, Primate::X14, Primate::X15, Primate::X16, Primate::X17,
      Primate::X28, Primate::X29, Primate::X30, Primate::X31, 0 /* t3-t6 */
    };

    for (unsigned i = 0; CSRegs[i]; ++i)
      SavedRegs.set(CSRegs[i]);

    if (MF.getSubtarget<PrimateSubtarget>().hasStdExtF()) {

      // If interrupt is enabled, this list contains all FP registers.
      const MCPhysReg * Regs = MF.getRegInfo().getCalleeSavedRegs();

      for (unsigned i = 0; Regs[i]; ++i)
        if (Primate::FPR16RegClass.contains(Regs[i]) ||
            Primate::FPR32RegClass.contains(Regs[i]) ||
            Primate::FPR64RegClass.contains(Regs[i]))
          SavedRegs.set(Regs[i]);
    }
  }
}

int64_t
PrimateFrameLowering::assignPRVStackObjectOffsets(MachineFrameInfo &MFI) const {
  int64_t Offset = 0;
  // Create a buffer of PRV objects to allocate.
  SmallVector<int, 8> ObjectsToAllocate;
  for (int I = 0, E = MFI.getObjectIndexEnd(); I != E; ++I) {
    unsigned StackID = MFI.getStackID(I);
    if (StackID != TargetStackID::ScalableVector)
      continue;
    if (MFI.isDeadObjectIndex(I))
      continue;

    ObjectsToAllocate.push_back(I);
  }

  // Allocate all PRV locals and spills
  for (int FI : ObjectsToAllocate) {
    // ObjectSize in bytes.
    int64_t ObjectSize = MFI.getObjectSize(FI);
    // If the data type is the fractional vector type, reserve one vector
    // register for it.
    if (ObjectSize < 8)
      ObjectSize = 8;
    // Currently, all scalable vector types are aligned to 8 bytes.
    Offset = alignTo(Offset + ObjectSize, 8);
    MFI.setObjectOffset(FI, -Offset);
  }

  return Offset;
}

static bool hasPRVSpillWithFIs(MachineFunction &MF, const PrimateInstrInfo &TII) {
  if (!MF.getSubtarget<PrimateSubtarget>().hasStdExtV())
    return false;
  return any_of(MF, [&TII](const MachineBasicBlock &MBB) {
    return any_of(MBB, [&TII](const MachineInstr &MI) {
      return TII.isPRVSpill(MI, /*CheckFIs*/ true);
    });
  });
}

void PrimateFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  const PrimateRegisterInfo *RegInfo =
      MF.getSubtarget<PrimateSubtarget>().getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterClass *RC = &Primate::GPRRegClass;
  auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();

  int64_t PRVStackSize = assignPRVStackObjectOffsets(MFI);
  PRFI->setPRVStackSize(PRVStackSize);
  const PrimateInstrInfo &TII = *MF.getSubtarget<PrimateSubtarget>().getInstrInfo();

  // estimateStackSize has been observed to under-estimate the final stack
  // size, so give ourselves wiggle-room by checking for stack size
  // representable an 11-bit signed field rather than 12-bits.
  // FIXME: It may be possible to craft a function with a small stack that
  // still needs an emergency spill slot for branch relaxation. This case
  // would currently be missed.
  // PRV loads & stores have no capacity to hold the immediate address offsets
  // so we must always reserve an emergency spill slot if the MachineFunction
  // contains any PRV spills.
  if (!isInt<11>(MFI.estimateStackSize(MF)) || hasPRVSpillWithFIs(MF, TII)) {
    int RegScavFI = MFI.CreateStackObject(RegInfo->getSpillSize(*RC),
                                          RegInfo->getSpillAlign(*RC), false);
    RS->addScavengingFrameIndex(RegScavFI);
    // For PRV, scalable stack offsets require up to two scratch registers to
    // compute the final offset. Reserve an additional emergency spill slot.
    if (PRVStackSize != 0) {
      int PRVRegScavFI = MFI.CreateStackObject(
          RegInfo->getSpillSize(*RC), RegInfo->getSpillAlign(*RC), false);
      RS->addScavengingFrameIndex(PRVRegScavFI);
    }
  }

  if (MFI.getCalleeSavedInfo().empty() || PRFI->useSaveRestoreLibCalls(MF)) {
    PRFI->setCalleeSavedStackSize(0);
    return;
  }

  unsigned Size = 0;
  for (const auto &Info : MFI.getCalleeSavedInfo()) {
    int FrameIdx = Info.getFrameIdx();
    if (MFI.getStackID(FrameIdx) != TargetStackID::Default)
      continue;

    Size += MFI.getObjectSize(FrameIdx);
  }
  PRFI->setCalleeSavedStackSize(Size);

  // Padding required to keep the PRV stack aligned to 8 bytes
  // within the main stack. We only need this when not using FP.
  if (PRVStackSize && !hasFP(MF) && Size % 8 != 0) {
    // Because we add the padding to the size of the stack, adding
    // getStackAlign() will keep it aligned.
    PRFI->setPRVPadding(getStackAlign().value());
  }
}

static bool hasPRVFrameObject(const MachineFunction &MF) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  for (int I = 0, E = MFI.getObjectIndexEnd(); I != E; ++I)
    if (MFI.getStackID(I) == TargetStackID::ScalableVector)
      return true;
  return false;
}

// Not preserve stack space within prologue for outgoing variables when the
// function contains variable size objects or there are vector objects accessed
// by the frame pointer.
// Let eliminateCallFramePseudoInstr preserve stack space for it.
bool PrimateFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects() &&
         !(hasFP(MF) && hasPRVFrameObject(MF));
}

// Eliminate ADJCALLSTACKDOWN, ADJCALLSTACKUP pseudo instructions.
MachineBasicBlock::iterator PrimateFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  Register SPReg = Primate::X2;
  DebugLoc DL = MI->getDebugLoc();

  if (!hasReservedCallFrame(MF)) {
    // If space has not been reserved for a call frame, ADJCALLSTACKDOWN and
    // ADJCALLSTACKUP must be converted to instructions manipulating the stack
    // pointer. This is necessary when there is a variable length stack
    // allocation (e.g. alloca), which means it's not possible to allocate
    // space for outgoing arguments from within the function prologue.
    int64_t Amount = MI->getOperand(0).getImm();

    if (Amount != 0) {
      // Ensure the stack remains aligned after adjustment.
      Amount = alignSPAdjust(Amount);

      if (MI->getOpcode() == Primate::ADJCALLSTACKDOWN)
        Amount = -Amount;

      adjustReg(MBB, MI, DL, SPReg, SPReg, Amount, MachineInstr::NoFlags);
    }
  }

  return MBB.erase(MI);
}

// We would like to split the SP adjustment to reduce prologue/epilogue
// as following instructions. In this way, the offset of the callee saved
// register could fit in a single store.
//   add     sp,sp,-2032
//   sw      ra,2028(sp)
//   sw      s0,2024(sp)
//   sw      s1,2020(sp)
//   sw      s3,2012(sp)
//   sw      s4,2008(sp)
//   add     sp,sp,-64
uint64_t
PrimateFrameLowering::getFirstSPAdjustAmount(const MachineFunction &MF) const {
  const auto *PRFI = MF.getInfo<PrimateMachineFunctionInfo>();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  uint64_t StackSize = MFI.getStackSize();

  // Disable SplitSPAdjust if save-restore libcall used. The callee saved
  // registers will be pushed by the save-restore libcalls, so we don't have to
  // split the SP adjustment in this case.
  if (PRFI->getLibCallStackSize())
    return 0;

  // Return the FirstSPAdjustAmount if the StackSize can not fit in signed
  // 12-bit and there exists a callee saved register need to be pushed.
  if (!isInt<12>(StackSize) && (CSI.size() > 0)) {
    // FirstSPAdjustAmount is choosed as (2048 - StackAlign)
    // because 2048 will cause sp = sp + 2048 in epilogue split into
    // multi-instructions. The offset smaller than 2048 can fit in signle
    // load/store instruction and we have to stick with the stack alignment.
    // 2048 is 16-byte alignment. The stack alignment for PR32 and PR64 is 16,
    // for PR32E is 4. So (2048 - StackAlign) will satisfy the stack alignment.
    return 2048 - getStackAlign().value();
  }
  return 0;
}

bool PrimateFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  const char *SpillLibCall = getSpillLibCallName(*MF, CSI);
  if (SpillLibCall) {
    // Add spill libcall via non-callee-saved register t0.
    BuildMI(MBB, MI, DL, TII.get(Primate::PseudoCALLReg), Primate::X5)
        .addExternalSymbol(SpillLibCall, PrimateII::MO_CALL)
        .setMIFlag(MachineInstr::FrameSetup);

    // Add registers spilled in libcall as liveins.
    for (auto &CS : CSI)
      MBB.addLiveIn(CS.getReg());
  }

  // Manually spill values not spilled by libcall.
  const auto &NonLibcallCSI = getNonLibcallCSI(*MF, CSI);
  for (auto &CS : NonLibcallCSI) {
    // Insert the spill to the stack frame.
    Register Reg = CS.getReg();
    const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
    TII.storeRegToStackSlot(MBB, MI, Reg, true, CS.getFrameIdx(), RC, TRI);
  }

  return true;
}

bool PrimateFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  // Manually restore values not restored by libcall. Insert in reverse order.
  // loadRegFromStackSlot can insert multiple instructions.
  const auto &NonLibcallCSI = getNonLibcallCSI(*MF, CSI);
  for (auto &CS : reverse(NonLibcallCSI)) {
    Register Reg = CS.getReg();
    const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
    TII.loadRegFromStackSlot(MBB, MI, Reg, CS.getFrameIdx(), RC, TRI);
    assert(MI != MBB.begin() && "loadRegFromStackSlot didn't insert any code!");
  }

  const char *RestoreLibCall = getRestoreLibCallName(*MF, CSI);
  if (RestoreLibCall) {
    // Add restore libcall via tail call.
    MachineBasicBlock::iterator NewMI =
        BuildMI(MBB, MI, DL, TII.get(Primate::PseudoTAIL))
            .addExternalSymbol(RestoreLibCall, PrimateII::MO_CALL)
            .setMIFlag(MachineInstr::FrameDestroy);

    // Remove trailing returns, since the terminator is now a tail call to the
    // restore function.
    if (MI != MBB.end() && MI->getOpcode() == Primate::PseudoRET) {
      NewMI->copyImplicitOps(*MF, *MI);
      MI->eraseFromParent();
    }
  }

  return true;
}

bool PrimateFrameLowering::canUseAsPrologue(const MachineBasicBlock &MBB) const {
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const MachineFunction *MF = MBB.getParent();
  const auto *PRFI = MF->getInfo<PrimateMachineFunctionInfo>();

  if (!PRFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Inserting a call to a __primate_save libcall requires the use of the register
  // t0 (X5) to hold the return address. Therefore if this register is already
  // used we can't insert the call.

  RegScavenger RS;
  RS.enterBasicBlock(*TmpMBB);
  return !RS.isRegUsed(Primate::X5);
}

bool PrimateFrameLowering::canUseAsEpilogue(const MachineBasicBlock &MBB) const {
  const MachineFunction *MF = MBB.getParent();
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const auto *PRFI = MF->getInfo<PrimateMachineFunctionInfo>();

  if (!PRFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Using the __primate_restore libcalls to restore CSRs requires a tail call.
  // This means if we still need to continue executing code within this function
  // the restore cannot take place in this basic block.

  if (MBB.succ_size() > 1)
    return false;

  MachineBasicBlock *SuccMBB =
      MBB.succ_empty() ? TmpMBB->getFallThrough() : *MBB.succ_begin();

  // Doing a tail call should be safe if there are no successors, because either
  // we have a returning block or the end of the block is unreachable, so the
  // restore will be eliminated regardless.
  if (!SuccMBB)
    return true;

  // The successor can only contain a return, since we would effectively be
  // replacing the successor with our own tail return at the end of our block.
  return SuccMBB->isReturnBlock() && SuccMBB->size() == 1;
}

bool PrimateFrameLowering::isSupportedStackID(TargetStackID::Value ID) const {
  switch (ID) {
  case TargetStackID::Default:
  case TargetStackID::ScalableVector:
    return true;
  case TargetStackID::NoAlloc:
  case TargetStackID::SGPRSpill:
  case TargetStackID::WasmLocal:
    return false;
  }
  llvm_unreachable("Invalid TargetStackID::Value");
}

TargetStackID::Value PrimateFrameLowering::getStackIDForScalableVectors() const {
  return TargetStackID::ScalableVector;
}
