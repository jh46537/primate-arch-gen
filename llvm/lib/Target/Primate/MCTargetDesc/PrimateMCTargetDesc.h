//===-- PrimateMCTargetDesc.h - Primate Target Descriptions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Primate specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEMCTARGETDESC_H
#define LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEMCTARGETDESC_H

#include "llvm/Config/config.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;

StringRef selectPrimateCPU(StringRef CPU, bool Is64Bit);

MCCodeEmitter *createPrimateMCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx);

MCAsmBackend *createPrimateAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                    const MCRegisterInfo &MRI,
                                    const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createPrimateELFObjectWriter(uint8_t OSABI,
                                                                 bool Is64Bit);
}

// Defines symbolic names for Primate registers.
#define GET_REGINFO_ENUM
#include "PrimateGenRegisterInfo.inc"

// Defines symbolic names for Primate instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "PrimateGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "PrimateGenSubtargetInfo.inc"

#endif
