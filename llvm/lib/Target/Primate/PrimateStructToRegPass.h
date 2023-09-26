#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATE_STRUCT_TO_REG_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATE_STRUCT_TO_REG_H

#include <iostream>

#include "Primate.h"
#include "PrimateTargetMachine.h"

// required for function pass
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm {
class PrimateStructToRegPass : public FunctionPass {
public:
  static char ID;
  explicit PrimateStructToRegPass() : FunctionPass(ID) {
  }

  StringRef getPassName() const override {
    return "Primate Struct to Register lowering";
  }
    
  bool runOnFunction(Function& F) override {
    errs() << "Hello from struct to reg pass\n";
    errs() << F.getName() << "\n";

    for(auto &bb: F) {
      for(auto &inst: bb) {
	if(llvm::isa<llvm::GetElementPtrInst>(inst)) {
	  auto ptr_type = inst.getType();
	  errs() << ptr_type->getStructName();
	  for(auto u = inst.user_begin(); u != inst.user_end(); u++) {
	    u->dump();
	  }
	  inst.dump();
	}
      }
    }
    
    return true;
  }
  void getAnalysisUsage(AnalysisUsage &Info) const override {
    //    Info.addRequired<>
  }
};
}

INITIALIZE_PASS(PrimateStructToRegPass, "primate-insert-struct2reg", "Primate Struct to Register lowering",
                false, false)

char llvm::PrimateStructToRegPass::ID = 0;

FunctionPass *llvm::createPrimateStructToRegPass() {
  return new llvm::PrimateStructToRegPass;
}

#endif
