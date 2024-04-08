

#include "PrimateCustomSchedule.h"
#include "Primate.h"

namespace llvm {

// Goal of this pass: Schedule use def chains in round robin

bool PrimateCustomSchedule::runOnMachineFunction(MachineFunction& MF) {
    return false;
}

MachineFunctionPass *createPrimateCustomSchedulePass();
void initializePrimateCustomSchedulePass(PassRegistry&);
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(PrimateCustomSchedule, "PrimateCustomSchedule",
                      "Primate Custom Schedule", false, false)
// temp
INITIALIZE_PASS_END(PrimateCustomSchedule, "PrimateCustomSchedule",
                    "Primate Custom Schedule", false, false)

llvm::MachineFunctionPass *llvm::createPrimateCustomSchedulePass() {
  return new llvm::PrimateCustomSchedule();
}
