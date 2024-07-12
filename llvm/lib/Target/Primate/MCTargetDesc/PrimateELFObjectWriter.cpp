//===-- PrimateELFObjectWriter.cpp - Primate ELF Writer -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/PrimateFixupKinds.h"
#include "MCTargetDesc/PrimateMCExpr.h"
#include "MCTargetDesc/PrimateMCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class PrimateELFObjectWriter : public MCELFObjectTargetWriter {
public:
  PrimateELFObjectWriter(uint8_t OSABI, bool Is64Bit);

  ~PrimateELFObjectWriter() override;

  // Return true if the given relocation must be with a symbol rather than
  // section plus offset.
  virtual bool needsRelocateWithSymbol(const MCValue &Val, const MCSymbol &Sym,
                               unsigned Type) const override {
    // TODO: this is very conservative, update once Primate psABI requirements
    //       are clarified.
    return true;
  }

protected:
  virtual unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;
};
}

PrimateELFObjectWriter::PrimateELFObjectWriter(uint8_t OSABI, bool Is64Bit)
    : MCELFObjectTargetWriter(Is64Bit, OSABI, ELF::EM_PRIMATE,
                              /*HasRelocationAddend*/ true) {}

PrimateELFObjectWriter::~PrimateELFObjectWriter() {}

unsigned PrimateELFObjectWriter::getRelocType(MCContext &Ctx,
                                            const MCValue &Target,
                                            const MCFixup &Fixup,
                                            bool IsPCRel) const {
  const MCExpr *Expr = Fixup.getValue();
  // Determine the type of the relocation
  unsigned Kind = Fixup.getTargetKind();
  if (Kind >= FirstLiteralRelocationKind)
    return Kind - FirstLiteralRelocationKind;
  if (IsPCRel) {
    switch (Kind) {
    default:
      Ctx.reportError(Fixup.getLoc(), "Unsupported relocation type");
      return ELF::R_PRIMATE_NONE;
    case FK_Data_4:
    case FK_PCRel_4:
      return ELF::R_PRIMATE_32_PCREL;
    case Primate::fixup_primate_pcrel_hi20:
      return ELF::R_PRIMATE_PCREL_HI20;
    case Primate::fixup_primate_pcrel_lo12_i:
      return ELF::R_PRIMATE_PCREL_LO12_I;
    case Primate::fixup_primate_pcrel_lo12_s:
      return ELF::R_PRIMATE_PCREL_LO12_S;
    case Primate::fixup_primate_got_hi20:
      return ELF::R_PRIMATE_GOT_HI20;
    case Primate::fixup_primate_tls_got_hi20:
      return ELF::R_PRIMATE_TLS_GOT_HI20;
    case Primate::fixup_primate_tls_gd_hi20:
      return ELF::R_PRIMATE_TLS_GD_HI20;
    case Primate::fixup_primate_jal:
      return ELF::R_PRIMATE_JAL;
    case Primate::fixup_primate_branch:
      return ELF::R_PRIMATE_BRANCH;
    case Primate::fixup_primate_prc_jump:
      return ELF::R_PRIMATE_PRC_JUMP;
    case Primate::fixup_primate_prc_branch:
      return ELF::R_PRIMATE_PRC_BRANCH;
    case Primate::fixup_primate_call:
      return ELF::R_PRIMATE_CALL;
    case Primate::fixup_primate_call_plt:
      return ELF::R_PRIMATE_CALL_PLT;
    case Primate::fixup_primate_add_8:
      return ELF::R_PRIMATE_ADD8;
    case Primate::fixup_primate_sub_8:
      return ELF::R_PRIMATE_SUB8;
    case Primate::fixup_primate_add_16:
      return ELF::R_PRIMATE_ADD16;
    case Primate::fixup_primate_sub_16:
      return ELF::R_PRIMATE_SUB16;
    case Primate::fixup_primate_add_32:
      return ELF::R_PRIMATE_ADD32;
    case Primate::fixup_primate_sub_32:
      return ELF::R_PRIMATE_SUB32;
    case Primate::fixup_primate_add_64:
      return ELF::R_PRIMATE_ADD64;
    case Primate::fixup_primate_sub_64:
      return ELF::R_PRIMATE_SUB64;
    }
  }

  switch (Kind) {
  default:
    Ctx.reportError(Fixup.getLoc(), "Unsupported relocation type");
    return ELF::R_PRIMATE_NONE;
  case FK_Data_1:
    Ctx.reportError(Fixup.getLoc(), "1-byte data relocations not supported");
    return ELF::R_PRIMATE_NONE;
  case FK_Data_2:
    Ctx.reportError(Fixup.getLoc(), "2-byte data relocations not supported");
    return ELF::R_PRIMATE_NONE;
  case FK_Data_4:
    if (Expr->getKind() == MCExpr::Target &&
        cast<PrimateMCExpr>(Expr)->getKind() == PrimateMCExpr::VK_Primate_32_PCREL)
      return ELF::R_PRIMATE_32_PCREL;
    return ELF::R_PRIMATE_32;
  case FK_Data_8:
    return ELF::R_PRIMATE_64;
  case Primate::fixup_primate_hi20:
    return ELF::R_PRIMATE_HI20;
  case Primate::fixup_primate_lo12_i:
    return ELF::R_PRIMATE_LO12_I;
  case Primate::fixup_primate_lo12_s:
    return ELF::R_PRIMATE_LO12_S;
  case Primate::fixup_primate_tprel_hi20:
    return ELF::R_PRIMATE_TPREL_HI20;
  case Primate::fixup_primate_tprel_lo12_i:
    return ELF::R_PRIMATE_TPREL_LO12_I;
  case Primate::fixup_primate_tprel_lo12_s:
    return ELF::R_PRIMATE_TPREL_LO12_S;
  case Primate::fixup_primate_tprel_add:
    return ELF::R_PRIMATE_TPREL_ADD;
  case Primate::fixup_primate_relax:
    return ELF::R_PRIMATE_RELAX;
  case Primate::fixup_primate_align:
    return ELF::R_PRIMATE_ALIGN;
  case Primate::fixup_primate_set_6b:
    return ELF::R_PRIMATE_SET6;
  case Primate::fixup_primate_sub_6b:
    return ELF::R_PRIMATE_SUB6;
  case Primate::fixup_primate_add_8:
    return ELF::R_PRIMATE_ADD8;
  case Primate::fixup_primate_set_8:
    return ELF::R_PRIMATE_SET8;
  case Primate::fixup_primate_sub_8:
    return ELF::R_PRIMATE_SUB8;
  case Primate::fixup_primate_set_16:
    return ELF::R_PRIMATE_SET16;
  case Primate::fixup_primate_add_16:
    return ELF::R_PRIMATE_ADD16;
  case Primate::fixup_primate_sub_16:
    return ELF::R_PRIMATE_SUB16;
  case Primate::fixup_primate_set_32:
    return ELF::R_PRIMATE_SET32;
  case Primate::fixup_primate_add_32:
    return ELF::R_PRIMATE_ADD32;
  case Primate::fixup_primate_sub_32:
    return ELF::R_PRIMATE_SUB32;
  case Primate::fixup_primate_add_64:
    return ELF::R_PRIMATE_ADD64;
  case Primate::fixup_primate_sub_64:
    return ELF::R_PRIMATE_SUB64;
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createPrimateELFObjectWriter(uint8_t OSABI, bool Is64Bit) {
  return std::make_unique<PrimateELFObjectWriter>(OSABI, Is64Bit);
}
