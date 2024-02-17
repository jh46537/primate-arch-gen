#ifndef PRIMATEOPMERGE_H_
#define PRIMATEOPMERGE_H_

#include "llvm/Pass.h"

#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {
class PrimateOPMerge : public MachineFunctionPass {

public:
    static char ID;
    PrimateOPMerge() : MachineFunctionPass(ID){};

    bool runOnMachineFunction(MachineFunction& MF) override;

};

char PrimateOPMerge::ID = 0;
static RegisterPass<PrimateOPMerge> X("PrimateOPMerge", "Primate Operation Merge",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
}

#endif
