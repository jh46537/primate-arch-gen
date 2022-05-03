//=== PrimateInstrFormatsV.td - Primate V Instruction Formats *- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file describes the Primate V extension instruction formats.
//
//===----------------------------------------------------------------------===//

class PrimateVFormat<bits<3> val> {
  bits<3> Value = val;
}
def OPIVV : PrimateVFormat<0b000>;
def OPFVV : PrimateVFormat<0b001>;
def OPMVV : PrimateVFormat<0b010>;
def OPIVI : PrimateVFormat<0b011>;
def OPIVX : PrimateVFormat<0b100>;
def OPFVF : PrimateVFormat<0b101>;
def OPMVX : PrimateVFormat<0b110>;

class PrimateMOP<bits<2> val> {
  bits<2> Value = val;
}
def MOPLDUnitStride   : PrimateMOP<0b00>;
def MOPLDIndexedUnord : PrimateMOP<0b01>;
def MOPLDStrided      : PrimateMOP<0b10>;
def MOPLDIndexedOrder : PrimateMOP<0b11>;

def MOPSTUnitStride   : PrimateMOP<0b00>;
def MOPSTIndexedUnord : PrimateMOP<0b01>;
def MOPSTStrided      : PrimateMOP<0b10>;
def MOPSTIndexedOrder : PrimateMOP<0b11>;

class PrimateLSUMOP<bits<5> val> {
  bits<5> Value = val;
}
def LUMOPUnitStride  : PrimateLSUMOP<0b00000>;
def LUMOPUnitStrideMask : PrimateLSUMOP<0b01011>;
def LUMOPUnitStrideWholeReg : PrimateLSUMOP<0b01000>;
def LUMOPUnitStrideFF: PrimateLSUMOP<0b10000>;
def SUMOPUnitStride  : PrimateLSUMOP<0b00000>;
def SUMOPUnitStrideMask : PrimateLSUMOP<0b01011>;
def SUMOPUnitStrideWholeReg : PrimateLSUMOP<0b01000>;

class PrimateAMOOP<bits<5> val> {
  bits<5> Value = val;
}
def AMOOPVamoSwap : PrimateAMOOP<0b00001>;
def AMOOPVamoAdd : PrimateAMOOP<0b00000>;
def AMOOPVamoXor : PrimateAMOOP<0b00100>;
def AMOOPVamoAnd : PrimateAMOOP<0b01100>;
def AMOOPVamoOr : PrimateAMOOP<0b01000>;
def AMOOPVamoMin : PrimateAMOOP<0b10000>;
def AMOOPVamoMax : PrimateAMOOP<0b10100>;
def AMOOPVamoMinu : PrimateAMOOP<0b11000>;
def AMOOPVamoMaxu : PrimateAMOOP<0b11100>;

class PrimateWidth<bits<4> val> {
  bits<4> Value = val;
}
def LSWidth8     : PrimateWidth<0b0000>;
def LSWidth16    : PrimateWidth<0b0101>;
def LSWidth32    : PrimateWidth<0b0110>;
def LSWidth64    : PrimateWidth<0b0111>;

class PRInstSetiVLi<dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI> {
  bits<5> uimm;
  bits<5> rd;
  bits<10> vtypei;

  let Inst{31} = 1;
  let Inst{30} = 1;
  let Inst{29-20} = vtypei{9-0};
  let Inst{19-15} = uimm;
  let Inst{14-12} = 0b111;
  let Inst{11-7} = rd;
  let Opcode = OPC_OP_V.Value;

  let Defs = [VTYPE, VL];
}

class PRInstSetVLi<dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI> {
  bits<5> rs1;
  bits<5> rd;
  bits<11> vtypei;

  let Inst{31} = 0;
  let Inst{30-20} = vtypei;
  let Inst{19-15} = rs1;
  let Inst{14-12} = 0b111;
  let Inst{11-7} = rd;
  let Opcode = OPC_OP_V.Value;

  let Defs = [VTYPE, VL];
}

class PRInstSetVL<dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31} = 1;
  let Inst{30-25} = 0b000000;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = 0b111;
  let Inst{11-7} = rd;
  let Opcode = OPC_OP_V.Value;

  let Defs = [VTYPE, VL];
}

class PRInstVV<bits<6> funct6, PrimateVFormat opv, dag outs, dag ins,
               string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> vs1;
  bits<5> vd;
  bit vm;

  let Inst{31-26} = funct6;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = vs1;
  let Inst{14-12} = opv.Value;
  let Inst{11-7} = vd;
  let Opcode = OPC_OP_V.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstVX<bits<6> funct6, PrimateVFormat opv, dag outs, dag ins,
                string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> rs1;
  bits<5> vd;
  bit vm;

  let Inst{31-26} = funct6;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = opv.Value;
  let Inst{11-7} = vd;
  let Opcode = OPC_OP_V.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstV2<bits<6> funct6, bits<5> vs2, PrimateVFormat opv, dag outs, dag ins,
                string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs1;
  bits<5> vd;
  bit vm;

  let Inst{31-26} = funct6;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = opv.Value;
  let Inst{11-7} = vd;
  let Opcode = OPC_OP_V.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstIVI<bits<6> funct6, dag outs, dag ins, string opcodestr,
                string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> imm;
  bits<5> vd;
  bit vm;

  let Inst{31-26} = funct6;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = imm;
  let Inst{14-12} = 0b011;
  let Inst{11-7} = vd;
  let Opcode = OPC_OP_V.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstV<bits<6> funct6, bits<5> vs1, PrimateVFormat opv, dag outs,
              dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> vd;
  bit vm;

  let Inst{31-26} = funct6;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = vs1;
  let Inst{14-12} = opv.Value;
  let Inst{11-7} = vd;
  let Opcode = OPC_OP_V.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstVLU<bits<3> nf, bit mew, PrimateLSUMOP lumop,
                bits<3> width, dag outs, dag ins, string opcodestr,
                string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs1;
  bits<5> vd;
  bit vm;

  let Inst{31-29} = nf;
  let Inst{28} = mew;
  let Inst{27-26} = MOPLDUnitStride.Value;
  let Inst{25} = vm;
  let Inst{24-20} = lumop.Value;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Inst{11-7} = vd;
  let Opcode = OPC_LOAD_FP.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstVLS<bits<3> nf, bit mew, bits<3> width,
                dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> vd;
  bit vm;

  let Inst{31-29} = nf;
  let Inst{28} = mew;
  let Inst{27-26} = MOPLDStrided.Value;
  let Inst{25} = vm;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Inst{11-7} = vd;
  let Opcode = OPC_LOAD_FP.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstVLX<bits<3> nf, bit mew, PrimateMOP mop, bits<3> width,
                dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> rs1;
  bits<5> vd;
  bit vm;

  let Inst{31-29} = nf;
  let Inst{28} = mew;
  let Inst{27-26} = mop.Value;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Inst{11-7} = vd;
  let Opcode = OPC_LOAD_FP.Value;

  let Uses = [VTYPE, VL];
  let PRVConstraint = VMConstraint;
}

class PRInstVSU<bits<3> nf, bit mew, PrimateLSUMOP sumop,
                bits<3> width, dag outs, dag ins, string opcodestr,
                string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs1;
  bits<5> vs3;
  bit vm;

  let Inst{31-29} = nf;
  let Inst{28} = mew;
  let Inst{27-26} = MOPSTUnitStride.Value;
  let Inst{25} = vm;
  let Inst{24-20} = sumop.Value;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Inst{11-7} = vs3;
  let Opcode = OPC_STORE_FP.Value;

  let Uses = [VTYPE, VL];
}

class PRInstVSS<bits<3> nf, bit mew, bits<3> width,
                dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> vs3;
  bit vm;

  let Inst{31-29} = nf;
  let Inst{28} = mew;
  let Inst{27-26} = MOPSTStrided.Value;
  let Inst{25} = vm;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Inst{11-7} = vs3;
  let Opcode = OPC_STORE_FP.Value;

  let Uses = [VTYPE, VL];
}

class PRInstVSX<bits<3> nf, bit mew, PrimateMOP mop, bits<3> width,
                dag outs, dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> rs1;
  bits<5> vs3;
  bit vm;

  let Inst{31-29} = nf;
  let Inst{28} = mew;
  let Inst{27-26} = mop.Value;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Inst{11-7} = vs3;
  let Opcode = OPC_STORE_FP.Value;

  let Uses = [VTYPE, VL];
}

class PRInstVAMO<PrimateAMOOP amoop, bits<3> width, dag outs, 
                 dag ins, string opcodestr, string argstr>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> vs2;
  bits<5> rs1;
  bit wd;
  bit vm;

  let Inst{31-27} = amoop.Value;
  let Inst{26} = wd;
  let Inst{25} = vm;
  let Inst{24-20} = vs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = width;
  let Opcode = OPC_AMO.Value;

  let Uses = [VTYPE, VL];
}
