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
    PrimateCustomSchedule() : MachineFunctionPass(ID){};

    bool runOnMachineFunction(MachineFunction& MF) override;

};

char PrimateCustomSchedule::ID = 0;
static RegisterPass<PrimateCustomSchedule> X("PrimateCustomSchedule", "Primate Custom Schedule",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

} // end namespace llvm

#endif // LLVM_LIB_TARGET_PRIMATE_PRIMATECUSTOMSCHEDULE_H
