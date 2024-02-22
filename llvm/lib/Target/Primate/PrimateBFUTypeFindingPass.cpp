//===-- PrimatePasses.h - Primate Middle End Passes --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
/// \file
//===----------------------------------------------------------------------===//

#include "PrimateBFUTypeFindingPass.h"

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

AnalysisKey PrimateBFUTypeFinding::Key;

PrimateBFUTypeFinding::Result PrimateBFUTypeFinding::run(Module& M, ModuleAnalysisManager& PA) {

    for(auto& F: M){
        for(auto& BB: F) {
            for(auto& inst: BB) {
                if(CallInst* ci = dyn_cast<CallInst>(&inst)) {
                    Function* calledFunc = ci->getCalledFunction();
                    MDNode *attr = calledFunc->getMetadata("primate");
                    if(attr) {
                        dbgs() << "Found Primate MD: ";
                        attr->dump();
                        // lol 
                        if( dyn_cast<MDString>(attr->getOperand(0))->getString() == "blue") {
                            dbgs() << "Found a BFU Func\n";
                            // StringRef BFU_name = dyn_cast<MDString>(attr->getOperand(1))->getString();
                            // TODO: Use the rest of the metadata in some way 
                            FunctionType* FT = calledFunc->getFunctionType();
                            if(calledFunc->hasStructRetAttr()) {
                                BFUTypes.insert(calledFunc->getParamStructRetType(0));
                                dbgs() << "Found Primate Type: ";
                                calledFunc->getParamStructRetType(0)->dump();
                            }
                            else {
                                dbgs() << "Found Primate Type: ";
                                FT->getReturnType()->dump();
                                BFUTypes.insert(FT->getReturnType());
                            }

                            // get param types. 
                            for(unsigned i = 0; i < calledFunc->getNumOperands(); i++) {
                                dbgs() << "Found Primate Type: ";
                                calledFunc->getOperand(i)->getType()->dump();
                                BFUTypes.insert(calledFunc->getOperand(i)->getType());
                            }
                        }
                    }
                }
            }
        }
    }
    // might get a void from a BFU that only consumes things. 
    BFUTypes.erase(Type::getVoidTy(M.getContext()));

    return BFUTypes;
}

}


