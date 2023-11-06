//===- PrimateInstrFormats.td - Primate Instruction Formats *- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//  These instruction format definitions are structured to match the
//  description in the Primate User-Level ISA specification as closely as
//  possible. For instance, the specification describes instructions with the
//  MSB (31st bit) on the left and the LSB (0th bit) on the right. This is
//  reflected in the order of parameters to each instruction class.
//
//  One area of divergence is in the description of immediates. The
//  specification describes immediate encoding in terms of bit-slicing
//  operations on the logical value represented. The immediate argument to
//  these instruction formats instead represents the bit sequence that will be
//  inserted into the instruction. e.g. although JAL's immediate is logically
//  a 21-bit value (where the LSB is always zero), we describe it as an imm20
//  to match how it is encoded.
//
//===----------------------------------------------------------------------===//

// Format specifies the encoding used by the instruction. This is used by
// PrimateMCCodeEmitter to determine which form of fixup to use. These
// definitions must be kept in-sync with PrimateBaseInfo.h.
class InstFormat<bits<5> val> {
  bits<5> Value = val;
}
def InstFormatPseudo : InstFormat<0>;
def InstFormatR      : InstFormat<1>;
def InstFormatR4     : InstFormat<2>;
def InstFormatI      : InstFormat<3>;
def InstFormatS      : InstFormat<4>;
def InstFormatB      : InstFormat<5>;
def InstFormatU      : InstFormat<6>;
def InstFormatJ      : InstFormat<7>;
def InstFormatCR     : InstFormat<8>;
def InstFormatCI     : InstFormat<9>;
def InstFormatCSS    : InstFormat<10>;
def InstFormatCIW    : InstFormat<11>;
def InstFormatCL     : InstFormat<12>;
def InstFormatCS     : InstFormat<13>;
def InstFormatCA     : InstFormat<14>;
def InstFormatCB     : InstFormat<15>;
def InstFormatCJ     : InstFormat<16>;
def InstFormatOther  : InstFormat<17>;

class PrimateVConstraint<bits<3> val> {
  bits<3> Value = val;
}
def NoConstraint  : PrimateVConstraint<0b000>;
def VS2Constraint : PrimateVConstraint<0b001>;
def VS1Constraint : PrimateVConstraint<0b010>;
def VMConstraint  : PrimateVConstraint<0b100>;

// Illegal instructions:
//
// * The destination vector register group for a masked vector instruction
// cannot overlap the source mask register (v0), unless the destination vector
// register is being written with a mask value (e.g., comparisons) or the
// scalar result of a reduction.
//
// * Widening: The destination EEW is greater than the source EEW, the source
// EMUL is at least 1. The destination vector register group cannot overlap
// with the source vector register groups besides the highest-numbered part of
// the destination register group.
//
// * Narrowing: The destination EEW is smaller than the source EEW. The
// destination vector register group cannot overlap with the source vector
// register groups besides the lowest-numbered part of the source register
// group.
//
// * vmsbf.m/vmsif.m/vmsof.m: The destination register cannot overlap the
// source register and, if masked, cannot overlap the mask register ('v0').
//
// * viota: The destination register cannot overlap the source register and,
// if masked, cannot overlap the mask register ('v0').
//
// * v[f]slide[1]up: The destination vector register group for vslideup cannot
// overlap the source vector register group.
//
// * vrgather: The destination vector register group cannot overlap with the
// source vector register groups.
//
// * vcompress: The destination vector register group cannot overlap the
// source vector register group or the source mask register
def WidenV       : PrimateVConstraint<!or(VS2Constraint.Value,
                                        VS1Constraint.Value,
                                        VMConstraint.Value)>;
def WidenW       : PrimateVConstraint<!or(VS1Constraint.Value,
                                        VMConstraint.Value)>;
def WidenCvt     : PrimateVConstraint<!or(VS2Constraint.Value,
                                        VMConstraint.Value)>;
def Iota         : PrimateVConstraint<!or(VS2Constraint.Value,
                                        VMConstraint.Value)>;
def SlideUp      : PrimateVConstraint<!or(VS2Constraint.Value,
                                        VMConstraint.Value)>;
def Vrgather     : PrimateVConstraint<!or(VS2Constraint.Value,
                                        VS1Constraint.Value,
                                        VMConstraint.Value)>;
def Vcompress    : PrimateVConstraint<!or(VS2Constraint.Value,
                                        VS1Constraint.Value)>;

// The following opcode names match those given in Table 19.1 in the
// Primate User-level ISA specification ("Primate base opcode map").
class PrimateOpcode<bits<7> val> {
  bits<7> Value = val;
}
def OPC_LOAD      : PrimateOpcode<0b0000011>;
def OPC_LOAD_FP   : PrimateOpcode<0b0000111>;
def OPC_MISC_MEM  : PrimateOpcode<0b0001111>;
def OPC_OP_IMM    : PrimateOpcode<0b0010011>;
def OPC_AUIPC     : PrimateOpcode<0b0010111>;
def OPC_OP_IMM_32 : PrimateOpcode<0b0011011>;
def OPC_STORE     : PrimateOpcode<0b0100011>;
def OPC_STORE_FP  : PrimateOpcode<0b0100111>;
def OPC_AMO       : PrimateOpcode<0b0101111>;
def OPC_OP        : PrimateOpcode<0b0110011>;
def OPC_LUI       : PrimateOpcode<0b0110111>;
def OPC_OP_32     : PrimateOpcode<0b0111011>;
def OPC_MADD      : PrimateOpcode<0b1000011>;
def OPC_MSUB      : PrimateOpcode<0b1000111>;
def OPC_NMSUB     : PrimateOpcode<0b1001011>;
def OPC_NMADD     : PrimateOpcode<0b1001111>;
def OPC_OP_FP     : PrimateOpcode<0b1010011>;
def OPC_OP_V      : PrimateOpcode<0b1010111>;
def OPC_BRANCH    : PrimateOpcode<0b1100011>;
def OPC_JALR      : PrimateOpcode<0b1100111>;
def OPC_JAL       : PrimateOpcode<0b1101111>;
def OPC_SYSTEM    : PrimateOpcode<0b1110011>;
def OPC_PR_INPUT           : PrimateOpcode<0b0001011>;
def OPC_PR_OUTPUT          : PrimateOpcode<0b1011011>;
def OPC_PR_EXTRACT_INSERT  : PrimateOpcode<0b0101011>;
def OPC_PR_MATCH           : PrimateOpcode<0b1111011>;

class PRInst<dag outs, dag ins, string opcodestr, string argstr,
             list<dag> pattern, InstFormat format, InstrItinClass itin = ItinGreen>
    : Instruction {
  field bits<32> Inst;
  // SoftFail is a field the disassembler can use to provide a way for
  // instructions to not match without killing the whole decode process. It is
  // mainly used for ARM, but Tablegen expects this field to exist or it fails
  // to build the decode table.
  field bits<32> SoftFail = 0;
  let Size = 4;

  bits<7> Opcode = 0;

  let Inst{6-0} = Opcode;

  let Namespace = "Primate";

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = opcodestr # "\t" # argstr;
  let Pattern = pattern;
  let Itinerary = itin;

  let TSFlags{4-0} = format.Value;

  // Defaults
  PrimateVConstraint PRVConstraint = NoConstraint;
  let TSFlags{7-5} = PRVConstraint.Value;

  bits<3> VLMul = 0;
  let TSFlags{10-8} = VLMul;

  bit HasDummyMask = 0;
  let TSFlags{11} = HasDummyMask;

  bit ForceTailAgnostic = false;
  let TSFlags{12} = ForceTailAgnostic;

  bit HasMergeOp = 0;
  let TSFlags{13} = HasMergeOp;

  bit HasSEWOp = 0;
  let TSFlags{14} = HasSEWOp;

  bit HasVLOp = 0;
  let TSFlags{15} = HasVLOp;
}

// Pseudo instructions
class Pseudo<dag outs, dag ins, list<dag> pattern, string opcodestr = "", string argstr = "",
             InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, pattern, InstFormatPseudo, itin>,
      Sched<[]> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// Pseudo load instructions.
class PseudoLoad<string opcodestr, RegisterClass rdty = GPR>
    : Pseudo<(outs rdty:$rd), (ins bare_symbol:$addr), [], opcodestr, "$rd, $addr"> {
  let hasSideEffects = 0;
  let mayLoad = 1;
  let mayStore = 0;
  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
}

class PseudoFloatLoad<string opcodestr, RegisterClass rdty = GPR>
    : Pseudo<(outs rdty:$rd, GPR:$tmp), (ins bare_symbol:$addr), [], opcodestr, "$rd, $addr, $tmp"> {
  let hasSideEffects = 0;
  let mayLoad = 1;
  let mayStore = 0;
  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
}

// Pseudo store instructions.
class PseudoStore<string opcodestr, RegisterClass rsty = GPR>
    : Pseudo<(outs rsty:$rs, GPR:$tmp), (ins bare_symbol:$addr), [], opcodestr, "$rs, $addr, $tmp"> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 1;
  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
}

// Instruction formats are listed in the order they appear in the Primate
// instruction set manual (R, I, S, B, U, J) with sub-formats (e.g. PRInstR4,
// PRInstRAtomic) sorted alphabetically.

class PRInstR<bits<7> funct7, bits<3> funct3, PrimateOpcode opcode, dag outs,
              dag ins, string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-25} = funct7;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstR4<bits<2> funct2, bits<3> funct3, PrimateOpcode opcode, dag outs,
               dag ins, string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR4, itin> {
  bits<5> rs3;
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-27} = rs3;
  let Inst{26-25} = funct2;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstR4Frm<bits<2> funct2, PrimateOpcode opcode, dag outs, dag ins,
                  string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR4, itin> {
  bits<5> rs3;
  bits<5> rs2;
  bits<5> rs1;
  bits<3> funct3;
  bits<5> rd;

  let Inst{31-27} = rs3;
  let Inst{26-25} = funct2;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstRAtomic<bits<5> funct5, bit aq, bit rl, bits<3> funct3,
                    PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                    string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-27} = funct5;
  let Inst{26} = aq;
  let Inst{25} = rl;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstRFrm<bits<7> funct7, PrimateOpcode opcode, dag outs, dag ins,
                 string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
  bits<5> rs2;
  bits<5> rs1;
  bits<3> funct3;
  bits<5> rd;

  let Inst{31-25} = funct7;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstI<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
              string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
  bits<12> imm12;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-20} = imm12;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstIShift<bits<5> imm11_7, bits<3> funct3, PrimateOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr,
                   InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
  bits<6> shamt;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-27} = imm11_7;
  let Inst{26} = 0;
  let Inst{25-20} = shamt;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstIShiftW<bits<7> imm11_5, bits<3> funct3, PrimateOpcode opcode,
                    dag outs, dag ins, string opcodestr, string argstr,
                    InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
  bits<5> shamt;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-25} = imm11_5;
  let Inst{24-20} = shamt;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstS<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
              string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatS, itin> {
  bits<12> imm12;
  bits<5> rs2;
  bits<5> rs1;

  let Inst{31-25} = imm12{11-5};
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = imm12{4-0};
  let Opcode = opcode.Value;
}

class PRInstB<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
              string opcodestr, string argstr, InstrItinClass itin = ItinBranch>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatB, itin> {
  bits<12> imm12;
  bits<5> rs2;
  bits<5> rs1;

  let Inst{31} = imm12{11};
  let Inst{30-25} = imm12{9-4};
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-8} = imm12{3-0};
  let Inst{7} = imm12{10};
  let Opcode = opcode.Value;
}

class PRInstU<PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
              string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatU, itin> {
  bits<20> imm20;
  bits<5> rd;

  let Inst{31-12} = imm20;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

class PRInstJ<PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
              string argstr, InstrItinClass itin = ItinBranch>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatJ, itin> {
  bits<20> imm20;
  bits<5> rd;

  let Inst{31} = imm20{19};
  let Inst{30-21} = imm20{9-0};
  let Inst{20} = imm20{10};
  let Inst{19-12} = imm20{18-11};
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}
