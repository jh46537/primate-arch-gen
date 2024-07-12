//===-- Primate.td - Describe the Primate Target Machine ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Primate subtarget features and instruction predicates.
//===----------------------------------------------------------------------===//

include "PrimateFeatures.td"

//===----------------------------------------------------------------------===//
// Named operands for CSR instructions.
//===----------------------------------------------------------------------===//

include "PrimateSystemOperands.td"

//===----------------------------------------------------------------------===//
// Registers, calling conventions, instruction descriptions.
//===----------------------------------------------------------------------===//

include "PrimateSchedule.td"
include "PrimateRegisterInfo.td"
include "PrimateCallingConv.td"
include "PrimateInstrInfo.td"
include "PrimateRegisterBanks.td"
include "PrimateSchedPrimate.td"

//===----------------------------------------------------------------------===//
// Primate processors supported.
//===----------------------------------------------------------------------===//

include "PrimateProcessors.td"

//===----------------------------------------------------------------------===//
// Define the Primate target.
//===----------------------------------------------------------------------===//

def PrimateInstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def PrimateAsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = 1;
  let AllowDuplicateRegisterNames = 1;
}

def PrimateAsmWriter : AsmWriter {
  int PassSubtarget = 1;
}

def Primate : Target {
  let InstructionSet = PrimateInstrInfo;
  let AssemblyParsers = [PrimateAsmParser];
  let AssemblyWriters = [PrimateAsmWriter];
  let AllowRegisterRenaming = 1;
}
