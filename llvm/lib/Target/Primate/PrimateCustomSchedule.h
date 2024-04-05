//===-- PrimateVLIWPacketizer.h - Primate VLIW packetizer -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATECUSTOMSCHEDULE_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATECUSTOMSCHEDULE_H

#include "llvm/Pass.h"

#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "PrimateInstrInfo.h"
#include "PrimateSubtarget.h"

namespace llvm {

class PrimateCustomSchedule : public MachineFunctionPass {
public:
    static char ID;
    PrimatePacketLegalizer() : MachineFunctionPass(ID){};

    bool runOnMachineFunction(MachineFunction& MF) override;

private:
    const PrimateTargetLowering *TLI;
    const PrimateInstrInfo *PII;
    const TargetRegisterInfo *TRI;
    DFAPacketizer* ResourceTracker;
    bool isWideReg(const Register) const;
    void fixBundle(MachineInstr *BundleMI);
    bool hasScalarRegs(MachineInstr*);
    bool hasScalarDefs(MachineInstr*);
    bool hasScalarOps(MachineInstr*);

};

char PrimatePacketLegalizer::ID = 0;
static RegisterPass<PrimatePacketLegalizer> X("PrimatePacketLegalizer", "Primate Packet Legalizer",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

} // end namespace llvm

#endif // LLVM_LIB_TARGET_PRIMATE_PRIMATECUSTOMSCHEDULE_H
