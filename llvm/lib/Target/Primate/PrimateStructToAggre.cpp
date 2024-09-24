#include "PrimateStructToAggre.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/User.h" 
#include "llvm/IR/IntrinsicInst.h"

#include <fstream>
#include <algorithm> 
#include <cctype>
#include <locale>

#define DEBUG_TYPE "PrimateStructToAggre"

using namespace llvm;

// trim from start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

namespace llvm {

    bool PrimateStructToAggre::isBFUType(Type* ty) {
        if(ty->isVoidTy()) {
            return true;
        }
        if(ty->isSingleValueType()) {
            return true;
        }
        if(StructType* STy = dyn_cast<StructType>(ty)) {
            return TLI->supportedAggregate(*STy);
        }
        return false;
    }

    void PrimateStructToAggre::findBFUTypes(Module& M) {
    }

    PreservedAnalyses PrimateStructToAggre::run(Function& F, FunctionAnalysisManager& PA) {
        BFUTypes = PA.getResult<PrimateBFUTypeFinding>(F);
        dbgs() << "Found " << BFUTypes.size() << " unique BFU types\n";
        for(Type* type: BFUTypes) {
            type->dump();
        }

        TLI = TM.getSubtarget<PrimateSubtarget>(F).getTargetLowering();
        // first normalize all the function calls to the same form 
        // 1. revert all vectorized aggregates to structs
        LLVM_DEBUG(dbgs() << "looking for struct allocas in func: " << F.getName() << "\n");
        SmallVector<AllocaInst*> workList;
        SmallVector<CallInst*> callWorklist;
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
                if(!isBFUType(ai->getAllocatedType())) {
                    continue;
                }
                LLVM_DEBUG(ai->dump());
                LLVM_DEBUG(dbgs() << "users: ");
                for(auto uIter: ai->users()) {
                    User& u = *uIter;
                    LLVM_DEBUG(u.dump());
                    if(auto* intrinInst = dyn_cast<IntrinsicInst>(&u)) {
                        MDNode* priTop = intrinInst->getCalledFunction()->getMetadata("primate");
                        if(!(priTop && dyn_cast<MDString>(priTop->getOperand(0))->getString() == "blue")) {
                            continue;
                        }
                        LLVM_DEBUG(dbgs() << "primate intrinsic {\n");
                        //find the users
                        for (auto* users: intrinInst->users()) {
                            LLVM_DEBUG(dbgs() << "user: ");
                        }
                    }
                    else if(auto* ci = dyn_cast<CallInst>(&u)) {
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
        }

        for(auto bleh: instructionsToRemove) {
            bleh->eraseFromParent();
        }

        F.dump();

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
            if(isBFUType(ai->getAllocatedType())) {
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
            else if(GetElementPtrInst* gepI = dyn_cast<GetElementPtrInst>(curInst)) {
                curInst = gepI->getPointerOperand(); 
            }
            else {
                curInst->dump();
                llvm_unreachable("can't follow a pointer...");
            }
        }
    }

    // remove all structs that are passed by pointer!
    void PrimateStructToAggre::convertCall(CallInst *ci, AllocaInst *ai) {
        // if we already fixed this then we move on
        dbgs() << "converting call: "; ci->dump();
        if(fixedCalls.find(ci) != fixedCalls.end()) {
            return;
        }
        // if its an instrinsic assume that it is done correctly already.
        if(ci->getCalledFunction()->isIntrinsic()) {
            return;
        }
        // look for struct ret
        Function* func = ci->getCalledFunction();

        bool needsReplace = false;
        Type* retType = ci->getType();
        if(ci->hasStructRetAttr()) {
            // find where the pointer came from
            retType = followPointerForType(ci->getArgOperand(0));
            if(isBFUType(retType)) {
                needsReplace = true;
            }
            else { 
                retType = ci->getType();
            }
        }

        SmallVector<Type*> argTypes;
        SmallVector<Value*> args;
        IRBuilder<> builder(ci);
        auto funcArg = func->arg_begin();
        dbgs() << "checking args\n";
        for(auto arg = ci->arg_begin(); arg != ci->arg_end(); arg++, funcArg++) {
            arg->get()->dump();
            if(funcArg->hasReturnedAttr() || funcArg->hasStructRetAttr()) {
                // returns have been handled above
                continue;
            }
            if(arg->get()->getType()->isPointerTy()) {
                dbgs() << "ptr: ";
                Type* ptrTy = followPointerForType(arg->get());
                ptrTy->dump();
                if(isBFUType(ptrTy)) {
                    dbgs() << "BFU Type.";
                    ci->dump();
                    ptrTy->dump();
                    LoadInst* newLoad = builder.CreateLoad(ptrTy, arg->get());
                    argTypes.push_back(ptrTy);
                    args.push_back(newLoad);
                    needsReplace = true;
                }
                else {
                    dbgs() << "scalar.";
                    argTypes.push_back(arg->get()->getType());
                    args.push_back(arg->get());
                }
                dbgs() << "\n";
            }
            else {
                dbgs() << "not ptr\n";
                argTypes.push_back(arg->get()->getType());
                args.push_back(arg->get());
            }
        }
        if(!needsReplace) {
            dbgs() << "Needs no replacement!\n";
            return;
        }
        dbgs() << "attempting to replace...\n";

        LLVM_DEBUG(dbgs() << "Return type: ");
        LLVM_DEBUG(retType->dump());
        LLVM_DEBUG(dbgs() << "Argument types: ");
        LLVM_DEBUG(for(auto* ty: argTypes){ty->dump();});

        Function *newFunc = nullptr;
        if(replacedFunctions.find(func) == replacedFunctions.end()) {
            FunctionType *FT = FunctionType::get(retType, argTypes, func->isVarArg());
            newFunc = Function::Create(FT, func->getLinkage(), func->getName(), func->getParent());
            newFunc->setMetadata("primate", func->getMetadata("primate"));
            //newFunc->setDSOLocal(true);
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

    Type* PrimateStructToAggre::getGEPTargetType(User::op_iterator ind_begin, User::op_iterator ind_end, Type* ptr_type) {
        if(auto* vty = dyn_cast<VectorType>(ptr_type)) {
            return vty->getElementType();
        }
        else if(auto* aty = dyn_cast<ArrayType>(ptr_type)) {
            return aty->getElementType();
        }
        else if(auto* sty = dyn_cast<StructType>(ptr_type)) {
            unsigned int const_idx = 0;
            if (ConstantInt* CI = dyn_cast<ConstantInt>(ind_begin->get())) {
                if (CI->getBitWidth() <= 32) {
                    const_idx = CI->getSExtValue();
                }
            }
            else {
                LLVM_DEBUG(dbgs() << "failed to find the type due to non-const ptr\n";);
                return nullptr;
            }
            if (std::next(ind_begin) == ind_end) {
                return sty->getTypeAtIndex(const_idx);
            }
            else {
                return getGEPTargetType(std::next(ind_begin), ind_end, sty->getTypeAtIndex(const_idx));
            }
        }
        else if(auto* ity = dyn_cast<IntegerType>(ptr_type)) {
            return ptr_type;
        }
        else {
            LLVM_DEBUG(dbgs() << "GEP is not targetting a vec, arr, or struct. weird.\n"; );
            return nullptr;
        }
    }

    // given some gep using an alloca, attempt to change all the load and stores using this
    // GEP into load, store, insert, extract instructions
    void PrimateStructToAggre::convertAndTrimGEP(GetElementPtrInst* gepI) {
        if(gepI->idx_begin() == gepI->idx_end()) { // is this possible?
            return;
        }

        IRBuilder<> builder(gepI->getParent());
        builder.SetInsertPoint(gepI);
        Value* srcPtr = gepI->getPointerOperand();
        Type* srcType = followPointerForType(srcPtr); // GEP type might not match the thing pointed to. Grab the alloca
        if(srcType != gepI->getSourceElementType()) {
            LLVM_DEBUG(dbgs() << "Gep type and pointer type are not the same. Can't generate insert/extracts so we bail.\n";);
            return;
        }

        auto b = std::next(gepI->idx_begin());
        auto e = gepI->idx_end();
        Type* finalType = getGEPTargetType(b, e, gepI->getSourceElementType()); // type of the final thing the GEP is calculating an address for. Sometimes the GEP will toss a 0 index.
        SmallVector<Use*> gepInds;
        unsigned int lastIndex;
        if(finalType == nullptr) {
            return;
        }
        for(auto& ind : gepI->indices()) {
            gepInds.push_back(&ind);
        }
        if (ConstantInt* CI = dyn_cast<ConstantInt>((gepInds.back()--))) {
            if(CI->getBitWidth() <= 32) {
                lastIndex = CI->getSExtValue();
            }
        }
        else {
            return;
        }


        for(auto uIter: gepI->users()) {
            if(auto* li = dyn_cast<LoadInst>(uIter)) {
                // bail if the loaded value type is not the same as the final type
                if (li->getType() != finalType) {
                    return;
                }
                builder.SetInsertPoint(li);
                LoadInst* newLoad = builder.CreateLoad(srcType, srcPtr);
                Value* extLoad    = builder.CreateExtractValue(newLoad, lastIndex, li->getName());
                LLVM_DEBUG(dbgs() << "replaced: "; li->dump(); 
                dbgs() << " with: "; extLoad->dump());
                li->replaceAllUsesWith(extLoad);
                LLVM_DEBUG(dbgs() << "Created Ops: ";
                newLoad->dump();
                extLoad->dump(););
                instructionsToRemove.push_back(li);
            }
            else if(auto* si = dyn_cast<StoreInst>(uIter)) {
                // bail if the stored value type is not the same as the final type
                if (si->getValueOperand()->getType() != finalType) {
                    return;
                }

                // TODO: HACK SHOULD BE OPT
                // load insert and store....
                LLVM_DEBUG(dbgs() << "Hit a store instr on the GEP users\n";);
                builder.SetInsertPoint(si);
                LoadInst *newLoad   = builder.CreateLoad(srcType, srcPtr);
                LLVM_DEBUG(newLoad->dump(););
                Value *insLoad      = builder.CreateInsertValue(newLoad, si->getValueOperand(), lastIndex);
                LLVM_DEBUG(insLoad->dump(););
                StoreInst *newStore = builder.CreateStore(insLoad, srcPtr);
                LLVM_DEBUG(newStore->dump(););
                instructionsToRemove.push_back(si);
            }
            else {
                LLVM_DEBUG(dbgs() << "Missed optimization due to gep being used by non load/store: "; uIter->dump();
                           dbgs() << "leaving the ops cleaned so far and bailing.\n";
                );
                return;
            }
        }
        instructionsToRemove.push_back(gepI);
    }
}
