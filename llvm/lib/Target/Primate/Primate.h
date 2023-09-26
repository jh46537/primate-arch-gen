//===-- Primate.h - Top-level interface for Primate -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// Primate back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATE_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATE_H

#include "MCTargetDesc/PrimateBaseInfo.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class PrimateRegisterBankInfo;
class PrimateSubtarget;
class PrimateTargetMachine;
class AsmPrinter;
class FunctionPass;
class InstructionSelector;
class MCInst;
class MCOperand;
class MachineInstr;
class MachineOperand;
class PassRegistry;

bool lowerPrimateMachineInstrToMCInst(const MachineInstr *MI, MCInst &OutMI,
                                    AsmPrinter &AP);
bool LowerPrimateMachineOperandToMCOperand(const MachineOperand &MO,
                                         MCOperand &MCOp, const AsmPrinter &AP);

FunctionPass *createPrimateISelDag(PrimateTargetMachine &TM);

FunctionPass *createPrimateMergeBaseOffsetOptPass();
void initializePrimateMergeBaseOffsetOptPass(PassRegistry &);

FunctionPass *createPrimateExpandPseudoPass();
void initializePrimateExpandPseudoPass(PassRegistry &);

FunctionPass *createPrimateExpandAtomicPseudoPass();
void initializePrimateExpandAtomicPseudoPass(PassRegistry &);

FunctionPass *createPrimateInsertVSETVLIPass();
void initializePrimateInsertVSETVLIPass(PassRegistry &);

FunctionPass *createPrimatePacketizer();
void initializePrimatePacketizerPass(PassRegistry &);

FunctionPass *createPrimateStructToRegPass();
void initializePrimateStructToRegPassPass(PassRegistry &);

InstructionSelector *createPrimateInstructionSelector(const PrimateTargetMachine &,
                                                    PrimateSubtarget &,
                                                    PrimateRegisterBankInfo &);
}

#endif
