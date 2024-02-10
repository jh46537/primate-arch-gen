
#include "PrimateExtMerge.h"

namespace llvm {

bool PrimateExtMerge::runOnMachineFunction(MachineFunction& MF) {
    for(MachineBasicBlock &MBB : MF) {
        for(MachineInstr &MI: MBB) {
            MI.dump();
        }
    }
    return false;
}

MachineFunctionPass *createPrimateExtMergePass();
void initializePrimateExtMergePass(PassRegistry&);
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(PrimateExtMerge, "primateExtMerge",
                      "Primate Extract Merger", false, false)
//INITIALIZE_PASS_DEPENDENCY(PrimateRegisterNormalize)
INITIALIZE_PASS_END(PrimateExtMerge, "primateExtMerge",
                    "Primate Extract Merger", false, false)

llvm::MachineFunctionPass *llvm::createPrimateExtMergePass() {
  return new llvm::PrimateExtMerge();
}
