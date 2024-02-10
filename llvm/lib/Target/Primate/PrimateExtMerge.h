#ifndef PRIMATEEXTRACTMERGE_H_
#define PRIMATEEXTRACTMERGE_H_

#include "llvm/Pass.h"

#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {
class PrimateExtMerge : public MachineFunctionPass {

public:
    static char ID;
    PrimateExtMerge() : MachineFunctionPass(ID){};

    bool runOnMachineFunction(MachineFunction& MF) override;

};

char PrimateExtMerge::ID = 0;
static RegisterPass<PrimateExtMerge> X("primateExtMerge", "Primate Extract Insert Merge",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
}

#endif
