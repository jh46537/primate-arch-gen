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

Type* KayvanfollowPointerForType(Value* start) {
    Value* curInst = start;
    while(true) {
        if(AllocaInst* allocaArg = dyn_cast<AllocaInst>(curInst)) {
            return allocaArg->getAllocatedType();
            break;
        }
        else if(BitCastInst* bci = dyn_cast<BitCastInst>(curInst)) {
            curInst = bci->getOperand(0);
        }
        else if(GetElementPtrInst* gepI = dyn_cast<GetElementPtrInst>(curInst)) {
            curInst = gepI->getPointerOperand(); 
        }
        else {
            curInst->dump();
            llvm_unreachable("can't follow a pointer...");
        }
    }
}

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
                            calledFunc->dump();
                            // StringRef BFU_name = dyn_cast<MDString>(attr->getOperand(1))->getString();
                            // TODO: Use the rest of the metadata in some way 
                            FunctionType* FT = calledFunc->getFunctionType();
                            if(calledFunc->hasStructRetAttr()) {
                                if(calledFunc->getParamStructRetType(0)->isAggregateType()) {
                                    BFUTypes.insert(calledFunc->getParamStructRetType(0));
                                    dbgs() << "Found Primate Type: ";
                                    calledFunc->getParamStructRetType(0)->dump();
                                }
                            }
                            else {
                                if(FT->getReturnType()->isAggregateType()) {
                                    dbgs() << "Found Primate Type: ";
                                    FT->getReturnType()->dump();
                                    BFUTypes.insert(FT->getReturnType());
                                }
                            }
                            dbgs() << "Checking the operands\n";
                            dbgs() << "getNumOperands(): " << ci->getNumOperands() << "\n";
                            // get param types. 
                            // first is ret. last is func called.
                            for(auto& a: ci->args()) {
                                if(a->getType()->isPointerTy()) {
                                    a->getType()->dump();
                                    dbgs() << "Found Primate Type: ";
                                    KayvanfollowPointerForType(a)->dump();
                                    BFUTypes.insert(KayvanfollowPointerForType(a));
                                }
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


