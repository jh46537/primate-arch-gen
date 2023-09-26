//===-- PrimateTargetMachine.cpp - Define TargetMachine for Primate -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about Primate target spec.
//
//===----------------------------------------------------------------------===//

#include "PrimateTargetMachine.h"
#include "MCTargetDesc/PrimateBaseInfo.h"
#include "Primate.h"
#include "PrimateTargetObjectFile.h"
#include "PrimateTargetTransformInfo.h"
#include "PrimateGEPFilter.h"
#include "TargetInfo/PrimateTargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"
using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePrimateTarget() {
  RegisterTargetMachine<PrimateTargetMachine> X(getThePrimate32Target());
  RegisterTargetMachine<PrimateTargetMachine> Y(getThePrimate64Target());
  auto *PR = PassRegistry::getPassRegistry();
  initializePrimateStructToRegPassPass(*PR); // needs to run on IR with struct information
  initializeGlobalISel(*PR);
  initializePrimateMergeBaseOffsetOptPass(*PR);
  initializePrimateExpandPseudoPass(*PR);
  initializePrimateInsertVSETVLIPass(*PR);
  initializePrimatePacketizerPass(*PR);
}

static StringRef computeDataLayout(const Triple &TT) {
  if (TT.isArch64Bit())
    return "e-m:e-p:64:64-i64:64-i128:128-n64-S128";
  assert(TT.isArch32Bit() && "only PR32 and PR64 are currently supported");
  return "e-m:e-p:32:32-i64:64-n32-S128";
}

static Reloc::Model getEffectiveRelocModel(const Triple &TT,
                                           Optional<Reloc::Model> RM) {
  if (!RM.hasValue())
    return Reloc::Static;
  return *RM;
}

PrimateTargetMachine::PrimateTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       Optional<Reloc::Model> RM,
                                       Optional<CodeModel::Model> CM,
                                       CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(TT), TT, CPU, FS, Options,
                        getEffectiveRelocModel(TT, RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<PrimateELFTargetObjectFile>()) {
  initAsmInfo();

  // Primate supports the MachineOutliner.
  setMachineOutliner(true);
}

const PrimateSubtarget *
PrimateTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU =
      CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  std::string TuneCPU =
      TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
  std::string FS =
      FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;
  std::string Key = CPU + TuneCPU + FS;
  auto &I = SubtargetMap[Key];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    auto ABIName = Options.MCOptions.getABIName();
    if (const MDString *ModuleTargetABI = dyn_cast_or_null<MDString>(
            F.getParent()->getModuleFlag("target-abi"))) {
      auto TargetABI = PrimateABI::getTargetABI(ABIName);
      if (TargetABI != PrimateABI::ABI_Unknown &&
          ModuleTargetABI->getString() != ABIName) {
        report_fatal_error("-target-abi option != target-abi module flag");
      }
      ABIName = ModuleTargetABI->getString();
    }
    I = std::make_unique<PrimateSubtarget>(TargetTriple, CPU, TuneCPU, FS, ABIName, *this);
  }
  return I.get();
}

TargetTransformInfo
PrimateTargetMachine::getTargetTransformInfo(const Function &F) {
  return TargetTransformInfo(PrimateTTIImpl(this, F));
}

// A Primate hart has a single byte-addressable address space of 2^XLEN bytes
// for all memory accesses, so it is reasonable to assume that an
// implementation has no-op address space casts. If an implementation makes a
// change to this, they can override it here.
bool PrimateTargetMachine::isNoopAddrSpaceCast(unsigned SrcAS,
                                             unsigned DstAS) const {
  return true;
}

namespace {
class PrimatePassConfig : public TargetPassConfig {
public:
  PrimatePassConfig(PrimateTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  PrimateTargetMachine &getPrimateTargetMachine() const {
    return getTM<PrimateTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  bool addIRTranslator() override;
  bool addLegalizeMachineIR() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
  void addPreEmitPass() override;
  void addPreEmitPass2() override;
  void addPreSched2() override;
  void addPreRegAlloc() override;
};
} // namespace

TargetPassConfig *PrimateTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new PrimatePassConfig(*this, PM);
}

void PrimatePassConfig::addIRPasses() {
  addPass(createPrimateStructToRegPass());
  addPass(createAtomicExpandPass());
  TargetPassConfig::addIRPasses();
}

bool PrimatePassConfig::addInstSelector() {
  addPass(createPrimateISelDag(getPrimateTargetMachine()));

  return false;
}

bool PrimatePassConfig::addIRTranslator() {
  addPass(new IRTranslator(getOptLevel()));
  return false;
}

bool PrimatePassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer());
  return false;
}

bool PrimatePassConfig::addRegBankSelect() {
  addPass(new RegBankSelect());
  return false;
}

void PrimateTargetMachine::registerPassBuilderCallbacks(llvm::PassBuilder &PB) {
  PB.registerPeepholeEPCallback([](llvm::FunctionPassManager& FPM, llvm::PassBuilder::OptimizationLevel Level){
    FPM.addPass(llvm::PrimateGEPFilterPass());
    FPM.addPass(llvm::PrimateStructLoadCombinerPass());
  });
}

bool PrimatePassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelect(getOptLevel()));
  return false;
}

void PrimatePassConfig::addPreSched2() {}

void PrimatePassConfig::addPreEmitPass() {
  addPass(&BranchRelaxationPassID);
  addPass(createPrimatePacketizer(), false);
}

void PrimatePassConfig::addPreEmitPass2() {
  addPass(createPrimateExpandPseudoPass());
  // Schedule the expansion of AMOs at the last possible moment, avoiding the
  // possibility for other passes to break the requirements for forward
  // progress in the LR/SC block.
  addPass(createPrimateExpandAtomicPseudoPass());
}

void PrimatePassConfig::addPreRegAlloc() {
  if (TM->getOptLevel() != CodeGenOpt::None)
    addPass(createPrimateMergeBaseOffsetOptPass());

  // Convert structs to registers before any other reg alloc
  addPass(createPrimateInsertVSETVLIPass());
}
