//===-- PrimateAsmBackend.h - Primate Assembler Backend -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEASMBACKEND_H
#define LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEASMBACKEND_H

#include "MCTargetDesc/PrimateBaseInfo.h"
#include "MCTargetDesc/PrimateFixupKinds.h"
#include "MCTargetDesc/PrimateMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"

namespace llvm {
class MCAssembler;
class MCObjectTargetWriter;
class raw_ostream;

class PrimateAsmBackend : public MCAsmBackend {
  const MCSubtargetInfo &STI;
  uint8_t OSABI;
  bool Is64Bit;
  bool ForceRelocs = false;
  const MCTargetOptions &TargetOptions;
  PrimateABI::ABI TargetABI = PrimateABI::ABI_Unknown;
  const MCInstrInfo* TII;

public:
  PrimateAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI, bool Is64Bit,
                  const MCTargetOptions &Options)
      : MCAsmBackend(llvm::endianness::little), STI(STI), OSABI(OSABI), Is64Bit(Is64Bit),
        TargetOptions(Options) {
    TargetABI = PrimateABI::computeTargetABI(
        STI.getTargetTriple(), STI.getFeatureBits(), Options.getABIName());
    PrimateFeatures::validate(STI.getTargetTriple(), STI.getFeatureBits());
  }
  ~PrimateAsmBackend() override {}

  void setForceRelocs() { ForceRelocs = true; }

  // Return Size with extra Nop Bytes for alignment directive in code section.
  bool shouldInsertExtraNopBytesForCodeAlign(const MCAlignFragment &AF,
                                             unsigned &Size) override;

  // Insert target specific fixup type for alignment directive in code section.
  bool shouldInsertFixupForCodeAlign(MCAssembler &Asm,
                                     const MCAsmLayout &Layout,
                                     MCAlignFragment &AF) override;


  virtual bool evaluateTargetFixup(const MCAssembler &Asm,
                                   const MCAsmLayout &Layout,
                                   const MCFixup &Fixup, const MCFragment *DF,
                                   const MCValue &Target,
                                   const MCSubtargetInfo *STI, uint64_t &Value,
                                   bool &WasForced) override;

  virtual void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                          const MCValue &Target, MutableArrayRef<char> Data,
                          uint64_t Value, bool IsResolved,
                          const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override;

  virtual bool shouldForceRelocation(const MCAssembler &Asm, 
                                     const MCFixup &Fixup,
                                     const MCValue &Target, 
                                     const MCSubtargetInfo *STI) override;

  virtual bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    llvm_unreachable("Handled by fixupNeedsRelaxationAdvanced");
  }

  virtual bool fixupNeedsRelaxationAdvanced(const MCFixup &Fixup, bool Resolved,
                                            uint64_t Value,
                                            const MCRelaxableFragment *DF,
                                            const MCAsmLayout &Layout,
                                            const bool WasForced) const override;

  virtual unsigned getNumFixupKinds() const override {
    return Primate::NumTargetFixupKinds;
  }

  virtual std::optional<MCFixupKind> getFixupKind(StringRef Name) const override;

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;

  bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override;
  unsigned getRelaxedOpcode(unsigned Op) const;

  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override;

  bool relaxDwarfLineAddr(MCDwarfLineAddrFragment &DF, MCAsmLayout &Layout,
                          bool &WasRelaxed) const override;
  bool relaxDwarfCFA(MCDwarfCallFrameFragment &DF, MCAsmLayout &Layout,
                     bool &WasRelaxed) const override;

  virtual bool writeNopData(raw_ostream &OS, uint64_t Count, const MCSubtargetInfo *STI) const override;

  const MCTargetOptions &getTargetOptions() const { return TargetOptions; }
  PrimateABI::ABI getTargetABI() const { return TargetABI; }
};
}

#endif
