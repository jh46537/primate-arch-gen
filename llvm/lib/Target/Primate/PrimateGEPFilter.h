//===-- PrimatePasses.h - Primate Middle End Passes --------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
/// \file
//===----------------------------------------------------------------------===//

#ifndef PRIMATE_PASSES_H
#define PRIMATE_PASSES_H
#define PRIMATE_GEP_PASS_NAME "Primate GEP Remove Pass"
#define PRIMATE_LOAD_PASS_NAME "Primate Load Combine Pass"
#include <algorithm>

#include "Primate.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsPrimate.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
  struct PrimateGEPFilterPass : public PassInfoMixin<PrimateGEPFilterPass> {
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& PA) {

      auto ret_PA = PreservedAnalyses::none();
      
      errs() << "waaaaa\n" << F.getName() << "\n";
	
      for(auto& bb: F) {
	Module* mod = F.getParent();
	auto& context = mod->getContext();
	IRBuilder<> builder(&bb);
	SmallVector<Instruction*> insts_to_remove;

	for(auto inst_iter = bb.begin(); inst_iter != bb.end();) {
	  Instruction& inst = *(inst_iter++);
	  if (auto *gep_inst = llvm::dyn_cast<llvm::GetElementPtrInst>(&inst)) {
	    errs() << "inspecting instr:\n";
	    errs() << "Type: " << *gep_inst->getSourceElementType() << "\n";
	    errs() << "inds: ";
	    for (auto& ind: gep_inst->indices()) { // last index is the offset into the struct
	      errs() << *ind << " ";
	    }
	    errs() << "\n";
	    inst.dump();
	    
	    for(auto u = inst.user_begin(); u != inst.user_end(); u++) {
	      u->dump();
	    }
	    Value* src_ptr = gep_inst->getOperand(0);
	    LoadInst *ldInst = llvm::dyn_cast<llvm::LoadInst>(*(std::find_if(inst.user_begin(), inst.user_end(),
					    [](User* instr){
					      return llvm::dyn_cast<llvm::LoadInst>(instr) != nullptr;
					    })));
	    // find load consumers
	    SmallVector<User*> consuming_ops;
	    for (auto u = ldInst->user_begin(); u != ldInst->user_end(); u++) {
	      consuming_ops.push_back(*u);
	    }
	    errs() << "end users\n";
	    
	    if(StructType* struct_type = llvm::dyn_cast<llvm::StructType>(gep_inst->getSourceElementType())) {
	      assert(ldInst && "no load for the gep???");
	      errs() << "inst is a struct ptr\n";
	      
	      // should be struct. so last should be constant value
	      ConstantInt* last_ind = dyn_cast<ConstantInt>((gep_inst->idx_begin() + gep_inst->getNumIndices()-1)->get());
	      assert(last_ind && "last index to GEP was non-constant!");
	      assert(last_ind->getBitWidth() <= 32 && "last index to GEP larger than 32-bits");
	      int start_bit = 0;
	      int ele_size = ldInst->getType()->getPrimitiveSizeInBits();
	      for(int i = 0; i < (int)last_ind->getSExtValue(); i++) {
		start_bit += struct_type->elements()[i]->getPrimitiveSizeInBits();
	      }
	      errs() << "starting bit pos: " << start_bit;
	      errs() << "size of element: " << ele_size;
	      builder.SetInsertPoint(ldInst->getPrevNode()->getNextNode());

	      // First generate a new load the consumes the old ptr.
	      LoadInst *newLd = builder.CreateLoad(struct_type, src_ptr);

	      // Generate the extract.
	      std::vector<Type *> arg_type = {
		ldInst->getType(),
		struct_type,
		IntegerType::get(context,32),
		IntegerType::get(context,32)
	      };
	      std::vector<Value *> args = {
		newLd,
		ConstantInt::get(IntegerType::get(context,32), start_bit),
		ConstantInt::get(IntegerType::get(context,32), start_bit + ele_size)
	      };
	      Function* func = llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::primate_extract, arg_type);
	      CallInst* call = builder.CreateCall(func,args);

	      // Route extract to result of the load
	      for (auto* user_op: consuming_ops) {
		user_op->replaceUsesOfWith(ldInst, call);
	      }

	      // clean up
	      insts_to_remove.push_back(ldInst);
	      insts_to_remove.push_back(gep_inst);

	      bb.dump();
	    }
	    errs() << "\n\n\n";
	  }
	}
	for(auto * inst: insts_to_remove) {
	  inst->eraseFromParent();
	}
	F.dump();
      }
      return ret_PA;
    }
  };

  struct PrimateStructLoadCombinerPass : public PassInfoMixin<PrimateStructLoadCombinerPass> {
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& PA) {
      errs() << "weeee" << F.getName() << "\n";
      for(auto& bb: F) {
	for(auto& inst: bb) {
	  if (llvm::isa<llvm::GetElementPtrInst>(inst)) {
	    errs() << "inspecting instr: ";
	    inst.dump();
	    for(auto u = inst.user_begin(); u != inst.user_end(); u++) {
	      u->dump();
	    }
	    errs() << "end users\n";
	  }
	}
      }
      return PreservedAnalyses::all();
    }
  };
}

#endif
