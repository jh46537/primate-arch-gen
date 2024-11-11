#include "PrimateIntrinsicPromotion.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/User.h" 
#include "llvm/IR/IntrinsicInst.h"

#define DEBUG_TYPE "PrimateIntrinsicPromotion"

namespace llvm {

cl::opt<bool> PromoteIntrinsics("pri-intrins-promote", cl::desc("toggle for intrinsic type promotion for debug"), cl::init(true));
  
PreservedAnalyses PrimateIntrinsicPromotion::run(Function& F, FunctionAnalysisManager& FAM) {
    LLVM_DEBUG(dbgs() << "PrimateIntrinsicPromotion\n");
    if(!PromoteIntrinsics) {
      return PreservedAnalyses::all();
    }

    std::vector<CallInst*> worklist;

    for (auto& bb: F) {
        for(auto &instr: bb) {
            if (auto* call = dyn_cast<CallInst>(&instr)) {
                MDNode* priTop = call->getCalledFunction()->getMetadata("primate");
                if(!(priTop && dyn_cast<MDString>(priTop->getOperand(0))->getString() == "blue")) {
                    LLVM_DEBUG(dbgs() << "Call is not a primate BFU call: "; call->dump(););
                    continue;
                }

                if(!(call->getType()->isPointerTy() || call->getCalledFunction()->hasStructRetAttr())) {
                    LLVM_DEBUG(dbgs() << "Call returns a value already: "; call->dump(););
                    continue;
                }

                worklist.push_back(call);
            }
        }
    }

    promoteReturnType(worklist);
    worklist.clear();

    // convert normal ops into intrinsic ops (if needed)
    for (auto& bb: F) {
        for(auto &instr: bb) {
            if (auto* call = dyn_cast<CallInst>(&instr)) {
                MDNode* priTop = call->getCalledFunction()->getMetadata("primate");
                if(!(priTop && dyn_cast<MDString>(priTop->getOperand(0))->getString() == "blue")) {
                    LLVM_DEBUG(dbgs() << "Call is not a primate BFU call: "; call->dump(););
                    continue;
                }
                if(isa<IntrinsicInst>(call)) {
                    LLVM_DEBUG(dbgs() << "Call is already an intrinsic: "; call->dump(););
                    continue;
                }
                worklist.push_back(call);
            }
        }
    }
    convertToIntrinsic(worklist);

    worklist.clear();

    // promote the args
    for (auto& bb: F) {
        for(auto &instr: bb) {
            if (auto* call = dyn_cast<CallInst>(&instr)) {
                MDNode* priTop = call->getCalledFunction()->getMetadata("primate");
                if(!(priTop && dyn_cast<MDString>(priTop->getOperand(0))->getString() == "blue")) {
                    LLVM_DEBUG(dbgs() << "Call is not a primate BFU call: "; call->dump(););
                    continue;
                }
                worklist.push_back(call);
            }
        }
    }

    promoteArgs(worklist);

    return PreservedAnalyses::none();
}

void PrimateIntrinsicPromotion::convertToIntrinsic(std::vector<CallInst*>& worklist) {
    for (CallInst* ci: worklist) {
        auto* calledFunc = ci->getCalledFunction();
        IRBuilder<> builder(ci);
        LLVM_DEBUG(dbgs() << "Converting call to intrinsic: "; ci->dump(););
        SmallVector<Value*, 3> args;
        SmallVector<Type*, 3> argTypes;

        if (!ci->getType()->isVoidTy()) {
            argTypes.push_back(ci->getType());
        }

        for(auto& arg: ci->args()) {
            args.push_back(arg);
            argTypes.push_back(arg->getType());
        }

        MDNode* priTop = calledFunc->getMetadata("primate");
        std::string bfuUnitName = dyn_cast<MDString>(priTop->getOperand(1))->getString().str();
        std::string bfuInstructionName = dyn_cast<MDString>(priTop->getOperand(2))->getString().str();
        LLVM_DEBUG(dbgs() << "Looking for intrinsic named llvm.primate.BFU." << bfuUnitName << "." << bfuInstructionName << "\n"; );
        auto intrinsicID = (llvm::Intrinsic::PRIMATEIntrinsics)ci->getParent()->getParent()->lookupIntrinsicID("llvm.primate.BFU." + bfuUnitName + "." + bfuInstructionName);
        Function* newFunc = llvm::Intrinsic::getDeclaration(ci->getCalledFunction()->getParent(), intrinsicID, argTypes);
        newFunc->setMetadata("primate", ci->getCalledFunction()->getMetadata("primate"));

        auto* newCall = builder.CreateCall(newFunc, args);
        ci->replaceAllUsesWith(newCall);
        ci->eraseFromParent();
    }
}
  
void PrimateIntrinsicPromotion::promoteArgs(std::vector<CallInst*>& worklist) {
    SmallVector<Value*, 8> instructionsToRemove;
    SmallVector<Function*, 8> functionsToRemove;
    
    for (CallInst* ci: worklist) {
      	if(ci->getCalledFunction()->hasNUses(0)) {
           functionsToRemove.push_back(ci->getCalledFunction());
        }
        IRBuilder<> builder(ci);
        LLVM_DEBUG(dbgs() << "Promoting call arguments: "; ci->dump(););
        
        // all things should be pulled up to ssa values
        SmallVector<Value*, 3> args;
        SmallVector<Type*, 3> argTypes;
        if(!ci->getType()->isVoidTy()) {
            argTypes.push_back(ci->getType());
        }
        bool replaceNeeded = false;
        for(auto& arg: ci->args()) {
            if(arg->getType()->isPointerTy()) {
                // pointer should come from an alloca
                if(auto* ai = dyn_cast<AllocaInst>(arg)) {
                    argTypes.push_back(ai->getAllocatedType());
                    args.push_back(builder.CreateLoad(ai->getAllocatedType(), ai));
                }
                else {
                    LLVM_DEBUG(dbgs() << "Call arg is not an alloca. bailing on promotion: "; ci->dump(); arg->dump(););
                    break;
                }
                replaceNeeded = true;
            }
            else {
                args.push_back(arg);
                argTypes.push_back(arg->getType());
            }
        }
        if(!replaceNeeded) {
            continue;
        }

        Function* newFunc = llvm::Intrinsic::getDeclaration(ci->getCalledFunction()->getParent(), dyn_cast<IntrinsicInst>(ci)->getIntrinsicID(), argTypes);
        newFunc->setMetadata("primate", ci->getCalledFunction()->getMetadata("primate"));
        auto* newCall = builder.CreateCall(newFunc, args);
        ci->replaceAllUsesWith(newCall);
        instructionsToRemove.push_back(ci);
        if(ci->getCalledFunction()->hasNUses(0)) {
        functionsToRemove.push_back(ci->getCalledFunction());
        }
    }
    for(auto* instr: instructionsToRemove) {
        dyn_cast<Instruction>(instr)->eraseFromParent();
    }
    for(auto* instr: functionsToRemove) {
        instr->eraseFromParent();
    }
}

  // 1 grab the sret pointer
  // 2 check if function is already replaced.
  // 3 convert the function to something that returns the value
  // 4 replace all calls to old with new
  // 5 store the returned value into the old sret pointer
void PrimateIntrinsicPromotion::promoteSRET(CallInst* ci) {
    auto* calledFunc = ci->getCalledFunction();

    // 1
    // sret always either 0 or 1
    int argStartIdx = 1;
    auto* returnType = calledFunc->getParamStructRetType(0);
    auto* returnDestPtr  = ci->getArgOperand(0);
    if(!returnType) {
        returnType = calledFunc->getParamStructRetType(1);
        returnDestPtr  = ci->getArgOperand(1);
        argStartIdx = 2;
    }
  
    // collect other args
    SmallVector<Type*, 3> argTypes = {returnType};
    SmallVector<Value*, 3> args;
    for(auto& arg: ci->args()) {
        if(argStartIdx) {
            argStartIdx--;
            continue;
        }
        argTypes.push_back(arg->getType());
        args.push_back(arg);
    }


    // 2
    if(replacedFunctions.find(calledFunc) != replacedFunctions.end()) {
        auto* newFunc = replacedFunctions[calledFunc];
        // create the new call
        IRBuilder<> builder(ci);
        auto* newCall = builder.CreateCall(newFunc, args);
        builder.CreateStore(newCall, returnDestPtr);
        ci->eraseFromParent();
        return;
    }
    
    // 3
    MDNode* priTop = calledFunc->getMetadata("primate");
    std::string bfuUnitName = dyn_cast<MDString>(priTop->getOperand(1))->getString().str();
    std::string bfuInstructionName = dyn_cast<MDString>(priTop->getOperand(2))->getString().str();
    LLVM_DEBUG(dbgs() << "Looking for intrinsic named llvm.primate.BFU." << bfuUnitName << "." << bfuInstructionName << "\n"; );
    auto intrinsicID = (llvm::Intrinsic::PRIMATEIntrinsics)ci->getParent()->getParent()->lookupIntrinsicID("llvm.primate.BFU." + bfuUnitName + "." + bfuInstructionName);
    Function* newFunc = llvm::Intrinsic::getDeclaration(ci->getCalledFunction()->getParent(), intrinsicID, argTypes);
    newFunc->setMetadata("primate", ci->getCalledFunction()->getMetadata("primate"));

    // 4
    IRBuilder<> builder(ci);
    auto* newCall = builder.CreateCall(newFunc, args);
    builder.CreateStore(newCall, returnDestPtr);
    ci->eraseFromParent();

    replacedFunctions[calledFunc] = newFunc;
}

void PrimateIntrinsicPromotion::promoteReturnType(std::vector<CallInst*>& worklist) {
    SmallVector<Value*, 8> instructionsToRemove;

    for (CallInst* ci: worklist) {
        if(ci->getCalledFunction()->hasStructRetAttr()) {
            promoteSRET(ci);
            continue;
        }
        SmallVector<Value*, 2> newInstrsToRemove;
        LLVM_DEBUG(dbgs() << "Promoting call result: "; ci->dump(););
        // only promote calls that are memcpy'd
        // these should have only one use (the memcpy....)
        if(!ci->hasOneUser()) {
            LLVM_DEBUG(dbgs() << "Call result is reused bailing on promotion... : "; ci->dump(););
            continue;
        }

        // get dest alloca 
        Value* destAlloca = nullptr;
	    Value* loadInstr  = nullptr;
        // find memcpy to find an alloca  for the return value
        for(auto* user: ci->users()) {
            if(destAlloca || loadInstr) {
                LLVM_DEBUG(dbgs() << "multiple users. Fail to promote\n";);
                destAlloca = nullptr;
                loadInstr = nullptr;
            }
            else if(auto* memcpy = dyn_cast<MemCpyInst>(user)) {
                newInstrsToRemove.push_back(memcpy);
                destAlloca = memcpy->getDest();
                destAlloca = dyn_cast<AllocaInst>(destAlloca);
                LLVM_DEBUG(dbgs() << "found a memcpy. Probably an alloca.\n";);
            }
            else if(auto* load = dyn_cast<LoadInst>(user)) {
                loadInstr = load;
                LLVM_DEBUG(dbgs() << "found a load.\n";);

                // this is checking for a fake memcpy due to small return type (primitives are load/store not memcpy)
                if(load->hasOneUser()) {
                    if(auto* store = dyn_cast<StoreInst>(load->user_back())) {
                        LLVM_DEBUG(dbgs() << "found a store.\n";);
                        Value* temp = store->getPointerOperand();
                        if(auto* ai = dyn_cast<AllocaInst>(temp)) {
                            LLVM_DEBUG(dbgs() << "found an alloca.\n";);
                            destAlloca = ai;
                        }
                        newInstrsToRemove.push_back(store);
                    }
                }
                newInstrsToRemove.push_back(load);
            }
            else {
                destAlloca = nullptr;
		        loadInstr  = nullptr;
                break;
            }
        }

        if(destAlloca == nullptr && loadInstr == nullptr) {
            LLVM_DEBUG(dbgs() << "Call returns a pointer that is reused. bailing on promotion: "; ci->dump(););
            continue;
        }

        // create the new function which returns SSA
        Type* retType = nullptr;
        if (destAlloca) {
            retType = dyn_cast<AllocaInst>(destAlloca)->getAllocatedType();
        }
        else if (loadInstr) {
            retType = dyn_cast<LoadInst>(loadInstr)->getType();
        }
        SmallVector<Type*, 3> argTypes = {retType};
        SmallVector<Value*, 3> args;
        for(auto& arg: ci->args()) {
            argTypes.push_back(arg->getType());
            args.push_back(arg);
        }
        Function* newFunc = llvm::Intrinsic::getDeclaration(ci->getCalledFunction()->getParent(), dyn_cast<IntrinsicInst>(ci)->getIntrinsicID(), argTypes);
        newFunc->setMetadata("primate", ci->getCalledFunction()->getMetadata("primate"));

        // create the new call
        IRBuilder<> builder(ci);
        auto* newCall = builder.CreateCall(newFunc, args);

        // store the result in the dest alloca
        if (destAlloca) {
            builder.CreateStore(newCall, destAlloca);
        } else if(loadInstr) {
            loadInstr->replaceAllUsesWith(newCall);
        }
        instructionsToRemove.insert(instructionsToRemove.end(), newInstrsToRemove.begin(), newInstrsToRemove.end());
        instructionsToRemove.push_back(ci);
    }
    LLVM_DEBUG(dbgs() << "Removing instructions\n";);
    for(auto* instr: instructionsToRemove) {
        LLVM_DEBUG(instr->dump(););
        if(!dyn_cast<Instruction>(instr)) {
            LLVM_DEBUG(dbgs() << "non-instruction????\n";);
            LLVM_DEBUG(instr->dump(););
        }
        dyn_cast<Instruction>(instr)->eraseFromParent();
    }
    instructionsToRemove.clear();
}

}
