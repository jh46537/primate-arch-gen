
  class PRInst<dag outs, dag ins, string opcodestr, string argstr,
             list<dag> pattern, InstFormat format, InstrItinClass itin = ItinGreen>
    : PRInstCommon<outs, ins, opcodestr, argstr, pattern, format> {
    field bits<40> Inst;
    field bits<40> SoftFail = 0;
    let Size = 5;

    bits<7> Opcode = 0;

    let Inst{6-0} = Opcode;
    let Itinerary = itin;
  }
  

  class PRInstR<bits<7> funct7, bits<3> funct3, PrimateOpcode opcode, dag outs,
              dag ins, string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
    : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
  bits<7> rs2;
  bits<7> rs1;
  bits<7> rd;

  let Inst{31-37} = funct7;
  let Inst{24-30} = rs2;
  let Inst{17-23} = rs1;
  let Inst{14-16} = funct3;
  let Inst{7-13}= rd;
  let Opcode = opcode.Value;
  }
  

  class PRInstR4<bits<2> funct2, bits<3> funct3, PrimateOpcode opcode, dag outs,
                dag ins, string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR4, itin> {
    bits<7> rs3;
    bits<7> rs2;
    bits<7> rs1;
    bits<7> rd;

    let Inst{ 39-33 } = rs3;
    let Inst{ 32-31 } = funct2;
    let Inst{ 30-24 } = rs2;
    let Inst{ 23-17 } = rs1;
    let Inst{ 16-14 } = funct3;
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
  
class PRInstR4Frm<bits<2> funct2, PrimateOpcode opcode, dag outs, dag ins,
                    string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR4, itin> {
    bits<7> rs3;
    bits<7> rs2;
    bits<7> rs1;
    bits<3> frm;
    bits<7> rd;

    let Inst{ 39-33 } = rs3;
    let Inst{ 32-31 } = funct2;
    let Inst{ 30-24 } = rs2;
    let Inst{ 23-17 } = rs1;
    let Inst{ 16-14 } = frm;
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstRAtomic<bits<5> funct5, bit aq, bit rl, bits<3> funct3,
                      PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                      string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
    bits<7> rs2;
    bits<7> rs1;
    bits<7> rd;

    let Inst{ 37 - 33 } = funct5;
    let Inst{ 32 } = aq;
    let Inst{ 31 } = rl;
    let Inst{ 30 - 24 } = rs2;
    let Inst{ 23 - 17 } = rs1;
    let Inst{ 16 - 14 } = funct3;
    let Inst{ 13 - 7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstRFrm<bits<7> funct7, PrimateOpcode opcode, dag outs, dag ins,
                  string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatR, itin> {
    bits<7> rs2;
    bits<7> rs1;
    bits<7> rd;
    bits<3> frm;

    let Inst{ 37 - 31 } = funct7;
    let Inst{ 30 - 24 } = rs2;
    let Inst{ 23 - 17 } = rs1;
    let Inst{ 16 - 14 } = frm;
    let Inst{ 13 - 7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstIBase<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                    string opcodestr, string argstr>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI> {
    bits<7> rs1;
    bits<7> rd;

    let Inst{ 23-17 } = rs1;
    let Inst{ 16-14 } = funct3;
    let Inst{ 13-7 } = rd;
    let Inst{ 6-0 } = opcode.Value;
  }
class PRInstI<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
    bits<12> imm12;
    bits<7> rs1;
    bits<7> rd;

    let Inst{ 35-24 } = imm12;
    let Inst{ 23-17 } = rs1;
    let Inst{ 16-14 } = funct3;
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstIShift<bits<5> imm11_7, bits<3> funct3, PrimateOpcode opcode,
                    dag outs, dag ins, string opcodestr, string argstr,
                    InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
    bits<6> shamt;
    bits<7> rs1;
    bits<7> rd;

    let Inst{ 30 } = 0;
    let Inst{ 35-31 } = imm11_7;
    let Inst{ 29-24 } = shamt;
    let Inst{ 23-17 } = rs1;
    let Inst{ 16-14 } = funct3;
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstIShiftW<bits<7> imm11_5, bits<3> funct3, PrimateOpcode opcode,
                      dag outs, dag ins, string opcodestr, string argstr,
                      InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatI, itin> {
    bits<5> shamt;
    bits<7> rs1;
    bits<7> rd;

    let Inst{ 35-29 } = imm11_5;
    let Inst{ 28-24 } = shamt;
    let Inst{ 23-17 } = rs1;
    let Inst{ 16-14 } = funct3;
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstS<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                string opcodestr, string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatS, itin> {
    bits<12> imm12;
    bits<7> rs2;
    bits<7> rs1;

    let Inst{ 35-29 } = imm12{11-5};
    let Inst{ 28-22 } = rs2;
    let Inst{ 21-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-7 } = imm12{4-0};
    let Opcode = opcode.Value;
  }
class PRInstB<bits<3> funct3, PrimateOpcode opcode, dag outs, dag ins,
                string opcodestr, string argstr, InstrItinClass itin = ItinBranch>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatB, itin> {
    bits<12> imm12;
    bits<7> rs2;
    bits<7> rs1;

    let Inst{ 35-35 } = imm12{11};
    let Inst{ 34-29 } = imm12{9-4};
    let Inst{ 28-22 } = rs2;
    let Inst{ 21-15 } = rs1;
    let Inst{ 14-12 } = funct3;
    let Inst{ 11-8 } = imm12{3-0};
    let Inst{ 7-7 } = imm12{10};
    let Opcode = opcode.Value;
  }
class PRInstU<PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                string argstr, InstrItinClass itin = ItinGreen>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatU, itin> {
    bits<20> imm20;
    bits<7> rd;

    let Inst{ 33-14 } = imm20;
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
class PRInstJ<PrimateOpcode opcode, dag outs, dag ins, string opcodestr,
                string argstr, InstrItinClass itin = ItinBranch>
      : PRInst<outs, ins, opcodestr, argstr, [], InstFormatJ, itin> {
    bits<20> imm20;
    bits<7> rd;

    let Inst{ 33 } = imm20{19};
    let Inst{ 32-23 } = imm20{9-0};
    let Inst{ 22 } = imm20{10};
    let Inst{ 21-14 } = imm20{18-11};
    let Inst{ 13-7 } = rd;
    let Opcode = opcode.Value;
  }
