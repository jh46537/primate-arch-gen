//===-- PrimateMCTargetDesc.cpp - Primate Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file provides Primate-specific target descriptions.
///
//===----------------------------------------------------------------------===//

#include "PrimateMCTargetDesc.h"
#include "PrimateBaseInfo.h"
#include "PrimateELFStreamer.h"
#include "PrimateInstPrinter.h"
#include "PrimateMCAsmInfo.h"
#include "PrimateTargetStreamer.h"
#include "TargetInfo/PrimateTargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/MC/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "PrimateGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "PrimateGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "PrimateGenSubtargetInfo.inc"

using namespace llvm;

static MCInstrInfo *createPrimateMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitPrimateMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createPrimateMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitPrimateMCRegisterInfo(X, Primate::X1);
  return X;
}

static MCAsmInfo *createPrimateMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new PrimateMCAsmInfo(TT);

  MCRegister SP = MRI.getDwarfRegNum(Primate::X2, true);
  MCCFIInstruction Inst = MCCFIInstruction::cfiDefCfa(nullptr, SP, 0);
  MAI->addInitialFrameState(Inst);

  return MAI;
}

static MCSubtargetInfo *createPrimateMCSubtargetInfo(const Triple &TT,
                                                   StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = TT.isArch64Bit() ? "generic-pr64" : "generic-pr32";
  if (CPU == "generic")
    report_fatal_error(Twine("CPU 'generic' is not supported. Use ") +
                       (TT.isArch64Bit() ? "generic-pr64" : "generic-pr32"));
  return createPrimateMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCInstPrinter *createPrimateMCInstPrinter(const Triple &T,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  return new PrimateInstPrinter(MAI, MII, MRI);
}

static MCTargetStreamer *
createPrimateObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new PrimateTargetELFStreamer(S, STI);
  return nullptr;
}

static MCTargetStreamer *createPrimateAsmTargetStreamer(MCStreamer &S,
                                                      formatted_raw_ostream &OS,
                                                      MCInstPrinter *InstPrint,
                                                      bool isVerboseAsm) {
  return new PrimateTargetAsmStreamer(S, OS);
}

static MCTargetStreamer *createPrimateNullTargetStreamer(MCStreamer &S) {
  return new PrimateTargetStreamer(S);
}

StringRef llvm::selectPrimateCPU(StringRef CPU, bool Is64Bit) {
  if (CPU.empty())
    CPU = Is64Bit ? "generic-pr64" : "generic-pr32";
  if (CPU == "generic")
    report_fatal_error(Twine("CPU 'generic' is not supported. Use ") +
                       (Is64Bit ? "generic-pr64" : "generic-pr32"));
  return CPU;
}

namespace {

class PrimateMCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit PrimateMCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    if (isConditionalBranch(Inst)) {
      int64_t Imm;
      if (Size == 2)
        Imm = Inst.getOperand(1).getImm();
      else
        Imm = Inst.getOperand(2).getImm();
      Target = Addr + Imm;
      return true;
    }

    if (Inst.getOpcode() == Primate::JAL) {
      Target = Addr + Inst.getOperand(1).getImm();
      return true;
    }

    return false;
  }
};

} // end anonymous namespace

static MCInstrAnalysis *createPrimateInstrAnalysis(const MCInstrInfo *Info) {
  return new PrimateMCInstrAnalysis(Info);
}

namespace {
MCStreamer *createPrimateELFStreamer(const Triple &T, MCContext &Context,
                                   std::unique_ptr<MCAsmBackend> &&MAB,
                                   std::unique_ptr<MCObjectWriter> &&MOW,
                                   std::unique_ptr<MCCodeEmitter> &&MCE,
                                   bool RelaxAll) {
  return createPrimateELFStreamer(Context, std::move(MAB), std::move(MOW),
                                std::move(MCE), RelaxAll);
}
} // end anonymous namespace

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePrimateTargetMC() {
  for (Target *T : {&getThePrimate32Target(), &getThePrimate64Target()}) {
    TargetRegistry::RegisterMCAsmInfo(*T, createPrimateMCAsmInfo);
    TargetRegistry::RegisterMCInstrInfo(*T, createPrimateMCInstrInfo);
    TargetRegistry::RegisterMCRegInfo(*T, createPrimateMCRegisterInfo);
    TargetRegistry::RegisterMCAsmBackend(*T, createPrimateAsmBackend);
    TargetRegistry::RegisterMCCodeEmitter(*T, createPrimateMCCodeEmitter);
    TargetRegistry::RegisterMCInstPrinter(*T, createPrimateMCInstPrinter);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createPrimateMCSubtargetInfo);
    TargetRegistry::RegisterELFStreamer(*T, createPrimateELFStreamer);
    TargetRegistry::RegisterObjectTargetStreamer(
        *T, createPrimateObjectTargetStreamer);
    TargetRegistry::RegisterMCInstrAnalysis(*T, createPrimateInstrAnalysis);

    // Register the asm target streamer.
    TargetRegistry::RegisterAsmTargetStreamer(*T, createPrimateAsmTargetStreamer);
    // Register the null target streamer.
    TargetRegistry::RegisterNullTargetStreamer(*T,
                                               createPrimateNullTargetStreamer);
  }
}
