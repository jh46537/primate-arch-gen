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
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
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
  void PseudoExpansionIndexFixup(const MachineInstr *MI, unsigned int *, unsigned int *);
  bool emitPseudoExpansionCustomLowering(MCStreamer &OutStreamer,
                                          const MachineInstr *MI, 
                                          unsigned int *lastSlotIdx);

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

void PrimateAsmPrinter::PseudoExpansionIndexFixup(const MachineInstr *MI, unsigned int *slotIdx, unsigned int *lastSlotIdx) {
  if(!MI->isPseudo()) {
    return;
  }

  switch(MI->getOpcode()) {
    case Primate::PseudoADDIwsi:
    case Primate::PseudoADDIwwi:
    case Primate::PseudoADDwww:
    case Primate::PseudoADDwss: {
      *slotIdx -= 1;
      return;
    }
    default: {
      return;
    }
  }
}

bool PrimateAsmPrinter::emitPseudoExpansionCustomLowering(MCStreamer &OutStreamer, const MachineInstr *MI, unsigned int *lastSlotIdx) {
  if(!MI->isPseudo()) {
    return false;
  }
  
  // custom lower the fused op instructions
  // lower things that are things that need to be lowered
  // only need to lower the words down to the other things
  // create instrs, and then move the slot idx n-1 (n is number of ops generated)
  switch(MI->getOpcode()) {
    case Primate::PseudoANDIswi: {
      // WIDEREG:$rs1, simm12:$imm1, simm12:$imm2
      MCInst ExtractOp1;
      MCInst TmpInst;
      MCOperand MCOp;
      ExtractOp1.setOpcode(Primate::EXTRACT);
      ExtractOp1.addOperand(MCOperand::createReg(Primate::X0));
      ExtractOp1.addOperand(MCOperand::createReg(MI->getOperand(1).getReg()));
      lowerOperand(MI->getOperand(2), MCOp);
      ExtractOp1.addOperand(MCOp);

      TmpInst.setOpcode(Primate::ANDI);
      // Operand: rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: rs1
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: imm12
      lowerOperand(MI->getOperand(3), MCOp);
      TmpInst.addOperand(MCOp);
      EmitToStreamer(OutStreamer, ExtractOp1);
      EmitToStreamer(OutStreamer, TmpInst);
      *lastSlotIdx += 1; 
      break;
    }
    case Primate::PseudoANDIwsi: {
      // ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, simm12:$imm1
      MCInst InsertOpDest;
      MCInst TmpInst;
      MCOperand MCOp;
      InsertOpDest.setOpcode(Primate::INSERT);
      // rd
      InsertOpDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      // rs1
      InsertOpDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      // rs2
      InsertOpDest.addOperand(MCOperand::createReg(Primate::X0));
      // imm
      lowerOperand(MI->getOperand(2), MCOp);
      InsertOpDest.addOperand(MCOp);

      TmpInst.setOpcode(Primate::ANDI);
      // rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // rs
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // imm12
      lowerOperand(MI->getOperand(2), MCOp);
      TmpInst.addOperand(MCOp);
      EmitToStreamer(OutStreamer, TmpInst);
      EmitToStreamer(OutStreamer, InsertOpDest);
      *lastSlotIdx += 1; 
      break;
    }
    case Primate::PseudoANDIwwi: {
      // ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, simm12:$imm2
      // insert: (outs WIDEREG:$rd), (ins WIDEREG:$rs1, GPR:$rs2, simm12:$imm12)
      MCInst InsertDest;
      MCInst ExtractOp1;
      MCInst TmpInst;
      MCOperand MCOp;
      InsertDest.setOpcode(Primate::INSERT);
      // rd
      InsertDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      // rs1
      InsertDest.addOperand(MCOperand::createReg(MI->getOperand(1).getReg()));
      // rs2 
      InsertDest.addOperand(MCOperand::createReg(Primate::X0));
      // imm12
      lowerOperand(MI->getOperand(2), MCOp);
      InsertDest.addOperand(MCOp);

      TmpInst.setOpcode(Primate::ANDI);
      // Operand: rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: rs1
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: imm12
      lowerOperand(MI->getOperand(5), MCOp);
      TmpInst.addOperand(MCOp);


      ExtractOp1.setOpcode(Primate::EXTRACT);
      // rd
      ExtractOp1.addOperand(MCOperand::createReg(Primate::X0));
      // rs1
      ExtractOp1.addOperand(MCOperand::createReg(MI->getOperand(3).getReg()));
      // imm12
      lowerOperand(MI->getOperand(4), MCOp);
      ExtractOp1.addOperand(MCOp);
      EmitToStreamer(OutStreamer, ExtractOp1);
      EmitToStreamer(OutStreamer, TmpInst);
      EmitToStreamer(OutStreamer, InsertDest);
      *lastSlotIdx += 2; 
      break;
    }

    case Primate::PseudoADDIswi: {
      // WIDEREG:$rs1, simm12:$imm1, simm12:$imm2
      MCInst ExtractOp1;
      MCInst TmpInst;
      MCOperand MCOp;
      ExtractOp1.setOpcode(Primate::EXTRACT);
      ExtractOp1.addOperand(MCOperand::createReg(Primate::X0));
      ExtractOp1.addOperand(MCOperand::createReg(MI->getOperand(1).getReg()));
      lowerOperand(MI->getOperand(2), MCOp);
      ExtractOp1.addOperand(MCOp);

      TmpInst.setOpcode(Primate::ADDI);
      // Operand: rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: rs1
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: imm12
      lowerOperand(MI->getOperand(3), MCOp);
      TmpInst.addOperand(MCOp);
      EmitToStreamer(OutStreamer, ExtractOp1);
      EmitToStreamer(OutStreamer, TmpInst);
      *lastSlotIdx += 1; 
      break;
    }
    case Primate::PseudoADDIwsi: {
      // ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, simm12:$imm1
      MCInst InsertOpDest;
      MCInst TmpInst;
      MCOperand MCOp;
      InsertOpDest.setOpcode(Primate::INSERT);
      // rd
      InsertOpDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      // rs1
      InsertOpDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      // rs2
      InsertOpDest.addOperand(MCOperand::createReg(Primate::X0));
      // imm
      lowerOperand(MI->getOperand(2), MCOp);
      InsertOpDest.addOperand(MCOp);

      TmpInst.setOpcode(Primate::ADDI);
      // rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // rs
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // imm12
      lowerOperand(MI->getOperand(2), MCOp);
      TmpInst.addOperand(MCOp);
      EmitToStreamer(OutStreamer, TmpInst);
      EmitToStreamer(OutStreamer, InsertOpDest);
      *lastSlotIdx += 1; 
      break;
    }
    case Primate::PseudoADDIwwi: {
      // ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, simm12:$imm2
      // insert: (outs WIDEREG:$rd), (ins WIDEREG:$rs1, GPR:$rs2, simm12:$imm12)
      MCInst InsertDest;
      MCInst ExtractOp1;
      MCInst TmpInst;
      MCOperand MCOp;
      InsertDest.setOpcode(Primate::INSERT);
      // rd
      InsertDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      // rs1
      InsertDest.addOperand(MCOperand::createReg(MI->getOperand(1).getReg()));
      // rs2 
      InsertDest.addOperand(MCOperand::createReg(Primate::X0));
      // imm12
      lowerOperand(MI->getOperand(2), MCOp);
      InsertDest.addOperand(MCOp);

      TmpInst.setOpcode(Primate::ADDI);
      // Operand: rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // rs1
      TmpInst.addOperand(MCOperand::createReg(Primate::X0));
      // Operand: imm12
      lowerOperand(MI->getOperand(5), MCOp);
      TmpInst.addOperand(MCOp);


      ExtractOp1.setOpcode(Primate::EXTRACT);
      // rd
      ExtractOp1.addOperand(MCOperand::createReg(Primate::X0));
      // rs1
      ExtractOp1.addOperand(MCOperand::createReg(MI->getOperand(3).getReg()));
      // imm12
      lowerOperand(MI->getOperand(4), MCOp);
      ExtractOp1.addOperand(MCOp);
      EmitToStreamer(OutStreamer, ExtractOp1);
      EmitToStreamer(OutStreamer, TmpInst);
      EmitToStreamer(OutStreamer, InsertDest);
      *lastSlotIdx += 2; 
      break;
    }
    case Primate::PseudoADDsww: {
      // outs GPR
      // ins WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2
      MCInst ExtractOp1;
      MCInst ExtractOp2;
      MCInst TmpInst;
      MCOperand MCOp;

      TmpInst.setOpcode(Primate::ADD);
      TmpInst.addOperand(MCOperand::createReg(Primate::X0)); //rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0)); //rs1
      TmpInst.addOperand(MCOperand::createReg(Primate::X0)); //rs2

      ExtractOp1.setOpcode(Primate::EXTRACT);
      // rd
      ExtractOp1.addOperand(MCOperand::createReg(Primate::X0));
      // rs1
      ExtractOp1.addOperand(MCOperand::createReg(MI->getOperand(1).getReg()));
      // imm12
      lowerOperand(MI->getOperand(2), MCOp);
      ExtractOp1.addOperand(MCOp);

      ExtractOp2.setOpcode(Primate::EXTRACT);
      // rd
      ExtractOp2.addOperand(MCOperand::createReg(Primate::X0));
      // rs1
      ExtractOp2.addOperand(MCOperand::createReg(MI->getOperand(3).getReg()));
      // imm12
      lowerOperand(MI->getOperand(4), MCOp);
      ExtractOp2.addOperand(MCOp);
      EmitToStreamer(OutStreamer, ExtractOp2);
      EmitToStreamer(OutStreamer, ExtractOp1);
      EmitToStreamer(OutStreamer, TmpInst);
      *lastSlotIdx += 2; 
      break;
    }
    case Primate::PseudoADDwss: {
      // (outs WIDEREG:$rd)
      // (ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, GPR:$rs2)

      // insert: (outs WIDEREG:$rd), (ins WIDEREG:$rs1, GPR:$rs2, simm12:$imm12)
      MCInst InsertDest;
      MCInst TmpInst;
      MCOperand MCOp;

      InsertDest.setOpcode(Primate::INSERT);
      InsertDest.addOperand(MCOperand::createReg(MI->getOperand(0).getReg())); // rd
      InsertDest.addOperand(MCOperand::createReg(MI->getOperand(1).getReg())); // rs1
      InsertDest.addOperand(MCOperand::createReg(Primate::X0));                // rs2
      lowerOperand(MI->getOperand(2), MCOp);
      InsertDest.addOperand(MCOp); // imm12

      TmpInst.setOpcode(Primate::ADD);
      TmpInst.addOperand(MCOperand::createReg(Primate::X0)); // rd
      TmpInst.addOperand(MCOperand::createReg(Primate::X0)); // rs1
      TmpInst.addOperand(MCOperand::createReg(Primate::X0)); // rs2
      EmitToStreamer(OutStreamer, TmpInst);
      EmitToStreamer(OutStreamer, InsertDest);
      *lastSlotIdx += 1; 
      break;
    }
    case Primate::PseudoADDwww: 
    default: {
      return false;
      //llvm_unreachable("don't know how to custom or TABLEGEN expand this pseudo instr. See Kayvan.");
    }
 }
 return true;
}

void PrimateAsmPrinter::EmitToStreamer(MCStreamer &S, const MCInst &Inst) {
  AsmPrinter::EmitToStreamer(*OutStreamer, Inst);
}

// Simple pseudo-instructions have their lowering (with expansion to real
// instructions) auto-generated.
#include "PrimateGenMCPseudoLowering.inc"

void PrimateAsmPrinter::emitInstruction(const MachineInstr *MI) {
  Primate_MC::verifyInstructionPredicates(MI->getOpcode(), STI->getFeatureBits());

  // last resource group contains count of all units (PrimatePipes)
  // scheduler file must be defined accordingly
  unsigned const numResourceGroups = STI->getSchedModel().NumProcResourceKinds;
  auto const& lastResourceGroup = STI->getSchedModel().ProcResourceTable[numResourceGroups-1];
  unsigned const numSlots = lastResourceGroup.NumUnits;
  dbgs() << "packetizing with " << numSlots << " slots\n";

  MCInst MCB;
  MCB.setOpcode(Primate::BUNDLE);
  MCB.addOperand(MCOperand::createImm(0));

  if (MI->isBundle()) {
    const MachineBasicBlock* MBB = MI->getParent();
    MachineBasicBlock::const_instr_iterator MII = MI->getIterator();
    LLVM_DEBUG(dbgs() << MBB->getFullName() << " " << MBB->getName() << "\n"; MBB->printAsOperand(dbgs(), false));
    LLVM_DEBUG(dbgs() << "========== Bundle ===========\n");
    unsigned lastSlotIdx = 0;
    std::vector<const MachineInstr*> ordered_machine_instrs;
    for (++MII; MII != MBB->instr_end() && MII->isInsideBundle(); ++MII) {
      ordered_machine_instrs.push_back(&(*MII));
    }
    std::sort(ordered_machine_instrs.begin(), ordered_machine_instrs.end(),
      [](const MachineInstr* a, const MachineInstr* b) -> bool {
        return a->getSlotIdx() < b->getSlotIdx();
      }
    );
    for(auto& MII: ordered_machine_instrs) {
      if (!MII->isDebugInstr() && !MII->isImplicitDef()) {
        unsigned slotIdx = MII->getSlotIdx();
        // ignore instructions without slots
        if (slotIdx == (unsigned)-1) {
          LLVM_DEBUG({
            dbgs() << "no slot!\n";
            MI->dump();
          });
          continue;
        }
        if(slotIdx > numSlots) {
          LLVM_DEBUG(
            {
              dbgs() << "Instruction slot higher than number of lanes!\n";
              MI->dump();
            }
          );
        }

        // pseudos that expand to many instrs need to fix up the slots for nop generation. 
        PseudoExpansionIndexFixup( &*MII, &slotIdx, &lastSlotIdx);

        LLVM_DEBUG({
          dbgs() << "slot " << slotIdx << " last slot " << lastSlotIdx;
          MII->dump();
        });

        if (slotIdx > lastSlotIdx) {
          emitNops(slotIdx - lastSlotIdx);
          lastSlotIdx = slotIdx;
        }
        ++lastSlotIdx;

        // may need to emit multiple ops, and then advance the slot idx.
        bool customLower = false;
        if(emitPseudoExpansionCustomLowering(*OutStreamer, &*MII, &lastSlotIdx)) {
          dbgs() << "custom lower instruction\n";
          customLower = true;
        }

        // Do any auto-generated pseudo lowerings.
        // TODO: Fix the interface here to deal with multiple expand
        // currently tablegen can only emit 1 anyway, so who cares
        if (emitPseudoExpansionLowering(*OutStreamer, &*MII) && !customLower){
          dbgs() << "tablegen lower instruction\n";
          customLower = true;
        }

        // check for pre instr labels and print those if we have them, this is required 
        // for pcrel_lo/hi pairs
        if(auto* preInstSym = MII->getPreInstrSymbol()) {
          OutStreamer->emitLabel(preInstSym);
        }

        // normal instruction lowering
        MCInst TmpInst;
        if (!lowerPrimateMachineInstrToMCInst(&*MII, TmpInst, *this) && !customLower)
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
  return;
  PrimateTargetStreamer &RTS =
      static_cast<PrimateTargetStreamer &>(*OutStreamer->getTargetStreamer());
  RTS.emitTargetAttributes(*STI);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePrimateAsmPrinter() {
  RegisterAsmPrinter<PrimateAsmPrinter> X(getThePrimate32Target());
  RegisterAsmPrinter<PrimateAsmPrinter> Y(getThePrimate64Target());
}
