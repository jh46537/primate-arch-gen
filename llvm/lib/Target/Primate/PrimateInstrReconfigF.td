
  let hasSideEffects = 0, mayLoad = 0, mayStore = 0, mayRaiseFPException = 1 in
  class FPUnaryOp_r<bits<7> funct7, bits<5> rs2val, bits<3> funct3,
                    DAGOperand rdty, DAGOperand rs1ty, string opcodestr>
      : PRInstR<funct7, funct3, OPC_OP_FP, (outs rdty:$rd), (ins rs1ty:$rs1),
                opcodestr, "$rd, $rs1"> {
    let rs2 = rs2val;
  }
  multiclass FPUnaryOp_r_m<bits<7> funct7, bits<5> rs2val, bits<3> funct3,
                          ExtInfo Ext, DAGOperand rdty, DAGOperand rs1ty,
                          string opcodestr> {
    let Predicates = Ext.Predicates, DecoderNamespace = Ext.Space in
    def Ext.Suffix : FPUnaryOp_r<funct7, rs2val, funct3, rdty, rs1ty, opcodestr>;
  }

  let hasSideEffects = 0, mayLoad = 0, mayStore = 0, mayRaiseFPException = 1,
      UseNamedOperandTable = 1, hasPostISelHook = 1 in
  class FPUnaryOp_r_frm<bits<7> funct7, bits<5> rs2val, DAGOperand rdty,
                        DAGOperand rs1ty, string opcodestr>
      : PRInstRFrm<funct7, OPC_OP_FP, (outs rdty:$rd),
                  (ins rs1ty:$rs1, frmarg:$frm), opcodestr,
                    "$rd, $rs1$frm"> {
    let rs2 = rs2val;
  }
  multiclass FPUnaryOp_r_frm_m<bits<7> funct7, bits<5> rs2val,
                              ExtInfo Ext, DAGOperand rdty, DAGOperand rs1ty,
                              string opcodestr, list<Predicate> ExtraPreds = []> {
    let Predicates = !listconcat(Ext.Predicates, ExtraPreds),
        DecoderNamespace = Ext.Space in
    def Ext.Suffix : FPUnaryOp_r_frm<funct7, rs2val, rdty, rs1ty,
                                    opcodestr>;
  }

  let hasSideEffects = 0, mayLoad = 0, mayStore = 0, mayRaiseFPException = 1,
      UseNamedOperandTable = 1, hasPostISelHook = 1 in
  class FPUnaryOp_r_frmlegacy<bits<7> funct7, bits<5> rs2val, DAGOperand rdty,
                              DAGOperand rs1ty, string opcodestr>
      : PRInstRFrm<funct7, OPC_OP_FP, (outs rdty:$rd),
                  (ins rs1ty:$rs1, frmarglegacy:$frm), opcodestr,
                    "$rd, $rs1$frm"> {
    let rs2 = rs2val;
  }
  multiclass FPUnaryOp_r_frmlegacy_m<bits<7> funct7, bits<5> rs2val,
                                    ExtInfo Ext, DAGOperand rdty, DAGOperand rs1ty,
                                    string opcodestr, list<Predicate> ExtraPreds = []> {
    let Predicates = !listconcat(Ext.Predicates, ExtraPreds),
        DecoderNamespace = Ext.Space in
    def Ext.Suffix : FPUnaryOp_r_frmlegacy<funct7, rs2val, rdty, rs1ty,
                                          opcodestr>;
  }
  