
//===-- PrimateTargetParser - Parser for target features ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a target parser to recognise hardware features
// for Primate CPUs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGETPARSER_PRIMATETARGETPARSER_H
#define LLVM_TARGETPARSER_PRIMATETARGETPARSER_H

#include "llvm/ADT/StringRef.h"

namespace llvm {

class Triple;

namespace Primate {

// We use 64 bits as the known part in the scalable vector types.
static constexpr unsigned PVVBitsPerBlock = 64;

bool parseCPU(StringRef CPU, bool IsRV64);
bool parseTuneCPU(StringRef CPU, bool IsRV64);
StringRef getMArchFromMcpu(StringRef CPU);
void fillValidCPUArchList(SmallVectorImpl<StringRef> &Values, bool IsPR64);
void fillValidTuneCPUArchList(SmallVectorImpl<StringRef> &Values, bool IsPR64);
bool hasFastUnalignedAccess(StringRef CPU);

} // namespace Primate 
} // namespace llvm

#endif
