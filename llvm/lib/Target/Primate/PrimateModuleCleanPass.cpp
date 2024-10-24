#include "PrimateModuleCleanPass.h"

#include "llvm/Demangle/Demangle.h"
 
using namespace llvm;

#define DEBUG_TYPE "primate-module-clean"

PreservedAnalyses PrimateModuleCleanPass::run(Module& M, ModuleAnalysisManager& MPM) {
    bool change = false;
    SmallVector<Function*, 8> functionsToRemove;

    for(auto& F: M) {
        if (demangle(F.getName()).find("primate_main") != std::string::npos) {
            continue;
        }
        if(F.getNumUses() == 0) {
            functionsToRemove.push_back(&F);
            change = true;
        }
    }
    for(auto* F: functionsToRemove) {
        LLVM_DEBUG(dbgs() << "Removing function: " << F->getName() << "\n";);
        F->eraseFromParent();
    }

    if(!change)
        return PreservedAnalyses::all();
    return PreservedAnalyses::none();
}