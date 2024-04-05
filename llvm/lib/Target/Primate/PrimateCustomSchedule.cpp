

#include "PrimateCustomSchedule.h"
#include "Primate.h"

namespace llvm {

// Goal of this pass: Schedule use def chains in round robin

bool PrimateCustomSchedule::runOnMachineFunction(MachineFunction& MF) {
    return false;
}

MachineFunctionPass *createPrimateCustomSchedule();
void initializePrimateCustomSchedule(PassRegistry&);
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(PrimateCustomSchedule, "PrimateCustomSchedule",
                      "Primate Custom Schedule", false, false)
INITIALIZE_PASS_END(PrimateCustomSchedule, "PrimateCustomSchedule",
                    "Primate Custom Schedule", false, false)

llvm::MachineFunctionPass *llvm::createPrimateCustomSchedule() {
  return new llvm::PrimateCustomSchedule();
}
