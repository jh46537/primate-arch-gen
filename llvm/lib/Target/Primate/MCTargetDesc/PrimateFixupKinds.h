//===-- PrimateFixupKinds.h - Primate Specific Fixup Entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEFIXUPKINDS_H
#define LLVM_LIB_TARGET_PRIMATE_MCTARGETDESC_PRIMATEFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

#undef Primate

namespace llvm {
namespace Primate {
enum Fixups {
  // 20-bit fixup corresponding to %hi(foo) for instructions like lui
  fixup_primate_hi20 = FirstTargetFixupKind,
  // 12-bit fixup corresponding to %lo(foo) for instructions like addi
  fixup_primate_lo12_i,
  // 12-bit fixup corresponding to %lo(foo) for the S-type store instructions
  fixup_primate_lo12_s,
  // 20-bit fixup corresponding to %pcrel_hi(foo) for instructions like auipc
  fixup_primate_pcrel_hi20,
  // 12-bit fixup corresponding to %pcrel_lo(foo) for instructions like addi
  fixup_primate_pcrel_lo12_i,
  // 12-bit fixup corresponding to %pcrel_lo(foo) for the S-type store
  // instructions
  fixup_primate_pcrel_lo12_s,
  // 20-bit fixup corresponding to %got_pcrel_hi(foo) for instructions like
  // auipc
  fixup_primate_got_hi20,
  // 20-bit fixup corresponding to %tprel_hi(foo) for instructions like lui
  fixup_primate_tprel_hi20,
  // 12-bit fixup corresponding to %tprel_lo(foo) for instructions like addi
  fixup_primate_tprel_lo12_i,
  // 12-bit fixup corresponding to %tprel_lo(foo) for the S-type store
  // instructions
  fixup_primate_tprel_lo12_s,
  // Fixup corresponding to %tprel_add(foo) for PseudoAddTPRel, used as a linker
  // hint
  fixup_primate_tprel_add,
  // 20-bit fixup corresponding to %tls_ie_pcrel_hi(foo) for instructions like
  // auipc
  fixup_primate_tls_got_hi20,
  // 20-bit fixup corresponding to %tls_gd_pcrel_hi(foo) for instructions like
  // auipc
  fixup_primate_tls_gd_hi20,
  // 20-bit fixup for symbol references in the jal instruction
  fixup_primate_jal,
  // 12-bit fixup for symbol references in the branch instructions
  fixup_primate_branch,
  // 11-bit fixup for symbol references in the compressed jump instruction
  fixup_primate_prc_jump,
  // 8-bit fixup for symbol references in the compressed branch instruction
  fixup_primate_prc_branch,
  // Fixup representing a legacy no-pic function call attached to the auipc
  // instruction in a pair composed of adjacent auipc+jalr instructions.
  fixup_primate_call,
  // Fixup representing a function call attached to the auipc instruction in a
  // pair composed of adjacent auipc+jalr instructions.
  fixup_primate_call_plt,
  // Used to generate an R_Primate_RELAX relocation, which indicates the linker
  // may relax the instruction pair.
  fixup_primate_relax,
  // Used to generate an R_Primate_ALIGN relocation, which indicates the linker
  // should fixup the alignment after linker relaxation.
  fixup_primate_align,
  // 8-bit fixup corresponding to R_Primate_SET8 for local label assignment.
  fixup_primate_set_8,
  // 8-bit fixup corresponding to R_Primate_ADD8 for 8-bit symbolic difference
  // paired relocations.
  fixup_primate_add_8,
  // 8-bit fixup corresponding to R_Primate_SUB8 for 8-bit symbolic difference
  // paired relocations.
  fixup_primate_sub_8,
  // 16-bit fixup corresponding to R_Primate_SET16 for local label assignment.
  fixup_primate_set_16,
  // 16-bit fixup corresponding to R_Primate_ADD16 for 16-bit symbolic difference
  // paired reloctions.
  fixup_primate_add_16,
  // 16-bit fixup corresponding to R_Primate_SUB16 for 16-bit symbolic difference
  // paired reloctions.
  fixup_primate_sub_16,
  // 32-bit fixup corresponding to R_Primate_SET32 for local label assignment.
  fixup_primate_set_32,
  // 32-bit fixup corresponding to R_Primate_ADD32 for 32-bit symbolic difference
  // paired relocations.
  fixup_primate_add_32,
  // 32-bit fixup corresponding to R_Primate_SUB32 for 32-bit symbolic difference
  // paired relocations.
  fixup_primate_sub_32,
  // 64-bit fixup corresponding to R_Primate_ADD64 for 64-bit symbolic difference
  // paired relocations.
  fixup_primate_add_64,
  // 64-bit fixup corresponding to R_Primate_SUB64 for 64-bit symbolic difference
  // paired relocations.
  fixup_primate_sub_64,
  // 6-bit fixup corresponding to R_Primate_SET6 for local label assignment in
  // DWARF CFA.
  fixup_primate_set_6b,
  // 6-bit fixup corresponding to R_Primate_SUB6 for local label assignment in
  // DWARF CFA.
  fixup_primate_sub_6b,

  // Used as a sentinel, must be the last
  fixup_primate_invalid,
  NumTargetFixupKinds = fixup_primate_invalid - FirstTargetFixupKind
};
} // end namespace Primate
} // end namespace llvm

#endif
