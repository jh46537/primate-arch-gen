#ifndef PRIMATEREGISTERNORMALIZE_H_
#define PRIMATEREGISTERNORMALIZE_H_

#include "llvm/Pass.h"

#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {
class PrimateRegisterNormalize : public MachineFunctionPass {

public: 
    static char ID;
    PrimateRegisterNormalize() : MachineFunctionPass(ID){};

    bool runOnMachineFunction(MachineFunction& MF) override;

};

char PrimateRegisterNormalize::ID = 0;
static RegisterPass<PrimateRegisterNormalize> X("primateRegisterNormalize", "Primate register normalization",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

}


#endif