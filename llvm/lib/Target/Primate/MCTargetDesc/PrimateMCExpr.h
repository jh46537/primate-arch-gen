//===-- PrimateMCExpr.h - Primate specific MC expression classes ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes Primate-specific MCExprs, used for modifiers like
// "%hi" or "%lo" etc.,
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEMCEXPR_H
#define LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEMCEXPR_H

#include "llvm/MC/MCExpr.h"

namespace llvm {

class StringRef;

class PrimateMCExpr : public MCTargetExpr {
public:
  enum VariantKind {
    VK_Primate_None,
    VK_Primate_LO,
    VK_Primate_HI,
    VK_Primate_PCREL_LO,
    VK_Primate_PCREL_HI,
    VK_Primate_GOT_HI,
    VK_Primate_TPREL_LO,
    VK_Primate_TPREL_HI,
    VK_Primate_TPREL_ADD,
    VK_Primate_TLS_GOT_HI,
    VK_Primate_TLS_GD_HI,
    VK_Primate_CALL,
    VK_Primate_CALL_PLT,
    VK_Primate_32_PCREL,
    VK_Primate_Invalid // Must be the last item
  };

private:
  const MCExpr *Expr;
  const VariantKind Kind;

  int64_t evaluateAsInt64(int64_t Value) const;

  explicit PrimateMCExpr(const MCExpr *Expr, VariantKind Kind)
      : Expr(Expr), Kind(Kind) {}

public:
  static const PrimateMCExpr *create(const MCExpr *Expr, VariantKind Kind,
                                   MCContext &Ctx);

  VariantKind getKind() const { return Kind; }

  const MCExpr *getSubExpr() const { return Expr; }

  /// Get the corresponding PC-relative HI fixup that a VK_Primate_PCREL_LO
  /// points to, and optionally the fragment containing it.
  ///
  /// \returns nullptr if this isn't a VK_Primate_PCREL_LO pointing to a
  /// known PC-relative HI fixup.
  const MCFixup *getPCRelHiFixup(const MCFragment **DFOut) const;

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;
  bool evaluateAsRelocatableImpl(MCValue &Res, const MCAsmLayout *Layout,
                                 const MCFixup *Fixup) const override;
  void visitUsedExpr(MCStreamer &Streamer) const override;
  MCFragment *findAssociatedFragment() const override {
    return getSubExpr()->findAssociatedFragment();
  }

  void fixELFSymbolsInTLSFixups(MCAssembler &Asm) const override;

  bool evaluateAsConstant(int64_t &Res) const;

  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }

  static bool classof(const PrimateMCExpr *) { return true; }

  static VariantKind getVariantKindForName(StringRef name);
  static StringRef getVariantKindName(VariantKind Kind);
};

} // end namespace llvm.

#endif
