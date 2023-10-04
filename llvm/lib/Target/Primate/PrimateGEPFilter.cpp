
#include "PrimateGEPFilter.h"

#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "GEP-Filter"

namespace llvm{
PreservedAnalyses PrimateGEPFilterPass::run(Function& F, FunctionAnalysisManager& PA) {

    auto ret_PA = PreservedAnalyses::none();

    LLVM_DEBUG(dbgs() << "Filtering GEP from: " << F.getName() << "\n");

    for(auto& bb: F) {
        Module* mod = F.getParent();
        auto& context = mod->getContext();
        IRBuilder<> builder(&bb);
        SmallVector<Instruction*> insts_to_remove;

        for(auto inst_iter = bb.begin(); inst_iter != bb.end();) {
            Instruction& inst = *(inst_iter++);
            if (auto *gep_inst = llvm::dyn_cast<llvm::GetElementPtrInst>(&inst)) {
                LLVM_DEBUG({
                    dbgs() << "inspecting instr:\n";
                    dbgs() << "Type: " << *gep_inst->getSourceElementType() << "\n";
                    dbgs() << "inds: ";
                    for (auto& ind: gep_inst->indices()) { // last index is the offset into the struct
                        dbgs() << *ind << " ";
                    }
                    dbgs() << "\n";
                    inst.dump();
                });    
                for(auto u = inst.user_begin(); u != inst.user_end(); u++) {
                    u->dump();
                }
                Value* src_ptr = gep_inst->getOperand(0);
                auto ldInstIter = (std::find_if(inst.user_begin(), inst.user_end(),
                                [](User* instr){
                                    return llvm::dyn_cast<llvm::LoadInst>(instr) != nullptr;
                                }));
                LoadInst *ldInst = ldInstIter == inst.user_end() ? nullptr : llvm::dyn_cast<llvm::LoadInst>(*ldInstIter);
                auto stInstIter = (std::find_if(inst.user_begin(), inst.user_end(),
                                    [](User* instr){
                                        return llvm::dyn_cast<llvm::StoreInst>(instr) != nullptr;
                                    }));
                StoreInst *stInst = stInstIter == inst.user_end() ? nullptr : llvm::dyn_cast<llvm::StoreInst>(*stInstIter);
                // find load consumers
                SmallVector<User*> consuming_ops;
                for (auto u = ldInst->user_begin(); u != ldInst->user_end(); u++) {
                    consuming_ops.push_back(*u);
                }
                LLVM_DEBUG(dbgs() << "end users\n");

                StructType* struct_type = nullptr;
                if((struct_type = llvm::dyn_cast<llvm::StructType>(gep_inst->getSourceElementType())) && (gep_inst->getNumIndices() >= 2)) {
                    assert(ldInst && "no load for the gep???");
                    LLVM_DEBUG(dbgs() << "inst is a struct ptr\n");
                    
                    // should be struct. so last should be constant value
                    ConstantInt* last_ind = dyn_cast<ConstantInt>((gep_inst->idx_begin() + gep_inst->getNumIndices()-1)->get());
                    assert(last_ind && "last index to GEP was non-constant!");
                    assert(last_ind->getBitWidth() <= 32 && "last index to GEP larger than 32-bits");
                    int start_bit = 0;
                    int ele_size = ldInst->getType()->getPrimitiveSizeInBits();
                    for(int i = 0; i < (int)last_ind->getSExtValue(); i++) {
                        start_bit += struct_type->elements()[i]->getPrimitiveSizeInBits();
                    }
                    LLVM_DEBUG (dbgs() << "starting bit pos: " << start_bit);
                    LLVM_DEBUG (dbgs() << "size of element: " << ele_size);
                    builder.SetInsertPoint(ldInst->getPrevNode()->getNextNode());

                    SmallVector<Value *> newIdxList;
                    for (Value *ind: gep_inst->indices()) { // last index is the offset into the struct
                        newIdxList.push_back(ind);
                    }
                    newIdxList.pop_back();

                    // Generate a trimmed GEP (i.e array of structs)
                    // Value* newGep = builder.CreateGEP(struct_type, src_ptr, newIdxList);

                    // Generate a new load the consumes the old ptr.
                    LoadInst *newLd = builder.CreateLoad(struct_type, src_ptr);

                    // Generate the extract.
                    std::vector<Type *> arg_type = {
                        ldInst->getType(),
                        struct_type,
                        IntegerType::get(context,32)
                    };
                    std::vector<Value *> args = {
                        newLd,
                        ConstantInt::get(IntegerType::get(context,32),
                                (( ((start_bit&((1<<5)-1)) << 5) +
                                ((start_bit + ele_size)&((1<<5)-1))) ) & 0x0FFF)
                    };
                    Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_extract, arg_type);
                    CallInst* call = builder.CreateCall(func,args);

                    // Route extract to result of the load
                    for (auto* user_op: consuming_ops) {
                        user_op->replaceUsesOfWith(ldInst, call);
                    }

                    if(stInst) {
                        LLVM_DEBUG(dbgs() << "Store Inst found. Replace with insert.");

                        builder.SetInsertPoint(stInst->getPrevNode()->getNextNode());

                        // first insert value into structure
                        std::vector<Type *> insArgType = {
                            struct_type,
                            struct_type,
                            stInst->getOperand(0)->getType(),
                            IntegerType::get(context,32)
                        };
                        std::vector<Value *> insArgs = {
                            newLd,
                            stInst->getOperand(0),
                            ConstantInt::get(IntegerType::get(context,32),
                                    (( ((start_bit&((1<<5)-1)) << 5) +
                                    ((start_bit + ele_size)&((1<<5)-1))) ) & 0x0FFF)
                        };
                        Function* insFunc = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_insert, insArgType);
                        CallInst* call = builder.CreateCall(insFunc,insArgs);

                        builder.CreateStore(call, src_ptr);
                        

                        insts_to_remove.push_back(stInst);
                    }

                    // clean up
                    insts_to_remove.push_back(ldInst);
                    insts_to_remove.push_back(gep_inst);

                    bb.dump();
                }
                
                LLVM_DEBUG(dbgs() << "\n");
            }
        }
        for(auto *inst: insts_to_remove) {
            // if the instruction still has users it 
            // better be a GEP. In that case we can replace the GEP
            // with just the raw pointer
            GetElementPtrInst *gepInst;
            if(!inst->user_empty() && (gepInst = dyn_cast<GetElementPtrInst>(inst))) {
                for (auto u = gepInst->user_begin(); u != gepInst->user_end(); u++) {
                    u->replaceUsesOfWith(gepInst, gepInst->getOperand(0));
                }
            }
            assert(inst->user_empty() && "There are still users of the instruction we want to remove");
            inst->eraseFromParent();
        }
        F.dump();
    }
    return ret_PA;
}
}

