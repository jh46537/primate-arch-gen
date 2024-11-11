#include "PrimatePrinterPass.h"

using namespace llvm;

PreservedAnalyses PrimatePrinterPass::run(Module& M, ModuleAnalysisManager& MAM) {
  dbgs() << "Primate Printer Pass\n";
  for(auto& F: M){
    for(auto& BB: F) {
      for(auto& Inst: BB) {
	if(CallInst* ci = dyn_cast<CallInst>(&Inst)) {
	  auto* called = ci->getCalledFunction();
	  auto* ft = called->getFunctionType();
	  called->dump();
	  for(int i = 0; i < ft->getNumParams(); i++) {
	    if(Type* byVal = called->getParamByValType(i)) {
	      byVal->dump();
	    }
	    else if(Type* byAlloca = called->getParamInAllocaType(i)) {
	      byAlloca->dump();
	    }
	    else if(Type* byRef = called->getParamByRefType(i)) {
	      byRef->dump();
	    }
	    else {
	      ft->getParamType(i)->dump();
	    }
	  }
	}
      }
    }
  }
  return PreservedAnalyses::all();
}
