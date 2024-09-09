#include "PrimateStructToAggre.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include <fstream>
#include <algorithm> 
#include <cctype>
#include <locale>

// TODO: EVIL EVIL EVIL. MOVE TO CLANG!!!!!!
#include "llvm/Demangle/Demangle.h"
// TODO: DONT USE FUNC NAME FOR GENERATING INTRINSICS!!!!


#define DEBUG_TYPE "PrimateStructToAggre"




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
        std::ifstream bfuMapping("./bfu_list.txt");
        int bfu_schedule_slot = 0;
        bool inscope = false;
        for (std::string line; std::getline(bfuMapping, line); ) {
            if(line == "{") {
                assert(!inscope);
                inscope = true;
            }
            else if(line == "}") {
                assert(inscope);
                inscope = false;
            }
            else if(inscope) {
                continue;
            }
            else {
                // assume we got a name
                rtrim(line);
                ltrim(line);
                dbgs() << "Found a BFU Named: " << line << "\n";
                nameToIntrins[line] = (llvm::Intrinsic::PRIMATEIntrinsics)(llvm::Intrinsic::primate_BFU_0 + (bfu_schedule_slot++));
                for (const auto& nameIntrinPair : nameToIntrins)
                    dbgs() << nameIntrinPair.first << "\n";
                }
        }

        BFUTypes = PA.getResult<PrimateBFUTypeFinding>(F);
        dbgs() << "Found " << BFUTypes.size() << " unique BFU types\n";
        for(Type* type: BFUTypes) {
            type->dump();
        }
	{
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

                // do the call conversion and cleaning on the remaining instructions
                for(auto& inst: bb) {
                    if(CallInst* ci = dyn_cast<llvm::CallInst>(&inst)) {
                        if(!ci->getCalledFunction()->isIntrinsic())
                            callWorklist.push_back(ci);
                    }
                }
                for(auto* ci: callWorklist) {
                    convertCall(ci, nullptr);
                }
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

            //removeAllocas(F);
        }

        // TODO: MOVE TO CLANG
        // TODO: if you see this in the future and hate the if else tree just know me too. its a hack. 
        {
            LLVM_DEBUG(dbgs() << "looking for intrinics in func: " << F.getName() << "\n");
            SmallVector<CallInst*> workList;
            for(auto& bb: F) {
                workList.clear();
                for(auto& inst: bb) {
                    if(!inst.isDebugOrPseudoInst()) {
                        if(CallInst *ci = dyn_cast<CallInst>(&inst)) {
                            workList.push_back(ci);
                        }
                    }
                }
                for(auto* inst: workList) {
                    // replace input inst with call intr
                    std::string demangledName = demangle(inst->getCalledFunction()->getName().str());
                    dbgs() << "demangled name: " << demangledName << "\n";
                    MDNode* priTop = inst->getCalledFunction()->getMetadata("primate");
                    if(priTop && dyn_cast<MDString>(priTop->getOperand(0))->getString() == "blue") {
                        if (!isBFUType(inst->getType())) {
                            llvm_unreachable("Calling a blue functional unit with unsupported type. (regenerate architecture)");
                        }
                        // Primate BFU
                        if(dyn_cast<MDString>(priTop->getOperand(1))->getString() == "IO") {
                            if(demangledName.find("PRIMATE::input<") != std::string::npos) {
                                bool needToExtract = false;
                                dbgs() << "generating intrinsic for\n";
                                inst->dump();
                                Type* resultType = inst->getType();
                                if(!resultType->isAggregateType()) {
                                    // generate an aggregate of the scalar type
                                    std::vector<Type *> structTypes = {
                                        resultType
                                    };
                                    resultType = llvm::StructType::get(F.getContext(), structTypes, true);
                                    needToExtract = true;
                                }
                                IRBuilder<> builder(inst);
                                std::vector<Type *> insArgType = {
                                    resultType
                                };
                                std::vector<Value*> insArg = {
                                    inst->getOperand(0)
                                };
                                Function* insFunc = llvm::Intrinsic::getDeclaration(F.getParent(), llvm::Intrinsic::primate_input, insArgType);
                                CallInst* newCi = builder.CreateCall(insFunc, insArg);
                                Value* newInstr = newCi;
                                if (needToExtract) {
                                    dbgs() << "creating struct for: ";
                                    resultType->dump();
                                    newInstr = builder.CreateExtractValue(newCi, 0);
                                }
                                inst->replaceAllUsesWith(newInstr);
                                instructionsToRemove.push_back(inst);
                            }
                            else if(demangledName.find("PRIMATE::input_done()") != std::string::npos) {
                                dbgs() << "generating intrinsic for\n";
                                inst->dump();
                                IRBuilder<> builder(inst);
                                std::vector<Type *> insArgType = {
                                };
                                std::vector<Value*> insArg = {
                                };
                                Function* insFunc = llvm::Intrinsic::getDeclaration(F.getParent(), llvm::Intrinsic::primate_input_done, insArgType);
                                CallInst* newCi = builder.CreateCall(insFunc, insArg);
                                inst->replaceAllUsesWith(newCi);
                                instructionsToRemove.push_back(inst);
                            }
                            else if(demangledName.find("PRIMATE::output<") != std::string::npos) {
                                IRBuilder<> builder(inst);
                                bool needToInsert = false;
                                Type* inputType = inst->getOperand(0)->getType();
                                Value* op0 = inst->getOperand(0);
                                if(!inputType->isAggregateType()) {
                                    // generate an aggregate of the scalar type
                                    std::vector<Type *> structTypes = {
                                        inputType
                                    };
                                    inputType = llvm::StructType::get(F.getContext(), structTypes, true);
                                    needToInsert = true;
                                    op0 = builder.CreateInsertValue(UndefValue::get(inputType), op0, 0); 
                                }

                                std::vector<Type *> insArgType = {
                                    inputType
                                };
                                std::vector<Value*> insArg = {
                                    op0,
                                    inst->getOperand(1)
                                };
                                Function* insFunc = llvm::Intrinsic::getDeclaration(F.getParent(), llvm::Intrinsic::primate_output, insArgType);
                                CallInst* newCi = builder.CreateCall(insFunc, insArg);
                                inst->replaceAllUsesWith(newCi);
                                instructionsToRemove.push_back(inst);
                            }
                            else if(demangledName.find("PRIMATE::output_done(") != std::string::npos) {
                                IRBuilder<> builder(inst);
                                std::vector<Type *> insArgType = {};
                                std::vector<Value*> insArg = {};
                                Function* insFunc = llvm::Intrinsic::getDeclaration(F.getParent(), llvm::Intrinsic::primate_output_done, insArgType);
                                CallInst* newCi = builder.CreateCall(insFunc, insArg);
                                inst->replaceAllUsesWith(newCi);
                                instructionsToRemove.push_back(inst);
                            }
                            else {
                                llvm_unreachable("UNSUPPORTED IO BFU OPERATION (Options are input, output)");
                            }
                        }
                        else {
                            std::string BFUName = dyn_cast<MDString>(priTop->getOperand(1))->getString().str();
                            rtrim(BFUName);
                            ltrim(BFUName);
                            IRBuilder<> builder(inst);
                            std::vector<Type *> insArgType = {
                                inst->getType(),
                                inst->getOperand(0)->getType()
                            };
                            std::vector<Value*> insArg = {
                                inst->getOperand(0)
                            };
                            if(nameToIntrins.find(BFUName) == nameToIntrins.end()) {
                                dbgs() << BFUName << "\n";
                            }
                            assert(nameToIntrins.find(BFUName) != nameToIntrins.end() && "unknown BFU name >:(");
                            Function* insFunc = llvm::Intrinsic::getDeclaration(F.getParent(), nameToIntrins.at(BFUName), insArgType);
                            CallInst* newCi = builder.CreateCall(insFunc, insArg);
                            inst->replaceAllUsesWith(newCi);
                            instructionsToRemove.push_back(inst);
                        }
                    }
                }
            }
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
                builder.SetInsertPoint(li);
                LoadInst* newLoad = builder.CreateLoad(srcType, srcPtr);
                Value* extLoad = builder.CreateExtractValue(newLoad, lastIndex, li->getName());
                LLVM_DEBUG(dbgs() << "replaced: "; li->dump(); 
                dbgs() << " with: "; extLoad->dump());
                li->replaceAllUsesWith(extLoad);
                LLVM_DEBUG(dbgs() << "Created Ops: ";
                newLoad->dump();
                extLoad->dump(););
                instructionsToRemove.push_back(li);
            }
            else if(auto* si = dyn_cast<StoreInst>(uIter)) {
                // TODO: HACK SHOULD BE OPT
                // load insert and store....
                builder.SetInsertPoint(si);
                LoadInst *newLoad = builder.CreateLoad(srcType, srcPtr);
                Value *insLoad = builder.CreateInsertValue(newLoad, si->getValueOperand(), lastIndex);
                StoreInst *newStore = builder.CreateStore(insLoad, srcPtr);
                LLVM_DEBUG(dbgs() << "Created Ops: ";
                newLoad->dump();
                insLoad->dump();
                newStore->dump(););
                instructionsToRemove.push_back(si);
            }
            else {
                llvm_unreachable("gep used by not a load/store D:");
            }
        }
        instructionsToRemove.push_back(gepI);
    }
}
