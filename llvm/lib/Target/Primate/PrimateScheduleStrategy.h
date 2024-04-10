//===-- R600MachineScheduler.h - R600 Scheduler Interface -*- C++ -*-------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// R600 Machine Scheduler interface
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_MACHINESCHEDULER_H
#define LLVM_LIB_TARGET_PRIMATE_MACHINESCHEDULER_H

#include "llvm/CodeGen/MachineScheduler.h"
#include "PrimateSubtarget.h"
#include "llvm/CodeGen/DFAPacketizer.h"
#include <vector>
#include <set>

using namespace llvm;

namespace llvm {

class PrimateInstrInfo;
struct PrimateRegisterInfo;

class PrimateSchedStrategy final : public MachineSchedStrategy {
  const PrimateTargetLowering *TLI;
  const PrimateInstrInfo *PII;
  const TargetRegisterInfo *TRI;
  DFAPacketizer* ResourceTracker;

  std::set<SUnit*> candidates;

public:
  PrimateSchedStrategy() = default;
  ~PrimateSchedStrategy() override = default;

  void initialize(ScheduleDAGMI *dag) override;
  SUnit *pickNode(bool &IsTopNode) override;
  void schedNode(SUnit *SU, bool IsTopNode) override;
  void releaseTopNode(SUnit *SU) override;
  void releaseBottomNode(SUnit *SU) override;
};

} // end namespace llvm

#endif
