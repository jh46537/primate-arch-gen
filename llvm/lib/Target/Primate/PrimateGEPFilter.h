//===-- PrimatePasses.h - Primate Middle End Passes --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
/// \file
//===----------------------------------------------------------------------===//

#ifndef PRIMATE_PASSES_H
#define PRIMATE_PASSES_H
#define PRIMATE_GEP_PASS_NAME "Primate GEP Remove Pass"
#define PRIMATE_LOAD_PASS_NAME "Primate Load Combine Pass"
#include <algorithm>

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsPrimate.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
  struct PrimateGEPFilterPass : public PassInfoMixin<PrimateGEPFilterPass> {
    PreservedAnalyses run(Module& F, ModuleAnalysisManager& PA);
  };
}

#endif
