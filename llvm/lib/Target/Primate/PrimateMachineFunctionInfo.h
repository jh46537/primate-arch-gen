//=- PrimateMachineFunctionInfo.h - Primate machine function info -----*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Primate-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATEMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATEMACHINEFUNCTIONINFO_H

#include "PrimateSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// PrimateMachineFunctionInfo - This class is derived from MachineFunctionInfo
/// and contains private Primate-specific information for each MachineFunction.
class PrimateMachineFunctionInfo : public MachineFunctionInfo {
private:
  /// FrameIndex for start of varargs area
  int VarArgsFrameIndex = 0;
  /// Size of the save area used for varargs
  int VarArgsSaveSize = 0;
  /// FrameIndex used for transferring values between 64-bit FPRs and a pair
  /// of 32-bit GPRs via the stack.
  int MoveF64FrameIndex = -1;
  /// Size of any opaque stack adjustment due to save/restore libcalls.
  unsigned LibCallStackSize = 0;
  /// Size of PRV stack.
  uint64_t PRVStackSize = 0;
  /// Padding required to keep PRV stack aligned within the main stack.
  uint64_t PRVPadding = 0;
  /// Size of stack frame to save callee saved registers
  unsigned CalleeSavedStackSize = 0;

public:
  PrimateMachineFunctionInfo(const MachineFunction &MF) {}

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  unsigned getVarArgsSaveSize() const { return VarArgsSaveSize; }
  void setVarArgsSaveSize(int Size) { VarArgsSaveSize = Size; }

  int getMoveF64FrameIndex(MachineFunction &MF) {
    if (MoveF64FrameIndex == -1)
      MoveF64FrameIndex =
          MF.getFrameInfo().CreateStackObject(8, Align(8), false);
    return MoveF64FrameIndex;
  }

  unsigned getLibCallStackSize() const { return LibCallStackSize; }
  void setLibCallStackSize(unsigned Size) { LibCallStackSize = Size; }

  bool useSaveRestoreLibCalls(const MachineFunction &MF) const {
    // We cannot use fixed locations for the callee saved spill slots if the
    // function uses a varargs save area, or is an interrupt handler.
    return MF.getSubtarget<PrimateSubtarget>().enableSaveRestore() &&
           VarArgsSaveSize == 0 && !MF.getFrameInfo().hasTailCall() &&
           !MF.getFunction().hasFnAttribute("interrupt");
  }

  uint64_t getPRVStackSize() const { return PRVStackSize; }
  void setPRVStackSize(uint64_t Size) { PRVStackSize = Size; }

  uint64_t getPRVPadding() const { return PRVPadding; }
  void setPRVPadding(uint64_t Padding) { PRVPadding = Padding; }

  unsigned getCalleeSavedStackSize() const { return CalleeSavedStackSize; }
  void setCalleeSavedStackSize(unsigned Size) { CalleeSavedStackSize = Size; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_Primate_PrimateMACHINEFUNCTIONINFO_H
