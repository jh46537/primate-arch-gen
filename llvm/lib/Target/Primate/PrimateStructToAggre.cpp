#include "PrimateStructToAggre.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsPrimate.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "PrimateStructToAggre"


namespace llvm {

    PreservedAnalyses PrimateStructToAggre::run(Module& M, ModuleAnalysisManager& PA) {
        for(auto& F: M) {
            // first normalize all the function calls to the same form 
            // 1. revert all vectorized aggregates to structs
            LLVM_DEBUG(dbgs() << "looking for struct allocas in func: " << F.getName() << "\n");
            SmallVector<AllocaInst*> workList;
            for(auto& bb: F) {
                // find allocas
                for(auto& inst: bb) {
                    if (AllocaInst* ai = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
                        workList.push_back(ai);
                    }
                }

                // do work on allocas
                for(auto* ai: workList) {
                    // normal allocas can be left
                    if(!ai->getAllocatedType()->isAggregateType()) {
                        continue;
                    }
                    LLVM_DEBUG(ai->dump());
                    LLVM_DEBUG(dbgs() << "users: ");
                    for(auto uIter: ai->users()) {
                        User& u = *uIter;
                        LLVM_DEBUG(dbgs() << "looking at instr: ");
                        LLVM_DEBUG(u.dump());
                        if(auto* ci = dyn_cast<CallInst>(&u)) {
                            LLVM_DEBUG(dbgs() << "call {\n");
                            convertCall(ci, ai);
                        }
                        else if(auto* gepi = dyn_cast<GetElementPtrInst>(&u)) {
                            LLVM_DEBUG(dbgs() << "GEP{\n");
                            convertAndTrimGEP(gepi);
                        }
                        else {
                            LLVM_DEBUG(dbgs() << "other{\n");
                        }
                        LLVM_DEBUG(dbgs() << "}\n");
                    }
                    LLVM_DEBUG(dbgs() << "---\n");
                }
                workList.clear();
                for(auto* inst: instructionsToRemove) {
                    LLVM_DEBUG(dbgs() << "Removing instr: ");
                    LLVM_DEBUG(inst->dump());
                    inst->eraseFromParent();
                }
                instructionsToRemove.clear();

                for(auto& inst: bb) {
                    if (CallInst* ci = dyn_cast<CallInst>(&inst)) {
                        LLVM_DEBUG(
                        dbgs() << "found a call to " << 
                                  ci->getName() << " or " << 
                                  ci->getCalledFunction()->getName() << 
                                  "\n"
                        );
                    }
                }

            }

            removeAllocas(F);
        }
        return PreservedAnalyses::none();
    }

    void PrimateStructToAggre::removeAllocas(Function& F) {
        LLVM_DEBUG(dbgs() << "remove mem ops for " << F.getName() << "\n";);
        // look for all the calls in the function
        SmallVector<AllocaInst*> worklist;
        for(auto& bb: F) {
            for(auto& inst: bb) {
                if(AllocaInst* ai = dyn_cast<AllocaInst>(&inst)) {
                    worklist.push_back(ai);
                }
            }
        }

        for(AllocaInst* ai: worklist) {
            if(ai->getAllocatedType()->isAggregateType()) {
                for(Value* consumer: ai->users()) {
                    LLVM_DEBUG(consumer->dump(););
                    if(LoadInst* li = dyn_cast<LoadInst>(consumer)) {

                    }
                    else if(StoreInst* si = dyn_cast<StoreInst>(consumer))
                    {

                    }
                    else if(BitCastInst* bci = dyn_cast<BitCastInst>(consumer)) {

                    }
                    else {
                        dbgs() << "Alloca: ";
                        ai->dump();
                        dbgs() << "Bad consumer: ";
                        consumer->dump();
                        dbgs() << "Function ";
                        F.dump();
                        llvm_unreachable("Function is not clean when trying to remove allocas :(");
                    }
                }
            }
        }
    }

    Type* PrimateStructToAggre::followPointerForType(Value* start) {
        Value* curInst = start;
        while(true) {
            if(AllocaInst* allocaArg = dyn_cast<AllocaInst>(curInst)) {
                return allocaArg->getAllocatedType();
                break;
            }
            else if(BitCastInst* bci = dyn_cast<BitCastInst>(curInst)) {
                curInst = bci->getOperand(0);
            }
            else {
                llvm_unreachable("can't follow a pointer...");
            }
        }
    }

    // remove all structs that are passed by pointer!
    void PrimateStructToAggre::convertCall(CallInst *ci, AllocaInst *ai) {
        // if we already fixed this then we move on
        if(fixedCalls.find(ci) != fixedCalls.end()) {
            return;
        }
        // look for struct ret
        Function* func = ci->getCalledFunction();

        Type* retType = nullptr;
        if(ci->hasStructRetAttr()) {
            // find where the pointer came from
            retType = followPointerForType(ci->getArgOperand(0));
        }
        else {
            retType = ci->getType();
        }

        SmallVector<Type*> argTypes;
        SmallVector<Value*> args;
        IRBuilder<> builder(ci);
        auto funcArg = func->arg_begin();
        for(auto arg = ci->arg_begin(); arg != ci->arg_end(); arg++, funcArg++) {
            if(funcArg->hasReturnedAttr() || funcArg->hasStructRetAttr()) {
                // returns have been handled above
                continue;
            }
            if(arg->get()->getType()->isPointerTy()) {
                Type* ptrTy = followPointerForType(arg->get());
                LoadInst* newLoad = builder.CreateLoad(ptrTy, arg->get());
                argTypes.push_back(ptrTy);
                args.push_back(newLoad);
            }
            else {
                args.push_back(arg->get());
                argTypes.push_back(arg->get()->getType());
            }
        }

        LLVM_DEBUG(dbgs() << "Return type: ");
        LLVM_DEBUG(retType->dump());
        LLVM_DEBUG(dbgs() << "Argument types: ");
        LLVM_DEBUG(for(auto* ty: argTypes){ty->dump();});

        Function *newFunc = nullptr;
        if(replacedFunctions.find(func) == replacedFunctions.end()) {
            FunctionType *FT = FunctionType::get(retType, argTypes, func->isVarArg());
            newFunc = Function::Create(FT, func->getLinkage(), func->getName(), func->getParent());
            replacedFunctions[func] = newFunc;
        }
        else {
            newFunc = replacedFunctions[func];
        }

        CallInst* newCall = builder.CreateCall(newFunc, args);
        fixedCalls.insert(ci);
        LLVM_DEBUG(dbgs() << "Created Call: ");
        LLVM_DEBUG(newCall->dump());

        if(func->hasStructRetAttr()) {
            StoreInst* stInst = builder.CreateStore(newCall, ci->getArgOperand(0));
            LLVM_DEBUG(dbgs() << "Creating Store: ");
            LLVM_DEBUG(stInst->dump());
        }
        else {
            ci->replaceAllUsesWith(newCall);
        }


        instructionsToRemove.push_back(ci);
    }

    void PrimateStructToAggre::convertAndTrimGEP(GetElementPtrInst* gepI) {
        IRBuilder<> builder(gepI->getParent());
        builder.SetInsertPoint(gepI);
        Value* srcPtr = gepI->getPointerOperand();
        Type* srcType = gepI->getSourceElementType();
        SmallVector<Use*> gepInds;
        for(auto& ind : gepI->indices()) {
            gepInds.push_back(&ind);
        }
        unsigned int lastIndex;
        if (ConstantInt* CI = dyn_cast<ConstantInt>((gepInds.back()--))) {
            if (CI->getBitWidth() <= 32) {
                lastIndex = CI->getSExtValue();
            }
        }
        else {
            llvm_unreachable("no support for variable last index GEPs");
        }


        for(auto uIter: gepI->users()) {
            if(auto* li = dyn_cast<LoadInst>(uIter)) {
                LoadInst* newLoad = builder.CreateLoad(srcType, srcPtr);
                Value* extLoad = builder.CreateExtractValue(newLoad, lastIndex, li->getName());
                li->replaceAllUsesWith(extLoad);
                instructionsToRemove.push_back(li);
            }
            else {
                llvm_unreachable("gep used by not a load");
            }
        }
        instructionsToRemove.push_back(gepI);
    }
}