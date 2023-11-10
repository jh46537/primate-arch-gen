//===-- PrimateVLIWPacketizer.h - Primate VLIW packetizer -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATEBUNDLEPOSTPROCESS_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATEBUNDLEPOSTPROCESS_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "Primate.h"
#include "PrimateVLIWPacketizer.h"
#include "PrimatePacketPostProc.h"
#include "PrimateInstrInfo.h"
#include "PrimateRegisterInfo.h"
#include "PrimateSubtarget.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include <vector>

namespace llvm {

class PrimatePacketPostProc : public MachineFunctionPass {

protected:
private:
    SmallVector<MachineInstr*> exts;
    SmallVector<MachineInstr*> ins;
    SmallVector<MachineInstr*> ops;

    const PrimateInstrInfo *PII;
    bool addOpForInsert(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder);
    bool addExtractForOp(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder);
    bool fixDanglingExt(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder);
    bool fixMaterializedReg(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder);
    bool fixBranchOperandIndexes(MachineInstr* BranchInstr, MachineInstr* Bundle);
    bool addNOPs(MachineFunction* MF, MachineInstr& bundle, MIBundleBuilder& builder);
  
public:
    static char ID;
    PrimatePacketPostProc() : MachineFunctionPass(ID) {
    }

    bool runOnMachineFunction(MachineFunction& MF) override;

protected:
};

char PrimatePacketPostProc::ID = 0;
static RegisterPass<PrimatePacketPostProc> X("primateBundlePostProc", "Primate Bundle Post Process Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

} // end namespace llvm

#endif // LLVM_LIB_TARGET_PRIMATE_PRIMATEVLIWPACKETIZER_H
