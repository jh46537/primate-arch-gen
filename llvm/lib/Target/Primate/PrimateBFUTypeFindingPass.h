//===-- PrimatePasses.h - Primate Middle End Passes --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
/// \file
// Pass responsible for finding all the types that the BFUs use in primate
//===----------------------------------------------------------------------===//

#ifndef PRIMATE_BFU_TYPE_FINDING_h
#define PRIMATE_BFU_TYPE_FINDING_h
#include <algorithm>
#include <set>

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
  struct PrimateBFUTypeFinding : public AnalysisInfoMixin<PrimateBFUTypeFinding> {
    std::set<Type*> BFUTypes;

    static AnalysisKey Key;

    using Result = std::set<Type*>;
    Result run(Module& M, ModuleAnalysisManager& PA);
  };
}

#endif
