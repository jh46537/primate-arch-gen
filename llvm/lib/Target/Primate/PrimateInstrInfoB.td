//===-- PrimateInstrInfoZb.td - Primate Bitmanip instructions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Primate instructions from the standard Bitmanip
// extensions, versions:
//   Zba - 1.0
//   Zbb - 1.0
//   Zbc - 1.0
//   Zbs - 1.0
//
// This file also describes Primate instructions from the Zbk* extensions in
// Cryptography Extensions Volume I: Scalar & Entropy Source Instructions,
// versions:
//   Zbkb - 1.0
//   Zbkc - 1.0
//   Zbkx - 1.0
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Operand and SDNode transformation definitions.
//===----------------------------------------------------------------------===//

def primate_clzw   : SDNode<"PrimateISD::CLZW",   SDT_PrimateIntUnaryOpW>;
def primate_ctzw   : SDNode<"PrimateISD::CTZW",   SDT_PrimateIntUnaryOpW>;
def primate_rolw   : SDNode<"PrimateISD::ROLW",   SDT_PrimateIntBinOpW>;
def primate_rorw   : SDNode<"PrimateISD::RORW",   SDT_PrimateIntBinOpW>;
def primate_brev8  : SDNode<"PrimateISD::BREV8",  SDTIntUnaryOp>;
def primate_orc_b  : SDNode<"PrimateISD::ORC_B",  SDTIntUnaryOp>;
def primate_zip    : SDNode<"PrimateISD::ZIP",    SDTIntUnaryOp>;
def primate_unzip  : SDNode<"PrimateISD::UNZIP",  SDTIntUnaryOp>;
def primate_absw   : SDNode<"PrimateISD::ABSW",   SDTIntUnaryOp>;
def primate_clmul  : SDNode<"PrimateISD::CLMUL",  SDTIntBinOp>;
def primate_clmulh : SDNode<"PrimateISD::CLMULH", SDTIntBinOp>;
def primate_clmulr : SDNode<"PrimateISD::CLMULR", SDTIntBinOp>;

def UImmLog2XLenHalfAsmOperand : AsmOperandClass {
  let Name = "UImmLog2XLenHalf";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImmLog2XLenHalf";
}

def shfl_uimm : PrimateOp, ImmLeaf<XLenVT, [{
  if (Subtarget->is64Bit())
    return isUInt<5>(Imm);
  return isUInt<4>(Imm);
}]> {
  let ParserMatchClass = UImmLog2XLenHalfAsmOperand;
  let DecoderMethod = "decodeUImmOperand<5>";
  let OperandType = "OPERAND_UIMM_SHFL";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    if (STI.getTargetTriple().isArch64Bit())
      return  isUInt<5>(Imm);
    return isUInt<4>(Imm);
  }];
}

def BCLRXForm : SDNodeXForm<imm, [{
  // Find the lowest 0.
  return CurDAG->getTargetConstant(llvm::countr_one(N->getZExtValue()),
                                   SDLoc(N), N->getValueType(0));
}]>;

def SingleBitSetMaskToIndex : SDNodeXForm<imm, [{
  // Find the lowest 1.
  return CurDAG->getTargetConstant(llvm::countr_zero(N->getZExtValue()),
                                   SDLoc(N), N->getValueType(0));
}]>;

// Checks if this mask has a single 0 bit and cannot be used with ANDI.
def BCLRMask : ImmLeaf<XLenVT, [{
  if (Subtarget->is64Bit())
    return !isInt<12>(Imm) && isPowerOf2_64(~Imm);
  return !isInt<12>(Imm) && isPowerOf2_32(~Imm);
}], BCLRXForm>;

// Checks if this mask has a single 1 bit and cannot be used with ORI/XORI.
def SingleBitSetMask : ImmLeaf<XLenVT, [{
  if (Subtarget->is64Bit())
    return !isInt<12>(Imm) && isPowerOf2_64(Imm);
  return !isInt<12>(Imm) && isPowerOf2_32(Imm);
}], SingleBitSetMaskToIndex>;

// Check if (or r, i) can be optimized to (BSETI (BSETI r, i0), i1),
// in which i = (1 << i0) | (1 << i1).
def BSETINVTwoBitsMask : PatLeaf<(imm), [{
  if (!N->hasOneUse())
    return false;
  // The immediate should not be a simm12.
  if (isInt<12>(N->getSExtValue()))
    return false;
  // The immediate must have exactly two bits set.
  return llvm::popcount(N->getZExtValue()) == 2;
}]>;

def BSETINVTwoBitsMaskHigh : SDNodeXForm<imm, [{
  uint64_t I = N->getZExtValue();
  return CurDAG->getTargetConstant(llvm::Log2_64(I), SDLoc(N),
                                   N->getValueType(0));
}]>;

// Check if (or r, imm) can be optimized to (BSETI (ORI r, i0), i1),
// in which imm = i0 | (1 << i1).
def BSETINVORIMask : PatLeaf<(imm), [{
  if (!N->hasOneUse())
    return false;
  // The immediate should not be a simm12.
  if (isInt<12>(N->getSExtValue()))
    return false;
  // There should be only one set bit from bit 11 to the top.
  return isPowerOf2_64(N->getZExtValue() & ~0x7ff);
}]>;

def BSETINVORIMaskLow : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getZExtValue() & 0x7ff,
                                   SDLoc(N), N->getValueType(0));
}]>;

// Check if (and r, i) can be optimized to (BCLRI (BCLRI r, i0), i1),
// in which i = ~((1<<i0) | (1<<i1)).
def BCLRITwoBitsMask : PatLeaf<(imm), [{
  if (!N->hasOneUse())
    return false;
  // The immediate should not be a simm12.
  if (isInt<12>(N->getSExtValue()))
    return false;
  // The immediate must have exactly two bits clear.
  return (unsigned)llvm::popcount(N->getZExtValue()) == Subtarget->getXLen() - 2;
}]>;

def BCLRITwoBitsMaskLow : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(llvm::countr_zero(~N->getZExtValue()),
                                   SDLoc(N), N->getValueType(0));
}]>;

def BCLRITwoBitsMaskHigh : SDNodeXForm<imm, [{
  uint64_t I = N->getSExtValue();
  if (!Subtarget->is64Bit())
    I |= 0xffffffffull << 32;
  return CurDAG->getTargetConstant(llvm::Log2_64(~I), SDLoc(N),
                                   N->getValueType(0));
}]>;

// Check if (and r, i) can be optimized to (BCLRI (ANDI r, i0), i1),
// in which i = i0 & ~(1<<i1).
def BCLRIANDIMask : PatLeaf<(imm), [{
  if (!N->hasOneUse())
    return false;
  // The immediate should not be a simm12.
  if (isInt<12>(N->getSExtValue()))
    return false;
  // There should be only one clear bit from bit 11 to the top.
  uint64_t I = N->getZExtValue() | 0x7ff;
  return Subtarget->is64Bit() ? isPowerOf2_64(~I) : isPowerOf2_32(~I);
}]>;

def BCLRIANDIMaskLow : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant((N->getZExtValue() & 0x7ff) | ~0x7ffull,
                                   SDLoc(N), N->getValueType(0));
}]>;

def C3LeftShift : PatLeaf<(imm), [{
  uint64_t C = N->getZExtValue();
  return C > 3 && (C >> llvm::countr_zero(C)) == 3;
}]>;

def C5LeftShift : PatLeaf<(imm), [{
  uint64_t C = N->getZExtValue();
  return C > 5 && (C >> llvm::countr_zero(C)) == 5;
}]>;

def C9LeftShift : PatLeaf<(imm), [{
  uint64_t C = N->getZExtValue();
  return C > 9 && (C >> llvm::countr_zero(C)) == 9;
}]>;

// Constant of the form (3 << C) where C is less than 32.
def C3LeftShiftUW : PatLeaf<(imm), [{
  uint64_t C = N->getZExtValue();
  unsigned Shift = llvm::countr_zero(C);
  return 1 <= Shift && Shift < 32 && (C >> Shift) == 3;
}]>;

// Constant of the form (5 << C) where C is less than 32.
def C5LeftShiftUW : PatLeaf<(imm), [{
  uint64_t C = N->getZExtValue();
  unsigned Shift = llvm::countr_zero(C);
  return 1 <= Shift && Shift < 32 && (C >> Shift) == 5;
}]>;

// Constant of the form (9 << C) where C is less than 32.
def C9LeftShiftUW : PatLeaf<(imm), [{
  uint64_t C = N->getZExtValue();
  unsigned Shift = llvm::countr_zero(C);
  return 1 <= Shift && Shift < 32 && (C >> Shift) == 9;
}]>;

def CSImm12MulBy4 : PatLeaf<(imm), [{
  if (!N->hasOneUse())
    return false;
  int64_t C = N->getSExtValue();
  // Skip if C is simm12, an lui, or can be optimized by the PatLeaf AddiPair.
  return !isInt<13>(C) && !isShiftedInt<20, 12>(C) && isShiftedInt<12, 2>(C);
}]>;

def CSImm12MulBy8 : PatLeaf<(imm), [{
  if (!N->hasOneUse())
    return false;
  int64_t C = N->getSExtValue();
  // Skip if C is simm12, an lui or can be optimized by the PatLeaf AddiPair or
  // CSImm12MulBy4.
  return !isInt<14>(C) && !isShiftedInt<20, 12>(C) && isShiftedInt<12, 3>(C);
}]>;

def SimmShiftRightBy2XForm : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getSExtValue() >> 2, SDLoc(N),
                                   N->getValueType(0));
}]>;

def SimmShiftRightBy3XForm : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getSExtValue() >> 3, SDLoc(N),
                                   N->getValueType(0));
}]>;

// Pattern to exclude simm12 immediates from matching, namely `non_imm12`.
// GISel currently doesn't support PatFrag for leaf nodes, so `non_imm12`
// cannot be implemented in that way. To reuse patterns between the two
// ISels, we instead create PatFrag on operators that use `non_imm12`.
class binop_with_non_imm12<SDPatternOperator binop>
  : PatFrag<(ops node:$x, node:$y), (binop node:$x, node:$y), [{
  auto *C = dyn_cast<ConstantSDNode>(Operands[1]);
  return !C || !isInt<12>(C->getSExtValue());
}]> {
  let PredicateCodeUsesOperands = 1;
  let GISelPredicateCode = [{
    const MachineOperand &ImmOp = *Operands[1];
    const MachineFunction &MF = *MI.getParent()->getParent();
    const MachineRegisterInfo &MRI = MF.getRegInfo();

    if (ImmOp.isReg() && ImmOp.getReg())
      if (auto Val = getIConstantVRegValWithLookThrough(ImmOp.getReg(), MRI)) {
        // We do NOT want immediates that fit in 12 bits.
        return !isInt<12>(Val->Value.getSExtValue());
      }

    return true;
  }];
}
def add_non_imm12       : binop_with_non_imm12<add>;
def or_is_add_non_imm12 : binop_with_non_imm12<or_is_add>;

def Shifted32OnesMask : IntImmLeaf<XLenVT, [{
  if (!Imm.isShiftedMask())
    return false;

  unsigned TrailingZeros = Imm.countr_zero();
  return TrailingZeros > 0 && TrailingZeros < 32 &&
         Imm == UINT64_C(0xFFFFFFFF) << TrailingZeros;
}], TrailingZeros>;

def sh1add_op : ComplexPattern<XLenVT, 1, "selectSHXADDOp<1>", [], [], 6>;
def sh2add_op : ComplexPattern<XLenVT, 1, "selectSHXADDOp<2>", [], [], 6>;
def sh3add_op : ComplexPattern<XLenVT, 1, "selectSHXADDOp<3>", [], [], 6>;

def sh1add_uw_op : ComplexPattern<XLenVT, 1, "selectSHXADD_UWOp<1>", [], [], 6>;
def sh2add_uw_op : ComplexPattern<XLenVT, 1, "selectSHXADD_UWOp<2>", [], [], 6>;
def sh3add_uw_op : ComplexPattern<XLenVT, 1, "selectSHXADD_UWOp<3>", [], [], 6>;

//===----------------------------------------------------------------------===//
// Instruction class templates
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class PRBUnary<bits<12> imm12, bits<3> funct3,
               PrimateOpcode opcode, string opcodestr>
    : PRInstIUnary<imm12, funct3, opcode, (outs GPR:$rd), (ins GPR:$rs1),
                   opcodestr, "$rd, $rs1">;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class PRBShift_ri<bits<5> imm11_7, bits<3> funct3, PrimateOpcode opcode,
                  string opcodestr>
    : PRInstIShift<imm11_7, funct3, opcode, (outs GPR:$rd),
                   (ins GPR:$rs1, uimmlog2xlen:$shamt), opcodestr,
                   "$rd, $rs1, $shamt">;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class PRBShiftW_ri<bits<7> imm11_5, bits<3> funct3, PrimateOpcode opcode,
                   string opcodestr>
    : PRInstIShiftW<imm11_5, funct3, opcode, (outs GPR:$rd),
                    (ins GPR:$rs1, uimm5:$shamt), opcodestr,
                    "$rd, $rs1, $shamt">;

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasStdExtZbbOrZbkb] in {
def ANDN  : ALU_rr<0b0100000, 0b111, "andn">,
            Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def ORN   : ALU_rr<0b0100000, 0b110, "orn">,
            Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def XNOR  : ALU_rr<0b0100000, 0b100, "xnor">,
            Sched<[WriteIALU, ReadIALU, ReadIALU]>;
} // Predicates = [HasStdExtZbbOrZbkb]

let Predicates = [HasStdExtZba] in {
def SH1ADD : ALU_rr<0b0010000, 0b010, "sh1add">,
             Sched<[WriteSHXADD, ReadSHXADD, ReadSHXADD]>;
def SH2ADD : ALU_rr<0b0010000, 0b100, "sh2add">,
             Sched<[WriteSHXADD, ReadSHXADD, ReadSHXADD]>;
def SH3ADD : ALU_rr<0b0010000, 0b110, "sh3add">,
             Sched<[WriteSHXADD, ReadSHXADD, ReadSHXADD]>;
} // Predicates = [HasStdExtZba]

let Predicates = [HasStdExtZba, IsPR64] in {
def SLLI_UW : PRBShift_ri<0b00001, 0b001, OPC_OP_IMM_32, "slli.uw">,
              Sched<[WriteShiftImm32, ReadShiftImm32]>;
def ADD_UW : ALUW_rr<0b0000100, 0b000, "add.uw">,
             Sched<[WriteIALU32, ReadIALU32, ReadIALU32]>;
def SH1ADD_UW : ALUW_rr<0b0010000, 0b010, "sh1add.uw">,
                Sched<[WriteSHXADD32, ReadSHXADD32, ReadSHXADD32]>;
def SH2ADD_UW : ALUW_rr<0b0010000, 0b100, "sh2add.uw">,
                Sched<[WriteSHXADD32, ReadSHXADD32, ReadSHXADD32]>;
def SH3ADD_UW : ALUW_rr<0b0010000, 0b110, "sh3add.uw">,
                Sched<[WriteSHXADD32, ReadSHXADD32, ReadSHXADD32]>;
} // Predicates = [HasStdExtZba, IsPR64]

let Predicates = [HasStdExtZbbOrZbkb] in {
def ROL   : ALU_rr<0b0110000, 0b001, "rol">,
            Sched<[WriteRotateReg, ReadRotateReg, ReadRotateReg]>;
def ROR   : ALU_rr<0b0110000, 0b101, "ror">,
            Sched<[WriteRotateReg, ReadRotateReg, ReadRotateReg]>;

def RORI  : PRBShift_ri<0b01100, 0b101, OPC_OP_IMM, "rori">,
            Sched<[WriteRotateImm, ReadRotateImm]>;
} // Predicates = [HasStdExtZbbOrZbkb]

let Predicates = [HasStdExtZbbOrZbkb, IsPR64], IsSignExtendingOpW = 1 in {
def ROLW  : ALUW_rr<0b0110000, 0b001, "rolw">,
            Sched<[WriteRotateReg32, ReadRotateReg32, ReadRotateReg32]>;
def RORW  : ALUW_rr<0b0110000, 0b101, "rorw">,
            Sched<[WriteRotateReg32, ReadRotateReg32, ReadRotateReg32]>;

def RORIW : PRBShiftW_ri<0b0110000, 0b101, OPC_OP_IMM_32, "roriw">,
            Sched<[WriteRotateImm32, ReadRotateImm32]>;
} // Predicates = [HasStdExtZbbOrZbkb, IsPR64]

let Predicates = [HasStdExtZbs] in {
def BCLR : ALU_rr<0b0100100, 0b001, "bclr">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def BSET : ALU_rr<0b0010100, 0b001, "bset">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def BINV : ALU_rr<0b0110100, 0b001, "binv">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
let IsSignExtendingOpW = 1 in
def BEXT : ALU_rr<0b0100100, 0b101, "bext">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;

def BCLRI : PRBShift_ri<0b01001, 0b001, OPC_OP_IMM, "bclri">,
            Sched<[WriteIALU, ReadIALU]>;
def BSETI : PRBShift_ri<0b00101, 0b001, OPC_OP_IMM, "bseti">,
            Sched<[WriteIALU, ReadIALU]>;
def BINVI : PRBShift_ri<0b01101, 0b001, OPC_OP_IMM, "binvi">,
            Sched<[WriteIALU, ReadIALU]>;
let IsSignExtendingOpW = 1 in
def BEXTI : PRBShift_ri<0b01001, 0b101, OPC_OP_IMM, "bexti">,
            Sched<[WriteIALU, ReadIALU]>;
} // Predicates = [HasStdExtZbs]

let Predicates = [HasStdExtZbb], IsSignExtendingOpW = 1 in {
def CLZ  : PRBUnary<0b011000000000, 0b001, OPC_OP_IMM, "clz">,
           Sched<[WriteCLZ, ReadCLZ]>;
def CTZ  : PRBUnary<0b011000000001, 0b001, OPC_OP_IMM, "ctz">,
           Sched<[WriteCTZ, ReadCTZ]>;
def CPOP : PRBUnary<0b011000000010, 0b001, OPC_OP_IMM, "cpop">,
           Sched<[WriteCPOP, ReadCPOP]>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb, IsPR64], IsSignExtendingOpW = 1 in {
def CLZW   : PRBUnary<0b011000000000, 0b001, OPC_OP_IMM_32, "clzw">,
             Sched<[WriteCLZ32, ReadCLZ32]>;
def CTZW   : PRBUnary<0b011000000001, 0b001, OPC_OP_IMM_32, "ctzw">,
             Sched<[WriteCTZ32, ReadCTZ32]>;
def CPOPW  : PRBUnary<0b011000000010, 0b001, OPC_OP_IMM_32, "cpopw">,
             Sched<[WriteCPOP32, ReadCPOP32]>;
} // Predicates = [HasStdExtZbb, IsPR64]

let Predicates = [HasStdExtZbb], IsSignExtendingOpW = 1 in {
def SEXT_B : PRBUnary<0b011000000100, 0b001, OPC_OP_IMM, "sext.b">,
             Sched<[WriteIALU, ReadIALU]>;
def SEXT_H : PRBUnary<0b011000000101, 0b001, OPC_OP_IMM, "sext.h">,
             Sched<[WriteIALU, ReadIALU]>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb] in {
def MIN  : ALU_rr<0b0000101, 0b100, "min">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def MINU : ALU_rr<0b0000101, 0b101, "minu">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def MAX  : ALU_rr<0b0000101, 0b110, "max">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def MAXU : ALU_rr<0b0000101, 0b111, "maxu">,
           Sched<[WriteIALU, ReadIALU, ReadIALU]>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb, IsPR32] in {
def ZEXT_H_PR32 : PRBUnary<0b000010000000, 0b100, OPC_OP, "zext.h">,
                  Sched<[WriteIALU, ReadIALU]>;
} // Predicates = [HasStdExtZbb, IsPR32]

let Predicates = [HasStdExtZbb, IsPR64], IsSignExtendingOpW = 1 in {
def ZEXT_H_PR64 : PRBUnary<0b000010000000, 0b100, OPC_OP_32, "zext.h">,
                  Sched<[WriteIALU, ReadIALU]>;
} // Predicates = [HasStdExtZbb, IsPR64]

let Predicates = [HasStdExtZbbOrZbkb, IsPR32] in {
def REV8_PR32 : PRBUnary<0b011010011000, 0b101, OPC_OP_IMM, "rev8">,
                Sched<[WriteREV8, ReadREV8]>;
} // Predicates = [HasStdExtZbbOrZbkb, IsPR32]

let Predicates = [HasStdExtZbbOrZbkb, IsPR64] in {
def REV8_PR64 : PRBUnary<0b011010111000, 0b101, OPC_OP_IMM, "rev8">,
                Sched<[WriteREV8, ReadREV8]>;
} // Predicates = [HasStdExtZbbOrZbkb, IsPR64]

let Predicates = [HasStdExtZbb] in {
def ORC_B : PRBUnary<0b001010000111, 0b101, OPC_OP_IMM, "orc.b">,
            Sched<[WriteORCB, ReadORCB]>;
} // Predicates = [HasStdExtZbb]

//===----------------------------------------------------------------------===//
// Pseudo Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasStdExtZba, IsPR64] in {
def : InstAlias<"zext.w $rd, $rs", (ADD_UW GPR:$rd, GPR:$rs, X0)>;
} // Predicates = [HasStdExtZba, IsPR64]

let Predicates = [HasStdExtZbb] in {
def : InstAlias<"ror $rd, $rs1, $shamt",
                (RORI  GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt), 0>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb, IsPR64] in {
def : InstAlias<"rorw $rd, $rs1, $shamt",
                (RORIW  GPR:$rd, GPR:$rs1, uimm5:$shamt), 0>;
} // Predicates = [HasStdExtZbb, IsPR64]

let Predicates = [HasStdExtZbs] in {
def : InstAlias<"bset $rd, $rs1, $shamt",
                (BSETI  GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt), 0>;
def : InstAlias<"bclr $rd, $rs1, $shamt",
                (BCLRI GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt), 0>;
def : InstAlias<"binv $rd, $rs1, $shamt",
                (BINVI GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt), 0>;
def : InstAlias<"bext $rd, $rs1, $shamt",
                (BEXTI GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt), 0>;
} // Predicates = [HasStdExtZbs]

//===----------------------------------------------------------------------===//
// Codegen patterns
//===----------------------------------------------------------------------===//

let Predicates = [HasStdExtZbbOrZbkb] in {
def : Pat<(XLenVT (and GPR:$rs1, (not GPR:$rs2))), (ANDN GPR:$rs1, GPR:$rs2)>;
def : Pat<(XLenVT (or  GPR:$rs1, (not GPR:$rs2))), (ORN  GPR:$rs1, GPR:$rs2)>;
def : Pat<(XLenVT (xor GPR:$rs1, (not GPR:$rs2))), (XNOR GPR:$rs1, GPR:$rs2)>;
} // Predicates = [HasStdExtZbbOrZbkb]

let Predicates = [HasStdExtZbbOrZbkb] in {
def : PatGprGpr<shiftop<rotl>, ROL>;
def : PatGprGpr<shiftop<rotr>, ROR>;

def : PatGprImm<rotr, RORI, uimmlog2xlen>;
// There's no encoding for roli in the the 'B' extension as it can be
// implemented with rori by negating the immediate.
def : Pat<(XLenVT (rotl GPR:$rs1, uimmlog2xlen:$shamt)),
          (RORI GPR:$rs1, (ImmSubFromXLen uimmlog2xlen:$shamt))>;
} // Predicates = [HasStdExtZbbOrZbkb]

let Predicates = [HasStdExtZbbOrZbkb, IsPR64] in {
def : PatGprGpr<shiftopw<primate_rolw>, ROLW>;
def : PatGprGpr<shiftopw<primate_rorw>, RORW>;
def : PatGprImm<primate_rorw, RORIW, uimm5>;
def : Pat<(primate_rolw GPR:$rs1, uimm5:$rs2),
          (RORIW GPR:$rs1, (ImmSubFrom32 uimm5:$rs2))>;
} // Predicates = [HasStdExtZbbOrZbkb, IsPR64]

let Predicates = [HasStdExtZbs] in {
def : Pat<(XLenVT (and (not (shiftop<shl> 1, (XLenVT GPR:$rs2))), GPR:$rs1)),
          (BCLR GPR:$rs1, GPR:$rs2)>;
def : Pat<(XLenVT (and (rotl -2, (XLenVT GPR:$rs2)), GPR:$rs1)),
          (BCLR GPR:$rs1, GPR:$rs2)>;
def : Pat<(XLenVT (or (shiftop<shl> 1, (XLenVT GPR:$rs2)), GPR:$rs1)),
          (BSET GPR:$rs1, GPR:$rs2)>;
def : Pat<(XLenVT (xor (shiftop<shl> 1, (XLenVT GPR:$rs2)), GPR:$rs1)),
          (BINV GPR:$rs1, GPR:$rs2)>;
def : Pat<(XLenVT (and (shiftop<srl> GPR:$rs1, (XLenVT GPR:$rs2)), 1)),
          (BEXT GPR:$rs1, GPR:$rs2)>;

def : Pat<(XLenVT (shiftop<shl> 1, (XLenVT GPR:$rs2))),
          (BSET (XLenVT X0), GPR:$rs2)>;
def : Pat<(XLenVT (not (shiftop<shl> -1, (XLenVT GPR:$rs2)))),
          (ADDI (BSET (XLenVT X0), GPR:$rs2), -1)>;

def : Pat<(XLenVT (and GPR:$rs1, BCLRMask:$mask)),
          (BCLRI GPR:$rs1, BCLRMask:$mask)>;
def : Pat<(XLenVT (or GPR:$rs1, SingleBitSetMask:$mask)),
          (BSETI GPR:$rs1, SingleBitSetMask:$mask)>;
def : Pat<(XLenVT (xor GPR:$rs1, SingleBitSetMask:$mask)),
          (BINVI GPR:$rs1, SingleBitSetMask:$mask)>;

def : Pat<(XLenVT (and (srl GPR:$rs1, uimmlog2xlen:$shamt), (XLenVT 1))),
          (BEXTI GPR:$rs1, uimmlog2xlen:$shamt)>;

def : Pat<(XLenVT (seteq (XLenVT (and GPR:$rs1, SingleBitSetMask:$mask)), 0)),
          (BEXTI (XORI GPR:$rs1, -1), SingleBitSetMask:$mask)>;

def : Pat<(XLenVT (or GPR:$r, BSETINVTwoBitsMask:$i)),
          (BSETI (BSETI GPR:$r, (TrailingZeros BSETINVTwoBitsMask:$i)),
                 (BSETINVTwoBitsMaskHigh BSETINVTwoBitsMask:$i))>;
def : Pat<(XLenVT (xor GPR:$r, BSETINVTwoBitsMask:$i)),
          (BINVI (BINVI GPR:$r, (TrailingZeros BSETINVTwoBitsMask:$i)),
                 (BSETINVTwoBitsMaskHigh BSETINVTwoBitsMask:$i))>;
def : Pat<(XLenVT (or GPR:$r, BSETINVORIMask:$i)),
          (BSETI (ORI GPR:$r, (BSETINVORIMaskLow BSETINVORIMask:$i)),
                 (BSETINVTwoBitsMaskHigh BSETINVORIMask:$i))>;
def : Pat<(XLenVT (xor GPR:$r, BSETINVORIMask:$i)),
          (BINVI (XORI GPR:$r, (BSETINVORIMaskLow BSETINVORIMask:$i)),
                 (BSETINVTwoBitsMaskHigh BSETINVORIMask:$i))>;
def : Pat<(XLenVT (and GPR:$r, BCLRITwoBitsMask:$i)),
          (BCLRI (BCLRI GPR:$r, (BCLRITwoBitsMaskLow BCLRITwoBitsMask:$i)),
                 (BCLRITwoBitsMaskHigh BCLRITwoBitsMask:$i))>;
def : Pat<(XLenVT (and GPR:$r, BCLRIANDIMask:$i)),
          (BCLRI (ANDI GPR:$r, (BCLRIANDIMaskLow BCLRIANDIMask:$i)),
                 (BCLRITwoBitsMaskHigh BCLRIANDIMask:$i))>;
} // Predicates = [HasStdExtZbs]

let Predicates = [HasStdExtZbb] in
def : PatGpr<primate_orc_b, ORC_B>;

let Predicates = [HasStdExtZbb] in {
def : PatGpr<ctlz, CLZ>;
def : PatGpr<cttz, CTZ>;
def : PatGpr<ctpop, CPOP>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb, IsPR64] in {
def : PatGpr<primate_clzw, CLZW>;
def : PatGpr<primate_ctzw, CTZW>;
def : Pat<(i64 (ctpop (i64 (zexti32 (i64 GPR:$rs1))))), (CPOPW GPR:$rs1)>;

def : Pat<(i64 (primate_absw GPR:$rs1)),
          (MAX GPR:$rs1, (SUBW (XLenVT X0), GPR:$rs1))>;
} // Predicates = [HasStdExtZbb, IsPR64]

let Predicates = [HasStdExtZbb] in {
def : Pat<(XLenVT (sext_inreg GPR:$rs1, i8)), (SEXT_B GPR:$rs1)>;
def : Pat<(XLenVT (sext_inreg GPR:$rs1, i16)), (SEXT_H GPR:$rs1)>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb] in {
def : PatGprGpr<smin, MIN>;
def : PatGprGpr<smax, MAX>;
def : PatGprGpr<umin, MINU>;
def : PatGprGpr<umax, MAXU>;
} // Predicates = [HasStdExtZbb]

let Predicates = [HasStdExtZbb, IsPR32] in
def : Pat<(i32 (and GPR:$rs, 0xFFFF)), (ZEXT_H_PR32 GPR:$rs)>;
let Predicates = [HasStdExtZbb, IsPR64] in
def : Pat<(i64 (and GPR:$rs, 0xFFFF)), (ZEXT_H_PR64 GPR:$rs)>;

let Predicates = [HasStdExtZba, IsPR64] in {
def : Pat<(i64 (shl (and GPR:$rs1, 0xFFFFFFFF), uimm5:$shamt)),
          (SLLI_UW GPR:$rs1, uimm5:$shamt)>;
// Match a shifted 0xffffffff mask. Use SRLI to clear the LSBs and SLLI_UW to
// mask and shift.
def : Pat<(i64 (and GPR:$rs1, Shifted32OnesMask:$mask)),
          (SLLI_UW (SRLI GPR:$rs1, Shifted32OnesMask:$mask),
                   Shifted32OnesMask:$mask)>;
def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0xFFFFFFFF), GPR:$rs2)),
          (ADD_UW GPR:$rs1, GPR:$rs2)>;
def : Pat<(i64 (and GPR:$rs, 0xFFFFFFFF)), (ADD_UW GPR:$rs, (XLenVT X0))>;

def : Pat<(i64 (or_is_add_non_imm12 (and GPR:$rs1, 0xFFFFFFFF), GPR:$rs2)),
          (ADD_UW GPR:$rs1, GPR:$rs2)>;

foreach i = {1,2,3} in {
  defvar shxadd_uw = !cast<Instruction>("SH"#i#"ADD_UW");
  def : Pat<(i64 (add_non_imm12 (shl (and GPR:$rs1, 0xFFFFFFFF), (i64 i)), (XLenVT GPR:$rs2))),
            (shxadd_uw GPR:$rs1, GPR:$rs2)>;
}

def : Pat<(i64 (add_non_imm12 (and (shl GPR:$rs1, (i64 1)), 0x1FFFFFFFF), (XLenVT GPR:$rs2))),
          (SH1ADD_UW GPR:$rs1, GPR:$rs2)>;
def : Pat<(i64 (add_non_imm12 (and (shl GPR:$rs1, (i64 2)), 0x3FFFFFFFF), (XLenVT GPR:$rs2))),
          (SH2ADD_UW GPR:$rs1, GPR:$rs2)>;
def : Pat<(i64 (add_non_imm12 (and (shl GPR:$rs1, (i64 3)), 0x7FFFFFFFF), (XLenVT GPR:$rs2))),
          (SH3ADD_UW GPR:$rs1, GPR:$rs2)>;

// More complex cases use a ComplexPattern.
foreach i = {1,2,3} in {
  defvar pat = !cast<ComplexPattern>("sh"#i#"add_uw_op");
  def : Pat<(i64 (add_non_imm12 pat:$rs1, (XLenVT GPR:$rs2))),
            (!cast<Instruction>("SH"#i#"ADD_UW") pat:$rs1, GPR:$rs2)>;
}

def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0xFFFFFFFE), (XLenVT GPR:$rs2))),
          (SH1ADD (SRLIW GPR:$rs1, 1), GPR:$rs2)>;
def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0xFFFFFFFC), (XLenVT GPR:$rs2))),
          (SH2ADD (SRLIW GPR:$rs1, 2), GPR:$rs2)>;
def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0xFFFFFFF8), (XLenVT GPR:$rs2))),
          (SH3ADD (SRLIW GPR:$rs1, 3), GPR:$rs2)>;

// Use SRLI to clear the LSBs and SHXADD_UW to mask and shift.
def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0x1FFFFFFFE), (XLenVT GPR:$rs2))),
          (SH1ADD_UW (SRLI GPR:$rs1, 1), GPR:$rs2)>;
def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0x3FFFFFFFC), (XLenVT GPR:$rs2))),
          (SH2ADD_UW (SRLI GPR:$rs1, 2), GPR:$rs2)>;
def : Pat<(i64 (add_non_imm12 (and GPR:$rs1, 0x7FFFFFFF8), (XLenVT GPR:$rs2))),
          (SH3ADD_UW (SRLI GPR:$rs1, 3), GPR:$rs2)>;

} // Predicates = [HasStdExtZba, IsPR64]

//===----------------------------------------------------------------------===//
// Experimental PR64 i32 legalization patterns.
//===----------------------------------------------------------------------===//

def BCLRMaski32 : ImmLeaf<i32, [{
  return !isInt<12>(Imm) && isPowerOf2_32(~Imm);
}]>;
def SingleBitSetMaski32 : ImmLeaf<i32, [{
  return !isInt<12>(Imm) && isPowerOf2_32(Imm);
}]>;

let Predicates = [HasStdExtZbbOrZbkb, IsPR64] in {
def : Pat<(i32 (and GPR:$rs1, (not GPR:$rs2))), (ANDN GPR:$rs1, GPR:$rs2)>;
def : Pat<(i32 (or  GPR:$rs1, (not GPR:$rs2))), (ORN  GPR:$rs1, GPR:$rs2)>;
def : Pat<(i32 (xor GPR:$rs1, (not GPR:$rs2))), (XNOR GPR:$rs1, GPR:$rs2)>;

def : PatGprGpr<shiftopw<rotl>, ROLW, i32, i64>;
def : PatGprGpr<shiftopw<rotr>, RORW, i32, i64>;
def : PatGprImm<rotr, RORIW, uimm5, i32>;

def : Pat<(i32 (rotl GPR:$rs1, uimm5:$rs2)),
          (RORIW GPR:$rs1, (ImmSubFrom32 uimm5:$rs2))>;
} // Predicates = [HasStdExtZbbOrZbkb, IsPR64]

let Predicates = [HasStdExtZba, IsPR64] in {
def : Pat<(shl (i64 (zext i32:$rs1)), uimm5:$shamt),
          (SLLI_UW GPR:$rs1, uimm5:$shamt)>;

def : Pat<(i64 (add_non_imm12 (zext GPR:$rs1), GPR:$rs2)),
          (ADD_UW GPR:$rs1, GPR:$rs2)>;
def : Pat<(zext GPR:$src), (ADD_UW GPR:$src, (XLenVT X0))>;

def : Pat<(i64 (or_is_add_non_imm12 (zext GPR:$rs1), GPR:$rs2)),
          (ADD_UW GPR:$rs1, GPR:$rs2)>;

foreach i = {1,2,3} in {
  defvar shxadd = !cast<Instruction>("SH"#i#"ADD");
  def : Pat<(i32 (add_non_imm12 (shl GPR:$rs1, (i64 i)), GPR:$rs2)),
            (shxadd GPR:$rs1, GPR:$rs2)>;
}
}

let Predicates = [HasStdExtZbs, IsPR64] in {
def : Pat<(i32 (and (not (shiftop<shl> 1, (XLenVT GPR:$rs2))), GPR:$rs1)),
          (BCLR GPR:$rs1, GPR:$rs2)>;
def : Pat<(i32 (and (rotl -2, (XLenVT GPR:$rs2)), GPR:$rs1)),
          (BCLR GPR:$rs1, GPR:$rs2)>;
def : Pat<(i32 (or (shiftop<shl> 1, (XLenVT GPR:$rs2)), GPR:$rs1)),
          (BSET GPR:$rs1, GPR:$rs2)>;
def : Pat<(i32 (xor (shiftop<shl> 1, (XLenVT GPR:$rs2)), GPR:$rs1)),
          (BINV GPR:$rs1, GPR:$rs2)>;
def : Pat<(i32 (and (shiftop<srl> GPR:$rs1, (XLenVT GPR:$rs2)), 1)),
          (BEXT GPR:$rs1, GPR:$rs2)>;
def : Pat<(i64 (and (anyext (i32 (shiftop<srl> GPR:$rs1, (XLenVT GPR:$rs2)))), 1)),
          (BEXT GPR:$rs1, GPR:$rs2)>;

def : Pat<(i32 (shiftop<shl> 1, (XLenVT GPR:$rs2))),
          (BSET (XLenVT X0), GPR:$rs2)>;
def : Pat<(i32 (not (shiftop<shl> -1, (XLenVT GPR:$rs2)))),
          (ADDI (BSET (XLenVT X0), GPR:$rs2), -1)>;

def : Pat<(i32 (and (srl GPR:$rs1, uimm5:$shamt), (i32 1))),
          (BEXTI GPR:$rs1, uimm5:$shamt)>;

def : Pat<(i32 (and GPR:$rs1, BCLRMaski32:$mask)),
          (BCLRI GPR:$rs1, (i64 (BCLRXForm $mask)))>;
def : Pat<(i32 (or GPR:$rs1, SingleBitSetMaski32:$mask)),
          (BSETI GPR:$rs1, (i64 (SingleBitSetMaskToIndex $mask)))>;
def : Pat<(i32 (xor GPR:$rs1, SingleBitSetMaski32:$mask)),
          (BINVI GPR:$rs1, (i64 (SingleBitSetMaskToIndex $mask)))>;
} // Predicates = [HasStdExtZbs, IsPR64]
