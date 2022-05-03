//===-- PrimateTargetInfo.h - Primate Target Implementation ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_TARGETINFO_PRIMATETARGETINFO_H
#define LLVM_LIB_TARGET_PRIMATE_TARGETINFO_PRIMATETARGETINFO_H

namespace llvm {

class Target;

Target &getThePrimate32Target();
Target &getThePrimate64Target();

} // namespace llvm

#endif // LLVM_LIB_TARGET_Primate_TARGETINFO_PrimateTARGETINFO_H
