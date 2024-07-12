//===-- PrimateTargetInfo.cpp - Primate Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/PrimateTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getThePrimate32Target() {
  static Target ThePrimate32Target;
  return ThePrimate32Target;
}

Target &llvm::getThePrimate64Target() {
  static Target ThePrimate64Target;
  return ThePrimate64Target;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePrimateTargetInfo() {
  RegisterTarget<Triple::primate32> X(getThePrimate32Target(), "primate32",
                                    "32-bit Primate", "Primate");
  RegisterTarget<Triple::primate64> Y(getThePrimate64Target(), "primate64",
                                    "64-bit Primate", "Primate");
}
