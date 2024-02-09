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
    LLVM_DEBUG(dbgs() << "hello from register normalize\n");

    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    unsigned a = TRI->getNumRegClasses();
    unsigned b = TRI->getNumRegUnits();
    for (MCRegUnitIterator Units(Primate::P_A0, TRI); Units.isValid(); ++Units) {
      // do something
      LLVM_DEBUG(dbgs() << "Register 0 has unit: " << *Units << "\n");
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