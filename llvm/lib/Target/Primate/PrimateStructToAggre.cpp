#include "PrimateStructToAggre.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsPrimate.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "PrimateStructToAggre"


namespace llvm {
    void PrimateStructToAggre::normalizeFuncs(Function& F) {
        LLVM_DEBUG(dbgs() << "normalizing function calls for " << F.getName() << "\n");
        // look for all the calls in the function
        SmallVector<CallInst*> worklist;
        for(auto& bb: F) {
            for(auto& inst: bb) {
                if(CallInst* ci = dyn_cast<CallInst>(&inst)) {
                    worklist.push_back(ci);
                }
            }
        }

        // calls that use vector types should check if those came from a struct originally.
        for(CallInst* ci: worklist) {
            if(ci->isDebugOrPseudoInst()) {
                continue;
            }
            LLVM_DEBUG(dbgs() << "-------------------\nLooking at call inst:");
            LLVM_DEBUG(ci->dump());
            if(ci->hasStructRetAttr()) {
                LLVM_DEBUG(dbgs() << "call inst already returns a struct via sret!");
                LLVM_DEBUG(ci->dump());
                continue;
            }
            IRBuilder<> builder(ci);
            for(auto* user: ci->users()) {
                // if the user is a store then we may have a struct acc 
                SmallVector<Value*> args;
                SmallVector<Type*> argsType;
                Type* retType = nullptr; 
                if(StoreInst* si = dyn_cast<StoreInst>(user)) {
                    LLVM_DEBUG(dbgs() << "ran into store inst: ");
                    LLVM_DEBUG(user->dump());
                    // check the pointer
                    // pointer is either an alloca, or a function arg
                    Value* dstPtr = si->getPointerOperand();
                    if(BitCastInst* bci = dyn_cast<BitCastInst>(dstPtr)) {
                        // bitcast yay!
                        LLVM_DEBUG(dbgs() << "ran into a bitcast:");
                        LLVM_DEBUG(bci->dump());
                        LLVM_DEBUG(dbgs() << " using as type: ");
                        retType = dyn_cast<PointerType>(bci->getSrcTy())->getPointerElementType();
                        LLVM_DEBUG(retType->dump());
                        Value* originalPointer = bci->getOperand(0);  

                        for(auto& arg: ci->args()) {
                            argsType.push_back(arg.get()->getType());
                            args.push_back(arg);
                        }
                        FunctionType* FT = FunctionType::get(retType, argsType, ci->getCalledFunction()->isVarArg());
                        Function* newFunc = Function::Create(FT, 
                                    ci->getCalledFunction()->getLinkage(), 
                                    ci->getCalledFunction()->getName(), 
                                    ci->getCalledFunction()->getParent());

                        Value* newCall = builder.CreateCall(newFunc, args);
                        Value* newStore = builder.CreateStore(newCall, originalPointer);

                        LLVM_DEBUG(dbgs() << "Created insts: ");
                        LLVM_DEBUG(newCall->dump());
                        LLVM_DEBUG(newStore->dump());
                        LLVM_DEBUG(dbgs() << "deleted insts:");
                        LLVM_DEBUG(bci->dump());
                        LLVM_DEBUG(ci->dump());
                        LLVM_DEBUG(si->dump());
                        instructionsToRemove.push_back(si);
                        instructionsToRemove.push_back(bci);
                        instructionsToRemove.push_back(ci);
                    }
                }
            }
        }

        for(auto* inst: instructionsToRemove) {
            inst->eraseFromParent();
        }
        instructionsToRemove.clear();
    }

    PreservedAnalyses PrimateStructToAggre::run(Module& M, ModuleAnalysisManager& PA) {
        for(auto& F: M) {
            // first normalize all the function calls to the same form 
            // 1. revert all vectorized aggregates to structs
            normalizeFuncs(F);
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

    void PrimateStructToAggre::convertCall(CallInst *ci, AllocaInst *ai) {
        // look for struct ret
        IRBuilder<> builder(ci->getParent());
        builder.SetInsertPoint(ci);

        Function* func = ci->getCalledFunction();
        Value* sretArg = nullptr;
        SmallVector<Type*> funcTypes;
        for(auto arg = func->arg_begin(); arg != func->arg_end(); arg++) {
            Argument* a = llvm::dyn_cast<Argument>(arg);
            if(!a) {
                LLVM_DEBUG(dbgs() << "not an argument...\n");
            }
            else if(a->hasStructRetAttr()) {
                int argIdx = arg->getArgNo();
                sretArg = ci->getArgOperand(argIdx);
            }
            else {
                funcTypes.push_back(a->getType());
            }
        }

        if (sretArg) {
            // if the function is returing to the alloca 
            // we need to remove it and then fix that up
            if(sretArg == ai) {
                LLVM_DEBUG(dbgs() << "returned to THIS alloca\n";);
                auto funcExists = replacedFunctions.find(func);
                if(funcExists == replacedFunctions.end()) {
                    LLVM_DEBUG(dbgs() << "adding a new function!\n";);
                    Type* retType = ai->getAllocatedType();                
                    FunctionType *FT = FunctionType::get(retType, funcTypes, func->isVarArg());
                    Function* newFunc = Function::Create(FT, func->getLinkage(), func->getName(), func->getParent());
                    replacedFunctions[func] = newFunc;
                }

                Function* newFunc = replacedFunctions.at(func);
                std::vector<Value *> args;
                auto farg = func->arg_begin();
                for(auto arg = ci->arg_begin(); arg != ci->arg_end(); arg++, farg++) {
                    Argument* a = llvm::dyn_cast<Argument>(farg);
                    if(!a) {
                        LLVM_DEBUG(dbgs() << "not an argument...\n");
                    }
                    else if(!a->hasStructRetAttr()) {
                        LLVM_DEBUG({dbgs() << "adding as a param: ";
                        arg->get()->dump();});
                        args.push_back(arg->get());
                    }
                }
                assert(args.size() + 1 == func->arg_size() && "unexpected number of arguments when collecting");
                Value* newCall = builder.CreateCall(newFunc, args);
                Value* newStore = builder.CreateStore(newCall, ai);
                LLVM_DEBUG(dbgs() << "created instructions: ");
                LLVM_DEBUG(newCall->dump());
                LLVM_DEBUG(newStore->dump());
                instructionsToRemove.push_back(ci);
            }
            else {
                LLVM_DEBUG(sretArg->dump());
                LLVM_DEBUG(dbgs() << "return to different alloca");
            }
        } 
        else {
            LLVM_DEBUG(dbgs() << "uses an alloca");
            // if we use an alloca ptr we should just load the alloca and then use the ssa val
        }
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