//===-- PrimateCallLowering.cpp - Call lowering -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file implements the lowering of LLVM calls to machine code calls for
/// GlobalISel.
//
//===----------------------------------------------------------------------===//

#include "PrimateCallLowering.h"
#include "PrimateISelLowering.h"
#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"

#define DEBUG_TYPE "PrimateCallLowering"

using namespace llvm;

PrimateCallLowering::PrimateCallLowering(const PrimateTargetLowering &TLI)
    : CallLowering(&TLI) {}

bool PrimateCallLowering::lowerReturn(MachineIRBuilder &MIRBuilder,
                                    const Value *Val, ArrayRef<Register> VRegs,
                                    FunctionLoweringInfo &FLI) const {

  MachineInstrBuilder Ret = MIRBuilder.buildInstrNoInsert(Primate::PseudoRET);

  if (Val != nullptr) {
    return false;
  }
  MIRBuilder.insertInstr(Ret);
  return true;
}

bool PrimateCallLowering::lowerFormalArguments(MachineIRBuilder &MIRBuilder,
                                             const Function &F,
                                             ArrayRef<ArrayRef<Register>> VRegs,
                                             FunctionLoweringInfo &FLI) const {

  LLVM_DEBUG(dbgs() << "Trying to lower args for "; F.dump());
  if (F.arg_empty())
    return true;

  return false;
}

bool PrimateCallLowering::lowerCall(MachineIRBuilder &MIRBuilder,
                                  CallLoweringInfo &Info) const {
  LLVM_DEBUG(dbgs() << "Trying to lower "; Info.Callee.dump());
  return false;
}
