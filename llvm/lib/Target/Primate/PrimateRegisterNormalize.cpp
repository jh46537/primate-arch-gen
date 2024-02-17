#include "PrimateRegisterNormalize.h"

#include "Primate.h"
#include "PrimateInstrInfo.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "PrimateRegisterNormalize"

namespace llvm {
bool llvm::PrimateRegisterNormalize::runOnMachineFunction(MachineFunction& MF) {
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  SmallVector<MachineInstr*> worklist; 
  MachineRegisterInfo &MRI = MF.getRegInfo();
  assert(MRI.isSSA() && "Not in SSA for generating extracts and inserts");

  for(MachineBasicBlock &MBB: MF) {
    for(MachineInstr &MI: MBB) {
      // instructions that are not inserts or extracts are not allowed to have
      // scalar registers. 
      // would be nice to do this on the dag but I swear I am so close to ending my own life.
      // I hate this project. 
      // this architecture is stupid.
      if(MI.getOpcode() == Primate::EXTRACT || MI.getOpcode() == Primate::INSERT) {
        dbgs() << "mogus ";
        for(auto &op: MI.uses()) {
          if(op.isReg() && MRI.getRegClass(op.getReg()) != &Primate::WIDEREGRegClass) {
            op.dump();
          }
        }
      }
    }
  }
  return false;
}

MachineFunctionPass *createPrimateRegisterNormalizePass();
void initializePrimateRegisterNormalizePass(PassRegistry&);
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(PrimateRegisterNormalize, "primateRegisterNormalize",
                      "Primate register normalization", false, false)
//INITIALIZE_PASS_DEPENDENCY(PrimateRegisterNormalize)
INITIALIZE_PASS_END(PrimateRegisterNormalize, "primateRegisterNormalize",
                    "Primate register normalization", false, false)

llvm::MachineFunctionPass *llvm::createPrimateRegisterNormalizePass() {
  return new llvm::PrimateRegisterNormalize();
}