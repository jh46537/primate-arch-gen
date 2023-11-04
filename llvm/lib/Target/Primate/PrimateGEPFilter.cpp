
#include "PrimateGEPFilter.h"

#include "llvm/Support/Debug.h"
#include "llvm/Demangle/Demangle.h"
#include <string>

#define DEBUG_TYPE "GEP-Filter"

namespace llvm {

// goal: add extracts and inserts infront of all ops.
// starting from a GEP, walk down dep graph until we hit ops. ops need
// prepending of ext, and postpending ins.
//
    PreservedAnalyses PrimateGEPFilterPass::run(Module& module, ModuleAnalysisManager &PA) {

        Module *mod = &module;
        for (auto &F: module) {
            LLVM_DEBUG(dbgs() << "Filtering GEP from: " << F.getName() << "\n");
            for (auto &bb : F) {
                auto &context = mod->getContext();
                SmallVector<Instruction *> insts_to_remove;
                SmallVector<Value *> worklist;

                for (auto inst_iter = bb.begin(); inst_iter != bb.end();) {
                    Instruction *inst = dyn_cast<Instruction>(inst_iter++);
                    CallInst *call_inst;
                    std::string func_name = "";
                    bool func_found = false;
                    if ((call_inst = dyn_cast<CallInst>(inst))) {
                        Function *func = call_inst->getCalledFunction();
                        if (func) {
                            func_name = llvm::demangle(func->getName().str());
                            func_found = true;
                        }
                    }

                    // no func. next inst.
                    if(!func_found) {
                        continue;
                    }

                    llvm::IRBuilder<> builder(inst);

                    if(func_name == "Insert(int, int, int)") {
                        // create insert
                        errs() << "creating an insert intrinsic\n";
                        std::vector<Type *> arg_type = {
                            inst->getOperand(0)->getType(), // out
                            inst->getOperand(0)->getType(), // in
                            IntegerType::get(context,32) // field code
                        };
                        std::vector<Value *> args = {
                            call_inst->getOperand(0), // reg in
                            call_inst->getOperand(2),  // value to insert
                            cast<ConstantInt>(call_inst->getOperand(1)) // field code
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_insert, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        inst->replaceAllUsesWith(call);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "Extract(int, int)") {
                        // create extract
                        errs() << "creating an extract intrinsic\n";
                        std::vector<Type *> arg_type = {
                            IntegerType::get(context,32),
                            IntegerType::get(context,32)
                        };
                        std::vector<Value *> args = {
                            call_inst->getOperand(0), // reg
                            cast<ConstantInt>(call_inst->getOperand(1))  // field
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_extract, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        inst->replaceAllUsesWith(call);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "Output_meta(int)") {
                        errs() << "creating an Output_meta intrinsic\n";
                        std::vector<Type *> arg_type = {
                            IntegerType::get(context,32)
                        };
                        std::vector<Value *> args = {
                            call_inst->getOperand(0)
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_Output_meta, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        insts_to_remove.push_back(inst);

                    }
                    else if(func_name == "Output_header(int, int)") {
                        errs() << "creating an Output_header intrinsic\n";
                        std::vector<Type *> arg_type = {
                            IntegerType::get(context,32)
                        };
                        std::vector<Value *> args = {
                            call_inst->getOperand(1), // reg
                            cast<ConstantInt>(call_inst->getOperand(0)) // size
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_Output_header, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "Output_done()") {
                        errs() << "creating an Output_done intrinsic\n";
                        std::vector<Type *> arg_type = {
                        };
                        std::vector<Value *> args = {
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_Output_done, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "forward_exact(int)") {
                        errs() << "creating an forward_exact intrinsic\n";
                        std::vector<Type *> arg_type = {
                            IntegerType::get(context,32),
                            IntegerType::get(context,32)
                        };
                        std::vector<Value *> args = {
                            call_inst->getOperand(0)
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_forward_exact, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        inst->replaceAllUsesWith(call);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "Input_header(int)") {
                        errs() << "creating an Input_header intrinsic\n";
                        std::vector<Type *> arg_type = {
                        };
                        std::vector<Value *> args = {
                            ConstantInt::get(IntegerType::get(context,32), 0),
                            cast<ConstantInt>(call_inst->getOperand(0))
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_Input_header, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        inst->replaceAllUsesWith(call);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "Input_done()") {
                        errs() << "creating an Input_done intrinsic\n";
                        std::vector<Type *> arg_type = {
                        };
                        std::vector<Value *> args = {
                        };
                        Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_Input_done, arg_type);
                        CallInst* call = builder.CreateCall(func,args);
                        insts_to_remove.push_back(inst);
                    }
                    else if(func_name == "init()") {
                        errs() << "replacing init with nop for now\n";
                        Value* alloc =  builder.CreateAdd(ConstantInt::get(IntegerType::get(context,32), 0), 
                                                                ConstantInt::get(IntegerType::get(context,32), 0));
                        inst->replaceAllUsesWith(alloc);
                        insts_to_remove.push_back(inst);
                    }
                    else {
                        errs() << "Non-primate func: " << func_name << "\n";
                    }
                } // for instruction
                for(Instruction *inst: insts_to_remove) {
                    inst->eraseFromParent();
                }
            }// for basic block
        }// for functions
        return PreservedAnalyses::none();
    } // runOnFunction
} // namespace llvm