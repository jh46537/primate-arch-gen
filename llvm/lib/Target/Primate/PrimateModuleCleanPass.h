//===-- PrimateIntrinsicPromotion.h - Primate Intrinsic Promotion Pass --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 
// Remove functions that are not used.
//
/// \file
//===----------------------------------------------------------------------===//

#ifndef PRIMATE_MODULE_CLEAN_PROMOTION_H
#define PRIMATE_MODULE_CLEAN_PROMOTION_H
#include <algorithm>
#include <set>

#include "Primate.h"
#include "PrimateTargetMachine.h"
#include "llvm/IR/User.h" 

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

#include "PrimateBFUTypeFindingPass.h"
#include "llvm/IR/IntrinsicsPrimate.h"

namespace llvm {
  struct PrimateModuleCleanPass : public PassInfoMixin<PrimateModuleCleanPass> {
    PrimateModuleCleanPass() {}

    PreservedAnalyses run(Module&, ModuleAnalysisManager&);
    static bool isRequired() { return true; }
  };
}

#endif
