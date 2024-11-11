//===-- PrimateAsmBackend.cpp - Primate Assembler Backend ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PrimateAsmBackend.h"
#include "PrimateMCExpr.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <iterator>

using namespace llvm;

std::optional<MCFixupKind> PrimateAsmBackend::getFixupKind(StringRef Name) const {
  if (STI.getTargetTriple().isOSBinFormatELF()) {
    unsigned Type;
    Type = llvm::StringSwitch<unsigned>(Name)
#define ELF_RELOC(X, Y) .Case(#X, Y)
#include "llvm/BinaryFormat/ELFRelocs/Primate.def"
#undef ELF_RELOC
               .Case("BFD_RELOC_NONE", ELF::R_PRIMATE_NONE)
               .Case("BFD_RELOC_32", ELF::R_PRIMATE_32)
               .Case("BFD_RELOC_64", ELF::R_PRIMATE_64)
               .Default(-1u);
    if (Type != -1u)
      return static_cast<MCFixupKind>(FirstLiteralRelocationKind + Type);
  }
  return {};
}

const MCFixupKindInfo &
PrimateAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  #include "PrimateInstructionSize.inc"
  const unsigned regDelta = regFieldBitWidth - 5;
  const static MCFixupKindInfo Infos[] = {
      // This table *must* be in the order that the fixup_* kinds are defined in
      // PrimateFixupKinds.h.
      //
      // name                      offset bits  flags
      {"fixup_primate_hi20", (12+regDelta), 20, 0},
      {"fixup_primate_lo12_i", (20+(regDelta*2)), 12, 0},
      {"fixup_primate_lo12_s", 0, 32, 0},
      {"fixup_primate_pcrel_hi20", 12+regDelta, 20,
       MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_IsTarget},
      {"fixup_primate_pcrel_lo12_i", 20+(2*regDelta), 12,
       MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_IsTarget},
      {"fixup_primate_pcrel_lo12_s", 0, 32,
       MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_IsTarget},
      {"fixup_primate_got_hi20", 12+regDelta, 20, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_tprel_hi20", 12+regDelta, 20, 0},
      {"fixup_primate_tprel_lo12_i", 20+(2*regDelta), 12, 0},
      {"fixup_primate_tprel_lo12_s", 0, 32, 0},
      {"fixup_primate_tprel_add", 0, 0, 0},
      {"fixup_primate_tls_got_hi20", 12, 20, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_tls_gd_hi20", 12, 20, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_jal", 12+regDelta, 20, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_branch", 0, 32+(2*regDelta), MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_prc_jump", 2, 11, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_prc_branch", 0, 16, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_call", 0, 64, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_call_plt", 0, 64, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_primate_relax", 0, 0, 0},
      {"fixup_primate_align", 0, 0, 0},

      {"fixup_primate_set_8", 0, 8, 0},
      {"fixup_primate_add_8", 0, 8, 0},
      {"fixup_primate_sub_8", 0, 8, 0},

      {"fixup_primate_set_16", 0, 16, 0},
      {"fixup_primate_add_16", 0, 16, 0},
      {"fixup_primate_sub_16", 0, 16, 0},

      {"fixup_primate_set_32", 0, 32, 0},
      {"fixup_primate_add_32", 0, 32, 0},
      {"fixup_primate_sub_32", 0, 32, 0},

      {"fixup_primate_add_64", 0, 64, 0},
      {"fixup_primate_sub_64", 0, 64, 0},

      {"fixup_primate_set_6b", 2, 6, 0},
      {"fixup_primate_sub_6b", 2, 6, 0},
  };
  static_assert((std::size(Infos)) == Primate::NumTargetFixupKinds,
                "Not all fixup kinds added to Infos array");

  // Fixup kinds from .reloc directive are like R_Primate_NONE. They
  // do not require any extra processing.
  if (Kind >= FirstLiteralRelocationKind)
    return MCAsmBackend::getFixupKindInfo(FK_NONE);

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

// If linker relaxation is enabled, or the relax option had previously been
// enabled, always emit relocations even if the fixup can be resolved. This is
// necessary for correctness as offsets may change during relaxation.
bool PrimateAsmBackend::shouldForceRelocation(const MCAssembler &Asm, 
                                              const MCFixup &Fixup,
                                              const MCValue &Target, 
                                              const MCSubtargetInfo *STI) {
  if (Fixup.getKind() >= FirstLiteralRelocationKind)
    return true;
  switch (Fixup.getTargetKind()) {
  default:
    break;
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    if (Target.isAbsolute())
      return false;
    break;
  case Primate::fixup_primate_got_hi20:
  case Primate::fixup_primate_tls_got_hi20:
  case Primate::fixup_primate_tls_gd_hi20:
    return true;
  }

  return STI->getFeatureBits()[Primate::FeatureRelax] || ForceRelocs;
}

bool PrimateAsmBackend::fixupNeedsRelaxationAdvanced(const MCFixup &Fixup,
                                                   bool Resolved,
                                                   uint64_t Value,
                                                   const MCRelaxableFragment *DF,
                                                   const MCAsmLayout &Layout,
                                                   const bool WasForced) const {
  // Return true if the symbol is actually unresolved.
  // Resolved could be always false when shouldForceRelocation return true.
  // We use !WasForced to indicate that the symbol is unresolved and not forced
  // by shouldForceRelocation.
  if (!Resolved && !WasForced)
    return true;

  int64_t Offset = int64_t(Value);
  switch (Fixup.getTargetKind()) {
  default:
    return false;
  case Primate::fixup_primate_prc_branch:
    // For compressed branch instructions the immediate must be
    // in the range [-256, 254].
    return Offset > 254 || Offset < -256;
  case Primate::fixup_primate_prc_jump:
    // For compressed jump instructions the immediate must be
    // in the range [-2048, 2046].
    return Offset > 2046 || Offset < -2048;
  }
}

void PrimateAsmBackend::relaxInstruction(MCInst &Inst,
                                         const MCSubtargetInfo &STI) const {
  // TODO: replace this with call to auto generated uncompressinstr() function.
  MCInst Res;
  switch (Inst.getOpcode()) {
  default:
    llvm_unreachable("Opcode not expected!");
  }
  Inst = std::move(Res);
}

bool PrimateAsmBackend::relaxDwarfLineAddr(MCDwarfLineAddrFragment &DF,
                                         MCAsmLayout &Layout,
                                         bool &WasRelaxed) const {
  MCContext &C = Layout.getAssembler().getContext();

  int64_t LineDelta = DF.getLineDelta();
  const MCExpr &AddrDelta = DF.getAddrDelta();
  SmallVectorImpl<char> &Data = DF.getContents();
  SmallVectorImpl<MCFixup> &Fixups = DF.getFixups();
  size_t OldSize = Data.size();

  int64_t Value;
  bool IsAbsolute = AddrDelta.evaluateKnownAbsolute(Value, Layout);
  assert(IsAbsolute && "CFA with invalid expression");
  (void)IsAbsolute;

  Data.clear();
  Fixups.clear();
  raw_svector_ostream OS(Data);

  // INT64_MAX is a signal that this is actually a DW_LNE_end_sequence.
  if (LineDelta != INT64_MAX) {
    OS << uint8_t(dwarf::DW_LNS_advance_line);
    encodeSLEB128(LineDelta, OS);
  }

  unsigned Offset;
  std::pair<unsigned, unsigned> Fixup;

  // According to the DWARF specification, the `DW_LNS_fixed_advance_pc` opcode
  // takes a single unsigned half (unencoded) operand. The maximum encodable
  // value is therefore 65535.  Set a conservative upper bound for relaxation.
  if (Value > 60000) {
    unsigned PtrSize = C.getAsmInfo()->getCodePointerSize();

    OS << uint8_t(dwarf::DW_LNS_extended_op);
    encodeULEB128(PtrSize + 1, OS);

    OS << uint8_t(dwarf::DW_LNE_set_address);
    Offset = OS.tell();
    Fixup = PtrSize == 4 ? std::make_pair(Primate::fixup_primate_add_32,
                                          Primate::fixup_primate_sub_32)
                         : std::make_pair(Primate::fixup_primate_add_64,
                                          Primate::fixup_primate_sub_64);
    OS.write_zeros(PtrSize);
  } else {
    OS << uint8_t(dwarf::DW_LNS_fixed_advance_pc);
    Offset = OS.tell();
    Fixup = {Primate::fixup_primate_add_16, Primate::fixup_primate_sub_16};
    support::endian::write<uint16_t>(OS, 0, llvm::endianness::little);
  }

  const MCBinaryExpr &MBE = cast<MCBinaryExpr>(AddrDelta);
  Fixups.push_back(MCFixup::create(
      Offset, MBE.getLHS(), static_cast<MCFixupKind>(std::get<0>(Fixup))));
  Fixups.push_back(MCFixup::create(
      Offset, MBE.getRHS(), static_cast<MCFixupKind>(std::get<1>(Fixup))));

  if (LineDelta == INT64_MAX) {
    OS << uint8_t(dwarf::DW_LNS_extended_op);
    OS << uint8_t(1);
    OS << uint8_t(dwarf::DW_LNE_end_sequence);
  } else {
    OS << uint8_t(dwarf::DW_LNS_copy);
  }

  WasRelaxed = OldSize != Data.size();
  return true;
}

bool PrimateAsmBackend::relaxDwarfCFA(MCDwarfCallFrameFragment &DF,
                                    MCAsmLayout &Layout,
                                    bool &WasRelaxed) const {

  const MCExpr &AddrDelta = DF.getAddrDelta();
  SmallVectorImpl<char> &Data = DF.getContents();
  SmallVectorImpl<MCFixup> &Fixups = DF.getFixups();
  size_t OldSize = Data.size();

  int64_t Value;
  bool IsAbsolute = AddrDelta.evaluateKnownAbsolute(Value, Layout);
  assert(IsAbsolute && "CFA with invalid expression");
  (void)IsAbsolute;

  Data.clear();
  Fixups.clear();
  raw_svector_ostream OS(Data);

  assert(
      Layout.getAssembler().getContext().getAsmInfo()->getMinInstAlignment() ==
          1 &&
      "expected 1-byte alignment");
  if (Value == 0) {
    WasRelaxed = OldSize != Data.size();
    return true;
  }

  auto AddFixups = [&Fixups, &AddrDelta](unsigned Offset,
                                         std::pair<unsigned, unsigned> Fixup) {
    const MCBinaryExpr &MBE = cast<MCBinaryExpr>(AddrDelta);
    Fixups.push_back(MCFixup::create(
        Offset, MBE.getLHS(), static_cast<MCFixupKind>(std::get<0>(Fixup))));
    Fixups.push_back(MCFixup::create(
        Offset, MBE.getRHS(), static_cast<MCFixupKind>(std::get<1>(Fixup))));
  };

  if (isUIntN(6, Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc);
    AddFixups(0, {Primate::fixup_primate_set_6b, Primate::fixup_primate_sub_6b});
  } else if (isUInt<8>(Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc1);
    support::endian::write<uint8_t>(OS, 0, llvm::endianness::little);
    AddFixups(1, {Primate::fixup_primate_set_8, Primate::fixup_primate_sub_8});
  } else if (isUInt<16>(Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc2);
    support::endian::write<uint16_t>(OS, 0, llvm::endianness::little);
    AddFixups(1, {Primate::fixup_primate_set_16, Primate::fixup_primate_sub_16});
  } else if (isUInt<32>(Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc4);
    support::endian::write<uint32_t>(OS, 0, llvm::endianness::little);
    AddFixups(1, {Primate::fixup_primate_set_32, Primate::fixup_primate_sub_32});
  } else {
    llvm_unreachable("unsupported CFA encoding");
  }

  WasRelaxed = OldSize != Data.size();
  return true;
}

// used to give a compressed instruction. I don't want to support that
// so now it looks like this.
unsigned PrimateAsmBackend::getRelaxedOpcode(unsigned Op) const {
  switch (Op) {
  default:
    return Op;
  }
}

bool PrimateAsmBackend::mayNeedRelaxation(const MCInst &Inst,
                                        const MCSubtargetInfo &STI) const {
  return getRelaxedOpcode(Inst.getOpcode()) != Inst.getOpcode();
}

bool PrimateAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count, const MCSubtargetInfo *STI) const {
  bool HasStdExtC = STI->getFeatureBits()[Primate::FeatureStdExtC];
  unsigned MinNopLen = HasStdExtC ? 2 : 4;

  if ((Count % MinNopLen) != 0)
    return false;

  // The canonical nop on Primate is addi x0, x0, 0.
  for (; Count >= 4; Count -= 4)
    OS.write("\x13\0\0\0", 4);

  // The canonical nop on PRC is c.nop.
  if (Count && HasStdExtC)
    OS.write("\x01\0", 2);

  return true;
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx, const MCSubtargetInfo *STI) {

  // byte offset to VLIW instr offset.
  dbgs() << "Value pre: " << Value << "\n";
  // lmao dirty hack
  #include "PrimateInstructionSize.inc"
  unsigned const numResourceGroups = STI->getSchedModel().NumProcResourceKinds;
  auto const& lastResourceGroup = STI->getSchedModel().ProcResourceTable[numResourceGroups-1];
  unsigned const numSlots = lastResourceGroup.NumUnits;
  int64_t signedValue = (static_cast<int64_t>(Value) + ((numSlots-1) * instrSize)) / (numSlots * instrSize);


  // insert into the instr
  switch (Fixup.getTargetKind()) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case Primate::fixup_primate_got_hi20:
  case Primate::fixup_primate_tls_got_hi20:
  case Primate::fixup_primate_tls_gd_hi20:
    llvm_unreachable("Relocation should be unconditionally forced\n");
  case Primate::fixup_primate_set_8:
  case Primate::fixup_primate_add_8:
  case Primate::fixup_primate_sub_8:
  case Primate::fixup_primate_set_16:
  case Primate::fixup_primate_add_16:
  case Primate::fixup_primate_sub_16:
  case Primate::fixup_primate_set_32:
  case Primate::fixup_primate_add_32:
  case Primate::fixup_primate_sub_32:
  case Primate::fixup_primate_add_64:
  case Primate::fixup_primate_sub_64:
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    return signedValue;
  case Primate::fixup_primate_set_6b:
    return signedValue & 0x03;
  case Primate::fixup_primate_lo12_i:
  case Primate::fixup_primate_pcrel_lo12_i:
  case Primate::fixup_primate_tprel_lo12_i:
    return signedValue & 0xfff;
  case Primate::fixup_primate_lo12_s:
  case Primate::fixup_primate_pcrel_lo12_s:
  case Primate::fixup_primate_tprel_lo12_s:
    return (((signedValue >> 5) & 0x7f) << 25) | ((signedValue & 0x1f) << 7);
  case Primate::fixup_primate_hi20:
  case Primate::fixup_primate_pcrel_hi20:
  case Primate::fixup_primate_tprel_hi20:
    // Add 1 if bit 11 is 1, to compensate for low 12 bits being negative.
    return ((signedValue + 0x800) >> 12) & 0xfffff;
  case Primate::fixup_primate_jal: {
    if (!isInt<20>(signedValue)) {
      dbgs() << "Failed to fit a jal.\n";
      dbgs() << "Slot Num: " << numSlots << "\n";
      dbgs() << "Value: " << signedValue << "\n";
      dbgs() << "instr bytes: " << numSlots*4 << "\n";
	
      Ctx.reportError(Fixup.getLoc(), "fixup value out of range for JAL");
    }
    // Need to produce imm[19|10:1|11|19:12] from the 21-bit Value.
    uint64_t Sbit = (signedValue >> 19) & 0x1;
    uint64_t Hi8  = (signedValue >> 11) & 0xff;
    uint64_t Mid1 = (signedValue >> 10) & 0x1;
    uint64_t Lo10 = (signedValue >> 0) & 0x3ff;
    // Inst{31} = Sbit;
    // Inst{30-21} = Lo10;
    // Inst{20} = Mid1;
    // Inst{19-12} = Hi8;
    signedValue = (Sbit << 19) | (Lo10 << 9) | (Mid1 << 8) | Hi8;
    return signedValue;
  }
  case Primate::fixup_primate_branch: {

    if (!isInt<12>(signedValue))
      Ctx.reportError(Fixup.getLoc(), "fixup value out of range for Branch");
    // Need to extract imm[12], imm[10:5], imm[4:1], imm[11] from the 13-bit
    // Value.
    uint64_t Sbit = (signedValue >> 11) & 0x1;
    uint64_t Hi1  = (signedValue >> 10) & 0x1;
    uint64_t Mid6 = (signedValue >> 4) & 0x3f;
    uint64_t Lo4  = (signedValue >> 0) & 0xf;
    // Inst{31} = Sbit;
    // Inst{30-25} = Mid6;
    // Inst{11-8} = Lo4;
    // Inst{7} = Hi1;
    signedValue = (Sbit << (31 + (3*(regFieldBitWidth - 5)))) | (Mid6 << (25 + (3*(regFieldBitWidth - 5)))) | (Lo4 << 8) | (Hi1 << 7);
    dbgs() << "fixups value post mangle: " << signedValue << "\n";
    return signedValue;
  }
  case Primate::fixup_primate_call:
  case Primate::fixup_primate_call_plt: {
    // Jalr will add UpperImm with the sign-extended 12-bit LowerImm,
    // we need to add 0x800ULL before extract upper bits to reflect the
    // effect of the sign extension.
    uint64_t UpperImm = (signedValue + 0x800ULL) & 0xfffff000ULL;
    uint64_t LowerImm = signedValue & 0xfffULL;
    return UpperImm | ((LowerImm << 20) << 32);
  }
  case Primate::fixup_primate_prc_jump: {
    // changed to support jump by 1
    // Need to produce offset[11|4|9:8|10|6|7|3:1|5] from the 11-bit Value.
    uint64_t Bit11  = (signedValue >> 10) & 0x1;
    uint64_t Bit4   = (signedValue >> 3) & 0x1;
    uint64_t Bit9_8 = (signedValue >> 7) & 0x3;
    uint64_t Bit10  = (signedValue >> 9) & 0x1;
    uint64_t Bit6   = (signedValue >> 5) & 0x1;
    uint64_t Bit7   = (signedValue >> 6) & 0x1;
    uint64_t Bit3_1 = (signedValue >> 0) & 0x7;
    uint64_t Bit5   = (signedValue >> 4) & 0x1;
    signedValue = (Bit11 << 10) | (Bit4 << 9) | (Bit9_8 << 7) | (Bit10 << 6) |
                  (Bit6 << 5) | (Bit7 << 4) | (Bit3_1 << 1) | Bit5;
    return signedValue;
  }
  case Primate::fixup_primate_prc_branch: {
    // changed to support jump by 1
    // Need to produce offset[8|4:3], [reg 3 bit], offset[7:6|2:1|5]
    uint64_t Bit8   = (signedValue >> 7) & 0x1;
    uint64_t Bit7_6 = (signedValue >> 5) & 0x3;
    uint64_t Bit5   = (signedValue >> 4) & 0x1;
    uint64_t Bit4_3 = (signedValue >> 2) & 0x3;
    uint64_t Bit2_1 = (signedValue >> 0) & 0x3;
    signedValue = (Bit8 << 12) | (Bit4_3 << 10) | (Bit7_6 << 5) | (Bit2_1 << 3) |
                  (Bit5 << 2);
    return signedValue;
  }

  }
}

bool PrimateAsmBackend::evaluateTargetFixup(
    const MCAssembler &Asm,
    const MCAsmLayout &Layout,
    const MCFixup &Fixup, const MCFragment *DF,
    const MCValue &Target,
    const MCSubtargetInfo *STI, uint64_t &Value,
    bool &WasForced) {
  const MCFixup *AUIPCFixup;
  const MCFragment *AUIPCDF;
  MCValue AUIPCTarget;
  switch (Fixup.getTargetKind()) {
  default:
    llvm_unreachable("Unexpected fixup kind!");
  case Primate::fixup_primate_pcrel_hi20:
    AUIPCFixup = &Fixup;
    AUIPCDF = DF;
    AUIPCTarget = Target;
    break;
  case Primate::fixup_primate_pcrel_lo12_i:
  case Primate::fixup_primate_pcrel_lo12_s: {
    AUIPCFixup = cast<PrimateMCExpr>(Fixup.getValue())->getPCRelHiFixup(&AUIPCDF);
    if (!AUIPCFixup) {
      Asm.getContext().reportError(Fixup.getLoc(),
                                   "could not find corresponding %pcrel_hi");
      return true;
    }

    // MCAssembler::evaluateFixup will emit an error for this case when it sees
    // the %pcrel_hi, so don't duplicate it when also seeing the %pcrel_lo.
    const MCExpr *AUIPCExpr = AUIPCFixup->getValue();
    if (!AUIPCExpr->evaluateAsRelocatable(AUIPCTarget, &Layout, AUIPCFixup))
      return true;
    break;
  }
  }

  if (!AUIPCTarget.getSymA() || AUIPCTarget.getSymB())
    return false;

  const MCSymbolRefExpr *A = AUIPCTarget.getSymA();
  const MCSymbol &SA = A->getSymbol();
  if (A->getKind() != MCSymbolRefExpr::VK_None || SA.isUndefined())
    return false;

  auto *Writer = Asm.getWriterPtr();
  if (!Writer)
    return false;

  bool IsResolved = Writer->isSymbolRefDifferenceFullyResolvedImpl(
      Asm, SA, *AUIPCDF, false, true);
  if (!IsResolved)
    return false;

  Value = Layout.getSymbolOffset(SA) + AUIPCTarget.getConstant();
  Value -= Layout.getFragmentOffset(AUIPCDF) + AUIPCFixup->getOffset();

  if (shouldForceRelocation(Asm, *AUIPCFixup, AUIPCTarget, STI)) {
    WasForced = true;
    return false;
  }

  return true;
}

void PrimateAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                 const MCValue &Target,
                                 MutableArrayRef<char> Data, uint64_t Value,
                                 bool IsResolved,
                                 const MCSubtargetInfo *STI) const {
  MCFixupKind Kind = Fixup.getKind();
  if (Kind >= FirstLiteralRelocationKind)
    return;
  MCContext &Ctx = Asm.getContext();
  MCFixupKindInfo Info = getFixupKindInfo(Kind);
  if (!Value)
    return; // Doesn't change encoding.
  // Apply any target-specific value adjustments.
  Value = adjustFixupValue(Fixup, Value, Ctx, STI);

  // Shift the value into position.
  Value <<= Info.TargetOffset;

  unsigned Offset = Fixup.getOffset();
  unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;

  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  for (unsigned i = 0; i != NumBytes; ++i) {
    Data[Offset + i] |= uint8_t((Value >> (i * 8)) & 0xff);
  }
}

// Linker relaxation may change code size. We have to insert Nops
// for .align directive when linker relaxation enabled. So then Linker
// could satisfy alignment by removing Nops.
// The function return the total Nops Size we need to insert.
bool PrimateAsmBackend::shouldInsertExtraNopBytesForCodeAlign(
    const MCAlignFragment &AF, unsigned &Size) {
  // Calculate Nops Size only when linker relaxation enabled.
  if (!STI.getFeatureBits()[Primate::FeatureRelax]) {
    dbgs() << "shouldInsertExtraNopBytesForCodeAlign: FeatureRelax is not enabled\n";
    return false;
  }

  unsigned MinNopLen = 6;

  if (AF.getAlignment() <= MinNopLen) {
    return false;
  } else {
    Size = AF.getAlignment().value() - MinNopLen;
    return true;
  }
}

// We need to insert R_Primate_ALIGN relocation type to indicate the
// position of Nops and the total bytes of the Nops have been inserted
// when linker relaxation enabled.
// The function insert fixup_primate_align fixup which eventually will
// transfer to R_Primate_ALIGN relocation type.
bool PrimateAsmBackend::shouldInsertFixupForCodeAlign(MCAssembler &Asm,
                                                    const MCAsmLayout &Layout,
                                                    MCAlignFragment &AF) {
  // Insert the fixup only when linker relaxation enabled.
  if (!STI.getFeatureBits()[Primate::FeatureRelax]) {
    dbgs() << "shouldInsertFixupForCodeAlign: FeatureRelax is not enabled\n";
    return false;
  }

  // Calculate total Nops we need to insert. If there are none to insert
  // then simply return.
  unsigned Count;
  if (!shouldInsertExtraNopBytesForCodeAlign(AF, Count) || (Count == 0))
    return false;

  MCContext &Ctx = Asm.getContext();
  const MCExpr *Dummy = MCConstantExpr::create(0, Ctx);
  // Create fixup_primate_align fixup.
  MCFixup Fixup =
      MCFixup::create(0, Dummy, MCFixupKind(Primate::fixup_primate_align), SMLoc());

  uint64_t FixedValue = 0;
  MCValue NopBytes = MCValue::get(Count);

  Asm.getWriter().recordRelocation(Asm, Layout, &AF, Fixup, NopBytes,
                                   FixedValue);

  return true;
}

std::unique_ptr<MCObjectTargetWriter>
PrimateAsmBackend::createObjectTargetWriter() const {
  // return createPrimateHarvardObjectWriter(OSABI, Is64Bit);
  return createPrimateELFObjectWriter(OSABI, Is64Bit);
}

MCAsmBackend *llvm::createPrimateAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  auto* ret = new PrimateAsmBackend(STI, OSABI, TT.isArch64Bit(), Options);
  return ret;
}
