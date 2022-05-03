//===-- PrimateInstrInfoM.td - Primate 'M' instructions ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Primate instructions from the standard 'M', Integer
// Multiplication and Division instruction set extension.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Primate specific DAG Nodes.
//===----------------------------------------------------------------------===//

def primate_mulhsu : SDNode<"PrimateISD::MULHSU", SDTIntBinOp>;
def primate_divw  : SDNode<"PrimateISD::DIVW",  SDT_PrimateIntBinOpW>;
def primate_divuw : SDNode<"PrimateISD::DIVUW", SDT_PrimateIntBinOpW>;
def primate_remuw : SDNode<"PrimateISD::REMUW", SDT_PrimateIntBinOpW>;

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasStdExtM] in {
def MUL     : ALU_rr<0b0000001, 0b000, "mul">,
              Sched<[WriteIMul, ReadIMul, ReadIMul]>;
def MULH    : ALU_rr<0b0000001, 0b001, "mulh">,
              Sched<[WriteIMul, ReadIMul, ReadIMul]>;
def MULHSU  : ALU_rr<0b0000001, 0b010, "mulhsu">,
              Sched<[WriteIMul, ReadIMul, ReadIMul]>;
def MULHU   : ALU_rr<0b0000001, 0b011, "mulhu">,
              Sched<[WriteIMul, ReadIMul, ReadIMul]>;
def DIV     : ALU_rr<0b0000001, 0b100, "div">,
              Sched<[WriteIDiv, ReadIDiv, ReadIDiv]>;
def DIVU    : ALU_rr<0b0000001, 0b101, "divu">,
              Sched<[WriteIDiv, ReadIDiv, ReadIDiv]>;
def REM     : ALU_rr<0b0000001, 0b110, "rem">,
              Sched<[WriteIDiv, ReadIDiv, ReadIDiv]>;
def REMU    : ALU_rr<0b0000001, 0b111, "remu">,
              Sched<[WriteIDiv, ReadIDiv, ReadIDiv]>;
} // Predicates = [HasStdExtM]

let Predicates = [HasStdExtM, IsPR64] in {
def MULW    : ALUW_rr<0b0000001, 0b000, "mulw">,
              Sched<[WriteIMul32, ReadIMul32, ReadIMul32]>;
def DIVW    : ALUW_rr<0b0000001, 0b100, "divw">,
              Sched<[WriteIDiv32, ReadIDiv32, ReadIDiv32]>;
def DIVUW   : ALUW_rr<0b0000001, 0b101, "divuw">,
              Sched<[WriteIDiv32, ReadIDiv32, ReadIDiv32]>;
def REMW    : ALUW_rr<0b0000001, 0b110, "remw">,
              Sched<[WriteIDiv32, ReadIDiv32, ReadIDiv32]>;
def REMUW   : ALUW_rr<0b0000001, 0b111, "remuw">,
              Sched<[WriteIDiv32, ReadIDiv32, ReadIDiv32]>;
} // Predicates = [HasStdExtM, IsPR64]

//===----------------------------------------------------------------------===//
// Pseudo-instructions and codegen patterns
//===----------------------------------------------------------------------===//

let Predicates = [HasStdExtM] in {
def : PatGprGpr<mul, MUL>;
def : PatGprGpr<mulhs, MULH>;
def : PatGprGpr<mulhu, MULHU>;
def : PatGprGpr<primate_mulhsu, MULHSU>;
def : PatGprGpr<sdiv, DIV>;
def : PatGprGpr<udiv, DIVU>;
def : PatGprGpr<srem, REM>;
def : PatGprGpr<urem, REMU>;
} // Predicates = [HasStdExtM]

let Predicates = [HasStdExtM, IsPR64] in {
def : Pat<(sext_inreg (mul GPR:$rs1, GPR:$rs2), i32),
          (MULW GPR:$rs1, GPR:$rs2)>;

def : PatGprGpr<primate_divw, DIVW>;
def : PatGprGpr<primate_divuw, DIVUW>;
def : PatGprGpr<primate_remuw, REMUW>;

// Handle the specific cases where using DIVU/REMU would be correct and result
// in fewer instructions than emitting DIVUW/REMUW then zero-extending the
// result.
def : Pat<(and (primate_divuw (assertzexti32 GPR:$rs1),
                            (assertzexti32 GPR:$rs2)), 0xffffffff),
          (DIVU GPR:$rs1, GPR:$rs2)>;
def : Pat<(and (primate_remuw (assertzexti32 GPR:$rs1),
                            (assertzexti32 GPR:$rs2)), 0xffffffff),
          (REMU GPR:$rs1, GPR:$rs2)>;

// Although the sexti32 operands may not have originated from an i32 srem,
// this pattern is safe as it is impossible for two sign extended inputs to
// produce a result where res[63:32]=0 and res[31]=1.
def : Pat<(srem (sexti32 (i64 GPR:$rs1)), (sexti32 (i64 GPR:$rs2))),
          (REMW GPR:$rs1, GPR:$rs2)>;
} // Predicates = [HasStdExtM, IsPR64]

let Predicates = [HasStdExtM, IsPR64, NotHasStdExtZba] in {
// Special case for calculating the full 64-bit product of a 32x32 unsigned
// multiply where the inputs aren't known to be zero extended. We can shift the
// inputs left by 32 and use a MULHU. This saves two SRLIs needed to finish
// zeroing the upper 32 bits.
// TODO: If one of the operands is zero extended and the other isn't, we might
// still be better off shifting both left by 32.
def : Pat<(i64 (mul (and GPR:$rs1, 0xffffffff), (and GPR:$rs2, 0xffffffff))),
          (MULHU (SLLI GPR:$rs1, 32), (SLLI GPR:$rs2, 32))>;
// Prevent matching the first part of this pattern to mulw. The mul here has
// additionals users or the ANDs would have been removed. The above pattern
// will be used for the other users. If we form a mulw we'll keep the ANDs alive
// and they'll still become SLLI+SRLI.
def : Pat<(sext_inreg (mul (and GPR:$rs1, 0xffffffff),
                           (and GPR:$rs2, 0xffffffff)), i32),
          (ADDIW (MULHU (SLLI GPR:$rs1, 32), (SLLI GPR:$rs2, 32)), 0)>;
} // Predicates = [HasStdExtM, IsPR64, NotHasStdExtZba]
