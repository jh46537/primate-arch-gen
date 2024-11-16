
  class PRInst<dag outs, dag ins, string opcodestr, string argstr,
             list<dag> pattern, InstFormat format, InstrItinClass itin = ItinGreen>
    : PRInstCommon<outs, ins, opcodestr, argstr, pattern, format> {
    field bits<32> Inst;
    field bits<32> SoftFail = 0;
    let Size = 4;

    bits<7> Opcode = 0;

    let Inst{6-0} = Opcode;
    let Itinerary = itin;
  }

  class PRInstR<bits<7> funct7, bits<3> funct3, PrimateOpcode opcode, dag outs,
              dag ins, string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{25-31} = funct7;
  let Inst{20-24} = rs2;
  let Inst{15-19} = rs1;
  let Inst{12-14} = funct3;
  let Inst{7-11}= rd;
  let Opcode = opcode.Value;
  }

  class PRInstR4<bits<2> funct2, bits<3> funct3, PrimateOpcode opcode, dag outs,
                dag ins, string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR4, itin> {
    bits<5> rs3;
    bits<5> rs2;
    bits<5> rs1;
    bits<5> rd;

    let Inst{ 31-27 } = rs3;
    let Inst{ 26-25 } = funct2;
    let Inst{ 24-20 } = rs2;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstR4Frm<bits<2> funct2, PrimateOpcode opcode, dag outs, dag ins,
                    string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR4, itin> {
    bits<5> rs3;
    bits<5> rs2;
    bits<5> rs1;
    bits<3> frm;
    bits<5> rd;

    let Inst{ 31-27 } = rs3;
    let Inst{ 26-25 } = funct2;
    let Inst{ 24-20 } = rs2;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = frm;
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstRAtomic<bits<5> funct5, bit aq, bit rl, bits<3> funct3,
                      PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                      string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
    bits<5> rs2;
    bits<5> rs1;
    bits<5> rd;

    let Inst{ 31 - 27 } = funct5;
    let Inst{ 26 } = aq;
    let Inst{ 25 } = rl;
    let Inst{ 24 - 20 } = rs2;
    let Inst{ 19 - 15 } = rs1;
    let Inst{ 14 - 12 } = funct3;
    let Inst{ 11 - 7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstRFrm<bits<7> funct7, PrimateOpcode opcode, dag outs, dag ins,
                  string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
    bits<5> rs2;
    bits<5> rs1;
    bits<5> rd;
    bits<3> frm;

    let Inst{ 31 - 25 } = funct7;
    let Inst{ 24 - 20 } = rs2;
    let Inst{ 19 - 15 } = rs1;
    let Inst{ 14 - 12 } = frm;
    let Inst{ 11 - 7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstIBase<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                    string opcodestr, string argstr>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI> {
    bits<5> rs1;
    bits<5> rd;

    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = rd;
    let Inst{ 6-0 } = opcode.Value;
  }
class PRInstI<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
    bits<12> imm12;
    bits<5> rs1;
    bits<5> rd;

    let Inst{ 31-20 } = imm12;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstIShift<bits<5> imm11_7, bits<3> funct3, PrimateOpcode opcode,
                    dag outs, dag ins, string opcodestr, string argstr,
                    InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
    bits<6> shamt;
    bits<5> rs1;
    bits<5> rd;

    let Inst{ 26 } = 0;
    let Inst{ 31-27 } = imm11_7;
    let Inst{ 25-20 } = shamt;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstIShiftW<bits<7> imm11_5, bits<3> funct3, PrimateOpcode opcode,
                      dag outs, dag ins, string opcodestr, string argstr,
                      InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
    bits<5> shamt;
    bits<5> rs1;
    bits<5> rd;

    let Inst{ 31-25 } = imm11_5;
    let Inst{ 24-20 } = shamt;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstS<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatS, itin> {
    bits<12> imm12;
    bits<5> rs2;
    bits<5> rs1;

    let Inst{ 31-25 } = imm12{11-5};
    let Inst{ 24-20 } = rs2;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = imm12{4-0};
    let Opcode = opcode.Value;
  }
class PRInstB<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                string opcodestr, string argstr, InstrItinClass itin = ItinBranch>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatB, itin> {
    bits<12> imm12;
    bits<5> rs2;
    bits<5> rs1;

    let Inst{ 31-31 } = imm12{11};
    let Inst{ 30-25 } = imm12{9-4};
    let Inst{ 24-20 } = rs2;
    let Inst{ 19-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-8 } = imm12{3-0};
    let Inst{ 7-7 } = imm12{10};
    let Opcode = opcode.Value;
  }
class PRInstU<PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatU, itin> {
    bits<20> imm20;
    bits<5> rd;

    let Inst{ 31-12 } = imm20;
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstJ<PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                string argstr, InstrItinClass itin = ItinBranch>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatJ, itin> {
    bits<20> imm20;
    bits<5> rd;

    let Inst{ 31 } = imm20{19};
    let Inst{ 30-21 } = imm20{9-0};
    let Inst{ 20 } = imm20{10};
    let Inst{ 19-12 } = imm20{18-11};
    let Inst{ 11-7 } = rd;
    let Opcode = opcode.Value;
  }
