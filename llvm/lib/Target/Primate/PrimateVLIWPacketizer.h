//===-- PrimateVLIWPacketizer.h - Primate VLIW packetizer -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATEVLIWPACKETIZER_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATEVLIWPACKETIZER_H

#include "llvm/CodeGen/DFAPacketizer.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include <vector>

namespace llvm {

class PrimateInstrInfo;
struct PrimateRegisterInfo;
class MachineBranchProbabilityInfo;
class MachineFunction;
class MachineInstr;
class MachineLoopInfo;
class TargetRegisterClass;

class PrimatePacketizerList : public VLIWPacketizerList {
protected:
  /// A handle to the branch probability pass.
  const MachineBranchProbabilityInfo *MBPI;
  const MachineLoopInfo *MLI;

private:
  const PrimateInstrInfo *PII;
  const PrimateRegisterInfo *PRI;

  bool insertBypassOps(MachineInstr* br_inst, llvm::SmallVector<MachineInstr*, 2>& generated_bypass_ops);

public:
  PrimatePacketizerList(MachineFunction &MF, MachineLoopInfo &MLI,
                        AAResults *AA,
                        const MachineBranchProbabilityInfo *MBPI);

  MachineBasicBlock::iterator addToPacket(MachineInstr &MI) override;
  void endPacket(MachineBasicBlock *MBB,
                 MachineBasicBlock::iterator MI) override;
  void initPacketizerState() override;
  bool ignorePseudoInstruction(const MachineInstr &MI,
                               const MachineBasicBlock *MBB) override;
  bool isSoloInstruction(const MachineInstr &MI) override;
  bool shouldAddToPacket(const MachineInstr &MI) override;
  bool isLegalToPacketizeTogether(SUnit *SUI, SUnit *SUJ) override;
  bool isLegalToPruneDependencies(SUnit *SUI, SUnit *SUJ) override;
  bool ignoreInstruction(const MachineInstr &I, const MachineBasicBlock *MBB) override;
  void tryToPullBitmanip(MachineInstr *I);


protected:
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_PRIMATE_PRIMATEVLIWPACKETIZER_H
