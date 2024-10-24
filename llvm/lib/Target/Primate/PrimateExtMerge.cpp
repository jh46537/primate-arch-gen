
#include "PrimateExtMerge.h"
#include "Primate.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

namespace llvm {

// Goal of this pass: SSA generates extra extracts and inserts
// IF between an extract and an insert there is no operation, 
// Then we can delete both of them, our data is mutable.

bool PrimateExtMerge::runOnMachineFunction(MachineFunction& MF) {
    dbgs() << "hello from Primate Extract Merger\n";
    SmallVector<MachineInstr*> worklist; 
    SmallVector<MachineInstr*> MIToRemove;
    MachineRegisterInfo &MRI = MF.getRegInfo();
    assert(MRI.isSSA() && "Not in SSA for merging aggregate ext/ins");
    for(MachineBasicBlock &MBB : MF) {
        worklist.clear();
        MIToRemove.clear(); 
        for(MachineInstr &MI: MBB) {
            MI.dump();
            if(MI.getOpcode() == Primate::INSERT) {
                worklist.push_back(&MI);
            }
        }
        for(auto* MI : worklist) {
            dbgs() << "------\n";
            MI->dump();
            Register defReg = MI->getOperand(0).getReg();
            Register wideInReg = MI->getOperand(1).getReg();
            Register valueOp = MI->getOperand(2).getReg();
            dbgs() << "Operand 1 is: "; MI->getOperand(2).dump();
            assert(Register::isVirtualRegister(valueOp) && "op 1 is not a v-reg");
            dbgs() << "Users for reg "<< Register::virtReg2Index(valueOp)  << ": \n";

            // check if def comes from ext.
            MachineInstr* defInstr = MRI.getVRegDef(valueOp);
            assert(defInstr && "defining instruction doesnt exist for op 1 of insert...");
            if(defInstr->getOpcode() == Primate::EXTRACT) {
                defInstr->dump();
                MIToRemove.push_back(defInstr);
                MIToRemove.push_back(MI);
                for(auto& userOP: MRI.use_operands(defReg)) {
                    userOP.setReg(wideInReg);
                }
            }

            // if def came from extract we will remove the insert, replace the use of the ins with the beegboiii

            dbgs() << "------\n";
        }

        dbgs() << "removing ops\n";
        // for(auto* MI : MIToRemove) {
        //     // FIXME: if the instr is used in multiple places then we can't remove it
        //     MI->eraseFromParent();
        // }
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
