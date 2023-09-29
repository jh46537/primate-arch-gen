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
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& PA);
  };

  struct PrimateStructLoadCombinerPass : public PassInfoMixin<PrimateStructLoadCombinerPass> {
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& PA) {
      dbgs() << "weeee" << F.getName() << "\n";
      for(auto& bb: F) {
	for(auto& inst: bb) {
	  if (llvm::isa<llvm::GetElementPtrInst>(inst)) {
	    dbgs() << "inspecting instr: ";
	    inst.dump();
	    for(auto u = inst.user_begin(); u != inst.user_end(); u++) {
	      u->dump();
	    }
	    dbgs() << "end users\n";
	  }
	}
      }
      return PreservedAnalyses::all();
    }
  };
}

#endif
