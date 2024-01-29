#include "PrimateRegisterNormalize.h"

#include "Primate.h"
#include "PrimateInstrInfo.h"
#include "PrimateTargetMachine.h"

#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

namespace llvm {
bool llvm::PrimateRegisterNormalize::runOnMachineFunction(MachineFunction& MF) {
    dbgs() << "hello from register normalize";

    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    for (MCRegUnitIterator Units(Primate::X0, TRI); Units.isValid(); ++Units) {
        // do something
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