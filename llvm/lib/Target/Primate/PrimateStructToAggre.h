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
#include <set>

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

#include "PrimateBFUTypeFindingPass.h"
#include "llvm/IR/IntrinsicsPrimate.h"

namespace llvm {
  struct PrimateStructToAggre : public PassInfoMixin<PrimateStructToAggre> {
    std::unordered_map<Function*, Function*> replacedFunctions;
    std::unordered_map<AllocaInst*, Value*> latestAllocaValue;
    std::set<Value*> fixedCalls;
    SmallVector<Instruction*> instructionsToRemove;
    std::set<Type*> BFUTypes;
    std::map<StringRef, llvm::Intrinsic::PRIMATEIntrinsics> nameToIntrins;

    PreservedAnalyses run(Module& M, ModuleAnalysisManager& PA);

    Type* followPointerForType(Value*);
    void normalizeFuncs(Function& F);
    void removeAllocas(Function& F);
    void convertCall(CallInst *ci, AllocaInst *ai);
    void convertAndTrimGEP(GetElementPtrInst* gepI);
    void findBFUTypes(Module& M);
    bool isBFUType(Type* ty);

  };
}

#endif
