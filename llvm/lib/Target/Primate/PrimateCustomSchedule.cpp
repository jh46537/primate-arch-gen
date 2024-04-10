

#include "PrimateCustomSchedule.h"
#include "Primate.h"
#include <vector>
#include <set>
#include <algorithm>
#include "llvm/Support/Debug.h"

namespace llvm {

// Goal of this pass: Schedule use def chains in round robin

bool PrimateCustomSchedule::runOnMachineFunction(MachineFunction& MF) {
  const MachineRegisterInfo& MRI = MF.getRegInfo();
  std::set<Register> checkedRegs;
  std::vector<MachineRegisterInfo::reg_instr_iterator> usedefChains;

  dbgs() << "Hello from the Primate Custom Scheduling Pass\n";
  for (MachineBasicBlock& MBB: MF) {
    checkedRegs.clear();
    usedefChains.clear();
    for (MachineInstr& MI: MBB) {
      // get all use-def chains in the BasicBlock
      for (MachineOperand& MO: MI.uses()) {
        if(MO.isReg()) {
          auto prevReg = std::find(checkedRegs.begin(), checkedRegs.end(), MO.getReg());
          if (prevReg == checkedRegs.end()) {
            MachineRegisterInfo::reg_instr_iterator curRegHead = MRI.reg_instr_begin(MO.getReg());
            usedefChains.push_back(curRegHead);
            checkedRegs.insert(MO.getReg());
          }
        }
      }
    }
    
    // remove instrs from their parent so we can reinsert
    for (auto& usedef: usedefChains){
      dbgs() << "new chain: ";
      for (auto& it = usedef; it != MRI.reg_instr_end(); it++) {
        MachineInstr& MI = *it;
        if(MI.getParent() == nullptr) {
          dbgs() << "no parent! ";
        }
        MI.dump();
      }
    }
  }
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
