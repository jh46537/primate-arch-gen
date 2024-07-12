//===-- PrimateBaseInfo.cpp - Top level definitions for Primate MC ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone enum definitions for the Primate target
// useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//

#include "PrimateBaseInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/RISCVISAInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/TargetParser.h"
#include "llvm/TargetParser/Triple.h"

namespace llvm {
namespace PrimateSysReg {
#define GET_SysRegsList_IMPL
#include "PrimateGenSearchableTables.inc"
} // namespace PrimateSysReg

namespace PrimateABI {
ABI computeTargetABI(const Triple &TT, FeatureBitset FeatureBits,
                     StringRef ABIName) {
  auto TargetABI = getTargetABI(ABIName);
  bool IsPR64 = TT.isArch64Bit();
  bool IsPRE = FeatureBits[Primate::FeaturePRE];

  if (!ABIName.empty() && TargetABI == ABI_Unknown) {
    errs()
        << "'" << ABIName
        << "' is not a recognized ABI for this target (ignoring target-abi)\n";
  } else if (ABIName.starts_with("ilp32") && IsPR64) {
    errs() << "32-bit ABIs are not supported for 64-bit targets (ignoring "
              "target-abi)\n";
    TargetABI = ABI_Unknown;
  } else if (ABIName.starts_with("lp64") && !IsPR64) {
    errs() << "64-bit ABIs are not supported for 32-bit targets (ignoring "
              "target-abi)\n";
    TargetABI = ABI_Unknown;
  } else if (IsPRE && TargetABI != ABI_ILP32E && TargetABI != ABI_Unknown) {
    // TODO: move this checking to PrimateTargetLowering and PrimateAsmParser
    errs()
        << "Only the ilp32e ABI is supported for PR32E (ignoring target-abi)\n";
    TargetABI = ABI_Unknown;
  }

  if (TargetABI != ABI_Unknown)
    return TargetABI;

  // For now, default to the ilp32/ilp32e/lp64 ABI if no explicit ABI is given
  // or an invalid/unrecognised string is given. In the future, it might be
  // worth changing this to default to ilp32f/lp64f and ilp32d/lp64d when
  // hardware support for floating point is present.
  if (IsPRE)
    return ABI_ILP32E;
  if (IsPR64)
    return ABI_LP64;
  return ABI_ILP32;
}

ABI getTargetABI(StringRef ABIName) {
  auto TargetABI = StringSwitch<ABI>(ABIName)
                       .Case("ilp32", ABI_ILP32)
                       .Case("ilp32f", ABI_ILP32F)
                       .Case("ilp32d", ABI_ILP32D)
                       .Case("ilp32e", ABI_ILP32E)
                       .Case("lp64", ABI_LP64)
                       .Case("lp64f", ABI_LP64F)
                       .Case("lp64d", ABI_LP64D)
                       .Default(ABI_Unknown);
  return TargetABI;
}

// To avoid the BP value clobbered by a function call, we need to choose a
// callee saved register to save the value. PR32E only has X8 and X9 as callee
// saved registers and X8 will be used as fp. So we choose X9 as bp.
MCRegister getBPReg() { return Primate::X9; }

// Returns the register holding shadow call stack pointer.
MCRegister getSCSPReg() { return Primate::X18; }

} // namespace PrimateABI

namespace PrimateFeatures {

void validate(const Triple &TT, const FeatureBitset &FeatureBits) {
  if (TT.isArch64Bit() && !FeatureBits[Primate::Feature64Bit])
    report_fatal_error("PR64 target requires an PR64 CPU");
  if (!TT.isArch64Bit() && FeatureBits[Primate::Feature64Bit])
    report_fatal_error("PR32 target requires an PR32 CPU");
  if (TT.isArch64Bit() && FeatureBits[Primate::FeaturePRE])
    report_fatal_error("PR32E can't be enabled for an PR64 target");
}

} // namespace PrimateFeatures

// Encode VTYPE into the binary format used by the the VSETVLI instruction which
// is used by our MC layer representation.
//
// Bits | Name       | Description
// -----+------------+------------------------------------------------
// 7    | vma        | Vector mask agnostic
// 6    | vta        | Vector tail agnostic
// 5:3  | vsew[2:0]  | Standard element width (SEW) setting
// 2:0  | vlmul[2:0] | Vector register group multiplier (LMUL) setting
unsigned PrimateVType::encodeVTYPE(PrimateII::VLMUL VLMUL, unsigned SEW,
                                 bool TailAgnostic, bool MaskAgnostic) {
  assert(isValidSEW(SEW) && "Invalid SEW");
  unsigned VLMULBits = static_cast<unsigned>(VLMUL);
  unsigned VSEWBits = Log2_32(SEW) - 3;
  unsigned VTypeI = (VSEWBits << 3) | (VLMULBits & 0x7);
  if (TailAgnostic)
    VTypeI |= 0x40;
  if (MaskAgnostic)
    VTypeI |= 0x80;

  return VTypeI;
}

std::pair<unsigned, bool> PrimateVType::decodeVLMUL(PrimateII::VLMUL VLMUL) {
  switch (VLMUL) {
  default:
    llvm_unreachable("Unexpected LMUL value!");
  case PrimateII::VLMUL::LMUL_1:
  case PrimateII::VLMUL::LMUL_2:
  case PrimateII::VLMUL::LMUL_4:
  case PrimateII::VLMUL::LMUL_8:
    return std::make_pair(1 << static_cast<unsigned>(VLMUL), false);
  case PrimateII::VLMUL::LMUL_F2:
  case PrimateII::VLMUL::LMUL_F4:
  case PrimateII::VLMUL::LMUL_F8:
    return std::make_pair(1 << (8 - static_cast<unsigned>(VLMUL)), true);
  }
}

void PrimateVType::printVType(unsigned VType, raw_ostream &OS) {
  unsigned Sew = getSEW(VType);
  OS << "e" << Sew;

  unsigned LMul;
  bool Fractional;
  std::tie(LMul, Fractional) = decodeVLMUL(getVLMUL(VType));

  if (Fractional)
    OS << ", mf";
  else
    OS << ", m";
  OS << LMul;

  if (isTailAgnostic(VType))
    OS << ", ta";
  else
    OS << ", tu";

  if (isMaskAgnostic(VType))
    OS << ", ma";
  else
    OS << ", mu";
}

} // namespace llvm
