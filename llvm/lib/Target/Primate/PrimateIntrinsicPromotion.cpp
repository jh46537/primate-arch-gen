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

void PrimateIntrinsicPromotion::promoteReturnType(std::vector<CallInst*>& worklist) {
    SmallVector<Value*, 8> instructionsToRemove;

    for (CallInst* ci: worklist) {
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
