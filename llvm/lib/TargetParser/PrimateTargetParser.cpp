
//===-- PrimateTargetParser.cpp - Parser for target features ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a target parser to recognise hardware features
// for RISC-V CPUs.
//
//===----------------------------------------------------------------------===//

#include "llvm/TargetParser/PrimateTargetParser.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/TargetParser/Triple.h"

namespace llvm {
namespace Primate {

enum CPUKind : unsigned {
#define PROC(ENUM, NAME, DEFAULT_MARCH, FAST_UNALIGN) CK_##ENUM,
#define TUNE_PROC(ENUM, NAME) CK_##ENUM,
#include "llvm/TargetParser/PrimateTargetParserDef.inc"
};

struct CPUInfo {
  StringLiteral Name;
  StringLiteral DefaultMarch;
  bool FastUnalignedAccess;
  bool is64Bit() const { return DefaultMarch.starts_with("pr64"); }
};

constexpr CPUInfo PrimateCPUInfo[] = {
#define PROC(ENUM, NAME, DEFAULT_MARCH, FAST_UNALIGN)                          \
  {NAME, DEFAULT_MARCH, FAST_UNALIGN},
#include "llvm/TargetParser/PrimateTargetParserDef.inc"
};

static const CPUInfo *getCPUInfoByName(StringRef CPU) {
  for (auto &C : PrimateCPUInfo)
    if (C.Name == CPU)
      return &C;
  return nullptr;
}

bool hasFastUnalignedAccess(StringRef CPU) {
  const CPUInfo *Info = getCPUInfoByName(CPU);
  return Info && Info->FastUnalignedAccess;
}

bool parseCPU(StringRef CPU, bool IsPR64) {
  const CPUInfo *Info = getCPUInfoByName(CPU);

  if (!Info)
    return false;
  return Info->is64Bit() == IsPR64;
}

bool parseTuneCPU(StringRef TuneCPU, bool IsPR64) {
  std::optional<CPUKind> Kind =
      llvm::StringSwitch<std::optional<CPUKind>>(TuneCPU)
#define TUNE_PROC(ENUM, NAME) .Case(NAME, CK_##ENUM)
  #include "llvm/TargetParser/PrimateTargetParserDef.inc"
      .Default(std::nullopt);

  if (Kind.has_value())
    return true;

  // Fallback to parsing as a CPU.
  return parseCPU(TuneCPU, IsPR64);
}

StringRef getMArchFromMcpu(StringRef CPU) {
  const CPUInfo *Info = getCPUInfoByName(CPU);
  if (!Info)
    return "";
  return Info->DefaultMarch;
}

void fillValidCPUArchList(SmallVectorImpl<StringRef> &Values, bool IsPR64) {
  for (const auto &C : PrimateCPUInfo) {
    if (IsPR64 == C.is64Bit())
      Values.emplace_back(C.Name);
  }
}

void fillValidTuneCPUArchList(SmallVectorImpl<StringRef> &Values, bool IsPR64) {
  for (const auto &C : PrimateCPUInfo) {
    if (IsPR64 == C.is64Bit())
      Values.emplace_back(C.Name);
  }
#define TUNE_PROC(ENUM, NAME) Values.emplace_back(StringRef(NAME));
#include "llvm/TargetParser/PrimateTargetParserDef.inc"
}

} // namespace Primate 
} // namespace llvm
