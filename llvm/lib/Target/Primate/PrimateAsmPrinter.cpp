//===-- PrimateAsmPrinter.cpp - Primate LLVM assembly writer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the Primate assembly language.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/PrimateInstPrinter.h"
#include "MCTargetDesc/PrimateMCExpr.h"
#include "MCTargetDesc/PrimateTargetStreamer.h"
#include "Primate.h"
#include "PrimateTargetMachine.h"
#include "TargetInfo/PrimateTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

STATISTIC(PrimateNumInstrsCompressed,
          "Number of Primate Compressed instructions emitted");

namespace {
class PrimateAsmPrinter : public AsmPrinter {
  const MCSubtargetInfo *STI;

public:
  explicit PrimateAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)), STI(TM.getMCSubtargetInfo()) {}

  StringRef getPassName() const override { return "Primate Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void emitInstruction(const MachineInstr *MI) override;

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;

  void EmitToStreamer(MCStreamer &S, const MCInst &Inst);
  bool emitPseudoExpansionLowering(MCStreamer &OutStreamer,
                                   const MachineInstr *MI);

  // Wrapper needed for tblgenned pseudo lowering.
  bool lowerOperand(const MachineOperand &MO, MCOperand &MCOp) const {
    return LowerPrimateMachineOperandToMCOperand(MO, MCOp, *this);
  }

  void emitStartOfAsmFile(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;

private:
  void emitAttributes();
};
}

#define GEN_COMPRESS_INSTR
#include "PrimateGenCompressInstEmitter.inc"
void PrimateAsmPrinter::EmitToStreamer(MCStreamer &S, const MCInst &Inst) {
  MCInst CInst;
  bool Res = compressInst(CInst, Inst, *STI, OutStreamer->getContext());
  if (Res)
    ++PrimateNumInstrsCompressed;
  AsmPrinter::EmitToStreamer(*OutStreamer, Res ? CInst : Inst);
}

// Simple pseudo-instructions have their lowering (with expansion to real
// instructions) auto-generated.
#include "PrimateGenMCPseudoLowering.inc"

void PrimateAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MCInst MCB;
  MCB.setOpcode(Primate::BUNDLE);
  MCB.addOperand(MCOperand::createImm(0));

  if (MI->isBundle()) {
    const MachineBasicBlock* MBB = MI->getParent();
    MachineBasicBlock::const_instr_iterator MII = MI->getIterator();

    // FIXME(ahsu):
    // make this an archgen param
    unsigned numSlots = 7;
    unsigned lastSlotIdx = 0;
    for (++MII; MII != MBB->instr_end() && MII->isInsideBundle(); ++MII) {
      if (!MII->isDebugInstr() && !MII->isImplicitDef()) {
        unsigned slotIdx = MII->getSlotIdx();
        // ignore instructions without slots
        if (slotIdx == (unsigned)-1)
          continue;
        if (slotIdx > lastSlotIdx) {
          emitNops(slotIdx - lastSlotIdx);
          lastSlotIdx = slotIdx;
        }
        ++lastSlotIdx;

        // Do any auto-generated pseudo lowerings.
        if (emitPseudoExpansionLowering(*OutStreamer, &*MII))
          return;

        MCInst TmpInst;
        if (!lowerPrimateMachineInstrToMCInst(&*MII, TmpInst, *this))
          EmitToStreamer(*OutStreamer, TmpInst);
      }
    }
    if (lastSlotIdx < numSlots)
      emitNops(numSlots - lastSlotIdx);
  } else {
    // Do any auto-generated pseudo lowerings.
    if (emitPseudoExpansionLowering(*OutStreamer, MI))
      return;

    MCInst TmpInst;
    if (!lowerPrimateMachineInstrToMCInst(MI, TmpInst, *this))
      EmitToStreamer(*OutStreamer, TmpInst);
  }

  // TODO(ahsu): insert post-processing canonicalization a la Hexagon
}

bool PrimateAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &OS) {
  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
    return false;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0)
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    case 'z':      // Print zero register if zero, regular printing otherwise.
      if (MO.isImm() && MO.getImm() == 0) {
        OS << PrimateInstPrinter::getRegisterName(Primate::X0);
        return false;
      }
      break;
    case 'i': // Literal 'i' if operand is not a register.
      if (!MO.isReg())
        OS << 'i';
      return false;
    }
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  case MachineOperand::MO_Register:
    OS << PrimateInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, OS);
    return false;
  case MachineOperand::MO_BlockAddress: {
    MCSymbol *Sym = GetBlockAddressSymbol(MO.getBlockAddress());
    Sym->print(OS, MAI);
    return false;
  }
  default:
    break;
  }

  return true;
}

bool PrimateAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                            unsigned OpNo,
                                            const char *ExtraCode,
                                            raw_ostream &OS) {
  if (!ExtraCode) {
    const MachineOperand &MO = MI->getOperand(OpNo);
    // For now, we only support register memory operands in registers and
    // assume there is no addend
    if (!MO.isReg())
      return true;

    OS << "0(" << PrimateInstPrinter::getRegisterName(MO.getReg()) << ")";
    return false;
  }

  return AsmPrinter::PrintAsmMemoryOperand(MI, OpNo, ExtraCode, OS);
}

bool PrimateAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  // Set the current MCSubtargetInfo to a copy which has the correct
  // feature bits for the current MachineFunction
  MCSubtargetInfo &NewSTI =
    OutStreamer->getContext().getSubtargetCopy(*TM.getMCSubtargetInfo());
  NewSTI.setFeatureBits(MF.getSubtarget().getFeatureBits());
  STI = &NewSTI;

  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

void PrimateAsmPrinter::emitStartOfAsmFile(Module &M) {
  if (TM.getTargetTriple().isOSBinFormatELF())
    emitAttributes();
}

void PrimateAsmPrinter::emitEndOfAsmFile(Module &M) {
  PrimateTargetStreamer &RTS =
      static_cast<PrimateTargetStreamer &>(*OutStreamer->getTargetStreamer());

  if (TM.getTargetTriple().isOSBinFormatELF())
    RTS.finishAttributeSection();
}

void PrimateAsmPrinter::emitAttributes() {
  PrimateTargetStreamer &RTS =
      static_cast<PrimateTargetStreamer &>(*OutStreamer->getTargetStreamer());
  RTS.emitTargetAttributes(*STI);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePrimateAsmPrinter() {
  RegisterAsmPrinter<PrimateAsmPrinter> X(getThePrimate32Target());
  RegisterAsmPrinter<PrimateAsmPrinter> Y(getThePrimate64Target());
}
