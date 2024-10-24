

#include "PrimateOpMerge.h"
#include "Primate.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

namespace llvm {

// Goal of this pass: we would like to merge extract with the 
// operation that is doing the extraction
// if 

bool PrimateOPMerge::runOnMachineFunction(MachineFunction& MF) {
    dbgs() << "hello from Primate Extract Merger\n";
    SmallVector<MachineInstr*> worklist; 
    SmallVector<MachineInstr*> MIToRemove;
    MachineRegisterInfo &MRI = MF.getRegInfo();
    assert(MRI.isSSA() && "Not in SSA for merging aggregate ext/ins");
    // for(MachineBasicBlock &MBB : MF) {
    //     worklist.clear();
    //     MIToRemove.clear(); 
    //     for(MachineInstr &MI: MBB) {
    //         MI.dump();
    //         if(MI.getOpcode() != Primate::EXTRACT) {
    //             worklist.push_back(&MI);
    //         }
    //     }
    // }

    return false;
}

MachineFunctionPass *createPrimateOPMergePass();
void initializePrimateOPMergePass(PassRegistry&);
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(PrimateOPMerge, "PrimateOPMerge",
                      "Primate OP Merge", false, false)
//INITIALIZE_PASS_DEPENDENCY(PrimateRegisterNormalize)
INITIALIZE_PASS_END(PrimateOPMerge, "PrimateOPMerge",
                    "Primate OP Merge", false, false)

llvm::MachineFunctionPass *llvm::createPrimateOPMergePass() {
  return new llvm::PrimateOPMerge();
}
