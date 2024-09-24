//===-- PrimateIntrinsicPromotion.h - Primate Intrinsic Promotion Pass --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 
// This pass will take primate BFU intrinsics and move them from pointer to ssa values
// All we need to do is replace pointer with ssa, and then store it in wherever the pointer was copied
// ideally we are early enough to not have to worry about the pointer being the alias
//
/// \file
//===----------------------------------------------------------------------===//

#ifndef PRIMATE_INTRINSIC_PROMOTION_H
#define PRIMATE_INTRINSIC_PROMOTION_H
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
  struct PrimateIntrinsicPromotion : public PassInfoMixin<PrimateIntrinsicPromotion> {
    std::unordered_map<Function*, Function*> replacedFunctions;
    std::unordered_map<AllocaInst*, Value*> latestAllocaValue;
    std::set<Value*> fixedCalls;
    SmallVector<Instruction*> instructionsToRemove;
    std::set<Type*> BFUTypes;
    std::map<std::string, llvm::Intrinsic::PRIMATEIntrinsics> nameToIntrins;
    const TargetLowering* TLI;
    TargetMachine& TM; 
    PrimateIntrinsicPromotion(TargetMachine& TM) : TM(TM){}

    PreservedAnalyses run(Function&, FunctionAnalysisManager&);
    static bool isRequired() { return true; }
    void promoteReturnType(std::vector<CallInst*>& worklist);
    void promoteArgs(std::vector<CallInst*>& worklist);

  };
}

#endif
