#include "PrimateIntrinsicPromotion.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/User.h" 
#include "llvm/IR/IntrinsicInst.h"

#define DEBUG_TYPE "PrimateIntrinsicPromotion"

namespace llvm {


PreservedAnalyses PrimateIntrinsicPromotion::run(Function& F, FunctionAnalysisManager& FAM) {
    LLVM_DEBUG(dbgs() << "PrimateIntrinsicPromotion\n");

    std::vector<CallInst*> worklist;

    for (auto& bb: F) {
        for(auto &instr: bb) {
            if (auto* call = dyn_cast<CallInst>(&instr)) {
                MDNode* priTop = call->getCalledFunction()->getMetadata("primate");
                if(!(priTop && dyn_cast<MDString>(priTop->getOperand(0))->getString() == "blue")) {
                    LLVM_DEBUG(dbgs() << "Call is not a primate BFU call: "; call->dump(););
                    continue;
                }

                if(!call->getType()->isPointerTy()) {
                    LLVM_DEBUG(dbgs() << "Call returns a value already: "; call->dump(););
                    continue;
                }

                worklist.push_back(call);
            }
        }
    }

    promoteReturnType(worklist);

    // promote the args
    worklist.clear();

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

void PrimateIntrinsicPromotion::promoteArgs(std::vector<CallInst*>& worklist) {
    SmallVector<Value*, 8> instructionsToRemove;
    
    for (CallInst* ci: worklist) {
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
    }
    for(auto* instr: instructionsToRemove) {
        dyn_cast<Instruction>(instr)->eraseFromParent();
    }
}

void PrimateIntrinsicPromotion::promoteReturnType(std::vector<CallInst*>& worklist) {
    SmallVector<Value*, 8> instructionsToRemove;

    for (CallInst* ci: worklist) {
        LLVM_DEBUG(dbgs() << "Promoting call result: "; ci->dump(););

        // only promote calls that are memcpy'd
        // these should have only one use (the memcpy....)
        if(!ci->hasOneUse()) {
            LLVM_DEBUG(dbgs() << "Call result is reused bailing on promotion... : "; ci->dump(););
            continue;
        }

        // get dest alloca 
        Value* destAlloca = nullptr;
        for(auto* user: ci->users()) {
            destAlloca = dyn_cast<Value>(user);
            // follow pointer though stores and memcpys
            if(auto* memcpy = dyn_cast<MemCpyInst>(destAlloca)) {
                instructionsToRemove.push_back(memcpy);
                instructionsToRemove.push_back(ci);
                destAlloca = memcpy->getDest();
                destAlloca = dyn_cast<AllocaInst>(destAlloca);
            }
            else {
                destAlloca = nullptr;
                break;
            }
        }

        if(destAlloca == nullptr) {
            LLVM_DEBUG(dbgs() << "Call returns a pointer that is not memcpy'd. bailing on promotion: "; ci->dump(););
            continue;
        }

        // create the new function which returns SSA
        Type* retType = dyn_cast<AllocaInst>(destAlloca)->getAllocatedType();
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
        builder.CreateStore(newCall, destAlloca);
    }
    for(auto* instr: instructionsToRemove) {
        dyn_cast<Instruction>(instr)->eraseFromParent();
    }
}

}