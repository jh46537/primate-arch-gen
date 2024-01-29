//===-- PrimatePasses.h - Primate Middle End Passes --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
/// \file
//===----------------------------------------------------------------------===//

#ifndef PRIMATE_STRUCT_TO_AGGRE_H
#define PRIMATE_STRUCT_TO_AGGRE_H
#include <algorithm>

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
  struct PrimateStructToAggre : public PassInfoMixin<PrimateStructToAggre> {
    std::unordered_map<Function*, Function*> replacedFunctions;
    std::unordered_map<AllocaInst*, Value*> latestAllocaValue;
    SmallVector<Instruction*> instructionsToRemove;

    PreservedAnalyses run(Module& M, ModuleAnalysisManager& PA);

    void normalizeFuncs(Function& F);
    void removeAllocas(Function& F);
    void convertCall(CallInst *ci, AllocaInst *ai);
    void convertAndTrimGEP(GetElementPtrInst* gepI);

  };
}

#endif
