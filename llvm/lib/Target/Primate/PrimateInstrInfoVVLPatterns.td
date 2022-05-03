//===-- PrimateInstrInfoVVLPatterns.td - PRCV VL patterns --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file contains the required infrastructure and VL patterns to
/// support code generation for the standard 'V' (Vector) extension, version
/// 0.10.  This version is still experimental as the 'V' extension hasn't been
/// ratified yet.
///
/// This file is included from and depends upon PrimateInstrInfoVPseudos.td
///
/// Note: the patterns for PRV intrinsics are found in
/// PrimateInstrInfoVPseudos.td.
///
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Helpers to define the VL patterns.
//===----------------------------------------------------------------------===//

def SDT_PrimateVLE_VL : SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisPtrTy<1>,
                                           SDTCisVT<2, XLenVT>]>;
def SDT_PrimateVSE_VL : SDTypeProfile<0, 3, [SDTCisVec<0>, SDTCisPtrTy<1>,
                                           SDTCisVT<2, XLenVT>]>;

def SDT_PrimateIntBinOp_VL : SDTypeProfile<1, 4, [SDTCisSameAs<0, 1>,
                                                SDTCisSameAs<0, 2>,
                                                SDTCisVec<0>, SDTCisInt<0>,
                                                SDTCVecEltisVT<3, i1>,
                                                SDTCisSameNumEltsAs<0, 3>,
                                                SDTCisVT<4, XLenVT>]>;

def SDT_PrimateFPUnOp_VL : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                              SDTCisVec<0>, SDTCisFP<0>,
                                              SDTCVecEltisVT<2, i1>,
                                              SDTCisSameNumEltsAs<0, 2>,
                                              SDTCisVT<3, XLenVT>]>;
def SDT_PrimateFPBinOp_VL : SDTypeProfile<1, 4, [SDTCisSameAs<0, 1>,
                                               SDTCisSameAs<0, 2>,
                                               SDTCisVec<0>, SDTCisFP<0>,
                                               SDTCVecEltisVT<3, i1>,
                                               SDTCisSameNumEltsAs<0, 3>,
                                               SDTCisVT<4, XLenVT>]>;

def primate_vmv_v_x_vl : SDNode<"PrimateISD::VMV_V_X_VL",
                              SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisInt<0>,
                                                   SDTCisVT<1, XLenVT>,
                                                   SDTCisVT<2, XLenVT>]>>;
def primate_vfmv_v_f_vl : SDNode<"PrimateISD::VFMV_V_F_VL",
                               SDTypeProfile<1, 2, [SDTCisVec<0>, SDTCisFP<0>,
                                                    SDTCisEltOfVec<1, 0>,
                                                    SDTCisVT<2, XLenVT>]>>;
def primate_vmv_s_x_vl : SDNode<"PrimateISD::VMV_S_X_VL",
                              SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                   SDTCisInt<0>,
                                                   SDTCisVT<2, XLenVT>,
                                                   SDTCisVT<3, XLenVT>]>>;
def primate_vfmv_s_f_vl : SDNode<"PrimateISD::VFMV_S_F_VL",
                               SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                    SDTCisFP<0>,
                                                    SDTCisEltOfVec<2, 0>,
                                                    SDTCisVT<3, XLenVT>]>>;

def primate_vle_vl : SDNode<"PrimateISD::VLE_VL", SDT_PrimateVLE_VL,
                          [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def primate_vse_vl : SDNode<"PrimateISD::VSE_VL", SDT_PrimateVSE_VL,
                          [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;

def primate_add_vl   : SDNode<"PrimateISD::ADD_VL",   SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_sub_vl   : SDNode<"PrimateISD::SUB_VL",   SDT_PrimateIntBinOp_VL>;
def primate_mul_vl   : SDNode<"PrimateISD::MUL_VL",   SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_mulhs_vl : SDNode<"PrimateISD::MULHS_VL", SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_mulhu_vl : SDNode<"PrimateISD::MULHU_VL", SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_and_vl   : SDNode<"PrimateISD::AND_VL",   SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_or_vl    : SDNode<"PrimateISD::OR_VL",    SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_xor_vl   : SDNode<"PrimateISD::XOR_VL",   SDT_PrimateIntBinOp_VL, [SDNPCommutative]>;
def primate_sdiv_vl  : SDNode<"PrimateISD::SDIV_VL",  SDT_PrimateIntBinOp_VL>;
def primate_srem_vl  : SDNode<"PrimateISD::SREM_VL",  SDT_PrimateIntBinOp_VL>;
def primate_udiv_vl  : SDNode<"PrimateISD::UDIV_VL",  SDT_PrimateIntBinOp_VL>;
def primate_urem_vl  : SDNode<"PrimateISD::UREM_VL",  SDT_PrimateIntBinOp_VL>;
def primate_shl_vl   : SDNode<"PrimateISD::SHL_VL",   SDT_PrimateIntBinOp_VL>;
def primate_sra_vl   : SDNode<"PrimateISD::SRA_VL",   SDT_PrimateIntBinOp_VL>;
def primate_srl_vl   : SDNode<"PrimateISD::SRL_VL",   SDT_PrimateIntBinOp_VL>;
def primate_smin_vl  : SDNode<"PrimateISD::SMIN_VL",  SDT_PrimateIntBinOp_VL>;
def primate_smax_vl  : SDNode<"PrimateISD::SMAX_VL",  SDT_PrimateIntBinOp_VL>;
def primate_umin_vl  : SDNode<"PrimateISD::UMIN_VL",  SDT_PrimateIntBinOp_VL>;
def primate_umax_vl  : SDNode<"PrimateISD::UMAX_VL",  SDT_PrimateIntBinOp_VL>;

def primate_saddsat_vl   : SDNode<"PrimateISD::SADDSAT_VL", SDT_PrimateIntBinOp_VL>;
def primate_uaddsat_vl   : SDNode<"PrimateISD::UADDSAT_VL", SDT_PrimateIntBinOp_VL>;
def primate_ssubsat_vl   : SDNode<"PrimateISD::SSUBSAT_VL", SDT_PrimateIntBinOp_VL>;
def primate_usubsat_vl   : SDNode<"PrimateISD::USUBSAT_VL", SDT_PrimateIntBinOp_VL>;

def primate_fadd_vl  : SDNode<"PrimateISD::FADD_VL",  SDT_PrimateFPBinOp_VL, [SDNPCommutative]>;
def primate_fsub_vl  : SDNode<"PrimateISD::FSUB_VL",  SDT_PrimateFPBinOp_VL>;
def primate_fmul_vl  : SDNode<"PrimateISD::FMUL_VL",  SDT_PrimateFPBinOp_VL, [SDNPCommutative]>;
def primate_fdiv_vl  : SDNode<"PrimateISD::FDIV_VL",  SDT_PrimateFPBinOp_VL>;
def primate_fneg_vl  : SDNode<"PrimateISD::FNEG_VL",  SDT_PrimateFPUnOp_VL>;
def primate_fabs_vl  : SDNode<"PrimateISD::FABS_VL",  SDT_PrimateFPUnOp_VL>;
def primate_fsqrt_vl : SDNode<"PrimateISD::FSQRT_VL", SDT_PrimateFPUnOp_VL>;
def primate_fcopysign_vl : SDNode<"PrimateISD::FCOPYSIGN_VL",  SDT_PrimateFPBinOp_VL>;
def primate_fminnum_vl   : SDNode<"PrimateISD::FMINNUM_VL",  SDT_PrimateFPBinOp_VL>;
def primate_fmaxnum_vl   : SDNode<"PrimateISD::FMAXNUM_VL",  SDT_PrimateFPBinOp_VL>;

def SDT_PrimateVecFMA_VL : SDTypeProfile<1, 5, [SDTCisSameAs<0, 1>,
                                              SDTCisSameAs<0, 2>,
                                              SDTCisSameAs<0, 3>,
                                              SDTCisVec<0>, SDTCisFP<0>,
                                              SDTCVecEltisVT<4, i1>,
                                              SDTCisSameNumEltsAs<0, 4>,
                                              SDTCisVT<5, XLenVT>]>;
def primate_fma_vl : SDNode<"PrimateISD::FMA_VL", SDT_PrimateVecFMA_VL, [SDNPCommutative]>;

def SDT_PrimateFPRoundOp_VL  : SDTypeProfile<1, 3, [
  SDTCisFP<0>, SDTCisFP<1>, SDTCisOpSmallerThanOp<0, 1>, SDTCisSameNumEltsAs<0, 1>,
  SDTCVecEltisVT<2, i1>, SDTCisSameNumEltsAs<1, 2>, SDTCisVT<3, XLenVT>
]>;
def SDT_PrimateFPExtendOp_VL  : SDTypeProfile<1, 3, [
  SDTCisFP<0>, SDTCisFP<1>, SDTCisOpSmallerThanOp<1, 0>, SDTCisSameNumEltsAs<0, 1>,
  SDTCVecEltisVT<2, i1>, SDTCisSameNumEltsAs<1, 2>, SDTCisVT<3, XLenVT>
]>;

def primate_fpround_vl : SDNode<"PrimateISD::FP_ROUND_VL", SDT_PrimateFPRoundOp_VL>;
def primate_fpextend_vl : SDNode<"PrimateISD::FP_EXTEND_VL", SDT_PrimateFPExtendOp_VL>;
def primate_fncvt_rod_vl : SDNode<"PrimateISD::VFNCVT_ROD_VL", SDT_PrimateFPRoundOp_VL>;

def SDT_PrimateFP2IOp_VL  : SDTypeProfile<1, 3, [
  SDTCisInt<0>, SDTCisFP<1>, SDTCisSameNumEltsAs<0, 1>,
  SDTCVecEltisVT<2, i1>, SDTCisSameNumEltsAs<1, 2>, SDTCisVT<3, XLenVT>
]>;
def SDT_PrimateI2FPOp_VL  : SDTypeProfile<1, 3, [
  SDTCisFP<0>, SDTCisInt<1>, SDTCisSameNumEltsAs<0, 1>,
  SDTCVecEltisVT<2, i1>, SDTCisSameNumEltsAs<1, 2>, SDTCisVT<3, XLenVT>
]>;

def primate_fp_to_sint_vl : SDNode<"PrimateISD::FP_TO_SINT_VL", SDT_PrimateFP2IOp_VL>;
def primate_fp_to_uint_vl : SDNode<"PrimateISD::FP_TO_UINT_VL", SDT_PrimateFP2IOp_VL>;
def primate_sint_to_fp_vl : SDNode<"PrimateISD::SINT_TO_FP_VL", SDT_PrimateI2FPOp_VL>;
def primate_uint_to_fp_vl : SDNode<"PrimateISD::UINT_TO_FP_VL", SDT_PrimateI2FPOp_VL>;

def primate_setcc_vl : SDNode<"PrimateISD::SETCC_VL",
                            SDTypeProfile<1, 5, [SDTCVecEltisVT<0, i1>,
                                                 SDTCisVec<1>,
                                                 SDTCisSameNumEltsAs<0, 1>,
                                                 SDTCisSameAs<1, 2>,
                                                 SDTCisVT<3, OtherVT>,
                                                 SDTCisSameAs<0, 4>,
                                                 SDTCisVT<5, XLenVT>]>>;

def primate_vrgather_vx_vl : SDNode<"PrimateISD::VRGATHER_VX_VL",
                                  SDTypeProfile<1, 4, [SDTCisVec<0>,
                                                       SDTCisSameAs<0, 1>,
                                                       SDTCisVT<2, XLenVT>,
                                                       SDTCVecEltisVT<3, i1>,
                                                       SDTCisSameNumEltsAs<0, 3>,
                                                       SDTCisVT<4, XLenVT>]>>;
def primate_vrgather_vv_vl : SDNode<"PrimateISD::VRGATHER_VV_VL",
                                  SDTypeProfile<1, 4, [SDTCisVec<0>,
                                                       SDTCisSameAs<0, 1>,
                                                       SDTCisInt<2>,
                                                       SDTCisSameNumEltsAs<0, 2>,
                                                       SDTCisSameSizeAs<0, 2>,
                                                       SDTCVecEltisVT<3, i1>,
                                                       SDTCisSameNumEltsAs<0, 3>,
                                                       SDTCisVT<4, XLenVT>]>>;
def primate_vrgatherei16_vv_vl : SDNode<"PrimateISD::VRGATHEREI16_VV_VL",
                                      SDTypeProfile<1, 4, [SDTCisVec<0>,
                                                           SDTCisSameAs<0, 1>,
                                                           SDTCisInt<2>,
                                                           SDTCVecEltisVT<2, i16>,
                                                           SDTCisSameNumEltsAs<0, 2>,
                                                           SDTCVecEltisVT<3, i1>,
                                                           SDTCisSameNumEltsAs<0, 3>,
                                                           SDTCisVT<4, XLenVT>]>>;

def primate_vselect_vl : SDNode<"PrimateISD::VSELECT_VL",
                              SDTypeProfile<1, 4, [SDTCisVec<0>,
                                                   SDTCisVec<1>,
                                                   SDTCisSameNumEltsAs<0, 1>,
                                                   SDTCVecEltisVT<1, i1>,
                                                   SDTCisSameAs<0, 2>,
                                                   SDTCisSameAs<2, 3>,
                                                   SDTCisVT<4, XLenVT>]>>;

def SDT_PrimateMaskBinOp_VL : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                 SDTCisSameAs<0, 2>,
                                                 SDTCVecEltisVT<0, i1>,
                                                 SDTCisVT<3, XLenVT>]>;
def primate_vmand_vl : SDNode<"PrimateISD::VMAND_VL", SDT_PrimateMaskBinOp_VL, [SDNPCommutative]>;
def primate_vmor_vl  : SDNode<"PrimateISD::VMOR_VL",  SDT_PrimateMaskBinOp_VL, [SDNPCommutative]>;
def primate_vmxor_vl : SDNode<"PrimateISD::VMXOR_VL", SDT_PrimateMaskBinOp_VL, [SDNPCommutative]>;

def true_mask : PatLeaf<(primate_vmset_vl (XLenVT srcvalue))>;

def primate_vmnot_vl : PatFrag<(ops node:$rs, node:$vl),
                             (primate_vmxor_vl node:$rs, true_mask, node:$vl)>;

def primate_vpopc_vl : SDNode<"PrimateISD::VPOPC_VL",
                            SDTypeProfile<1, 3, [SDTCisVT<0, XLenVT>,
                                                 SDTCisVec<1>, SDTCisInt<1>,
                                                 SDTCVecEltisVT<2, i1>,
                                                 SDTCisSameNumEltsAs<1, 2>,
                                                 SDTCisVT<3, XLenVT>]>>;

def SDT_PrimateVEXTEND_VL : SDTypeProfile<1, 3, [SDTCisVec<0>,
                                               SDTCisSameNumEltsAs<0, 1>,
                                               SDTCisSameNumEltsAs<1, 2>,
                                               SDTCVecEltisVT<2, i1>,
                                               SDTCisVT<3, XLenVT>]>;
def primate_sext_vl : SDNode<"PrimateISD::VSEXT_VL", SDT_PrimateVEXTEND_VL>;
def primate_zext_vl : SDNode<"PrimateISD::VZEXT_VL", SDT_PrimateVEXTEND_VL>;

def primate_trunc_vector_vl : SDNode<"PrimateISD::TRUNCATE_VECTOR_VL",
                                   SDTypeProfile<1, 3, [SDTCisVec<0>,
                                                        SDTCisVec<1>,
                                                        SDTCisSameNumEltsAs<0, 2>,
                                                        SDTCVecEltisVT<2, i1>,
                                                        SDTCisVT<3, XLenVT>]>>;

def SDT_PrimateVWMUL_VL : SDTypeProfile<1, 4, [SDTCisVec<0>,
                                             SDTCisSameNumEltsAs<0, 1>,
                                             SDTCisSameAs<1, 2>,
                                             SDTCisSameNumEltsAs<1, 3>,
                                             SDTCVecEltisVT<3, i1>,
                                             SDTCisVT<4, XLenVT>]>;
def primate_vwmul_vl  : SDNode<"PrimateISD::VWMUL_VL",  SDT_PrimateVWMUL_VL, [SDNPCommutative]>;
def primate_vwmulu_vl : SDNode<"PrimateISD::VWMULU_VL", SDT_PrimateVWMUL_VL, [SDNPCommutative]>;

def SDTPRVVecReduce : SDTypeProfile<1, 4, [
  SDTCisVec<0>, SDTCisVec<1>, SDTCisSameAs<0, 2>, SDTCVecEltisVT<3, i1>,
  SDTCisSameNumEltsAs<1, 3>, SDTCisVT<4, XLenVT>
]>;

def primate_mul_vl_oneuse : PatFrag<(ops node:$A, node:$B, node:$C, node:$D),
                                  (primate_mul_vl node:$A, node:$B, node:$C,
                                                node:$D), [{
  return N->hasOneUse();
}]>;

def primate_vwmul_vl_oneuse : PatFrag<(ops node:$A, node:$B, node:$C, node:$D),
                                    (primate_vwmul_vl node:$A, node:$B, node:$C,
                                                    node:$D), [{
  return N->hasOneUse();
}]>;

def primate_vwmulu_vl_oneuse : PatFrag<(ops node:$A, node:$B, node:$C, node:$D),
                                     (primate_vwmulu_vl node:$A, node:$B, node:$C,
                                                      node:$D), [{
  return N->hasOneUse();
}]>;

foreach kind = ["ADD", "UMAX", "SMAX", "UMIN", "SMIN", "AND", "OR", "XOR",
                "FADD", "SEQ_FADD", "FMIN", "FMAX"] in
  def rvv_vecreduce_#kind#_vl : SDNode<"PrimateISD::VECREDUCE_"#kind#"_VL", SDTPRVVecReduce>;

// Ignore the vl operand.
def SplatFPOp : PatFrag<(ops node:$op),
                        (primate_vfmv_v_f_vl node:$op, srcvalue)>;

def sew8simm5  : ComplexPattern<XLenVT, 1, "selectPRVSimm5<8>",  []>;
def sew16simm5 : ComplexPattern<XLenVT, 1, "selectPRVSimm5<16>", []>;
def sew32simm5 : ComplexPattern<XLenVT, 1, "selectPRVSimm5<32>", []>;
def sew64simm5 : ComplexPattern<XLenVT, 1, "selectPRVSimm5<64>", []>;

multiclass VPatBinaryVL_VV<SDNode vop,
                           string instruction_name,
                           ValueType result_type,
                           ValueType op_type,
                           ValueType mask_type,
                           int sew,
                           LMULInfo vlmul,
                           VReg RetClass,
                           VReg op_reg_class> {
  def : Pat<(result_type (vop
                         (op_type op_reg_class:$rs1),
                         (op_type op_reg_class:$rs2),
                         (mask_type true_mask),
                         VLOpFrag)),
            (!cast<Instruction>(instruction_name#"_VV_"# vlmul.MX)
                         op_reg_class:$rs1,
                         op_reg_class:$rs2,
                         GPR:$vl, sew)>;
  def : Pat<(result_type (vop
                         (op_type op_reg_class:$rs1),
                         (op_type op_reg_class:$rs2),
                         (mask_type VMV0:$vm),
                         VLOpFrag)),
        (!cast<Instruction>(instruction_name#"_VV_"# vlmul.MX#"_MASK")
                     (result_type (IMPLICIT_DEF)),
                     op_reg_class:$rs1,
                     op_reg_class:$rs2,
                     VMV0:$vm, GPR:$vl, sew)>;
}

multiclass VPatBinaryVL_XI<SDNode vop,
                           string instruction_name,
                           string suffix,
                           ValueType result_type,
                           ValueType vop_type,
                           ValueType mask_type,
                           int sew,
                           LMULInfo vlmul,
                           VReg RetClass,
                           VReg vop_reg_class,
                           ComplexPattern SplatPatKind,
                           DAGOperand xop_kind> {
  def : Pat<(result_type (vop
                     (vop_type vop_reg_class:$rs1),
                     (vop_type (SplatPatKind (XLenVT xop_kind:$rs2))),
                     (mask_type true_mask),
                     VLOpFrag)),
        (!cast<Instruction>(instruction_name#_#suffix#_# vlmul.MX)
                     vop_reg_class:$rs1,
                     xop_kind:$rs2,
                     GPR:$vl, sew)>;
  def : Pat<(result_type (vop
                     (vop_type vop_reg_class:$rs1),
                     (vop_type (SplatPatKind (XLenVT xop_kind:$rs2))),
                     (mask_type VMV0:$vm),
                     VLOpFrag)),
        (!cast<Instruction>(instruction_name#_#suffix#_# vlmul.MX#"_MASK")
                     (result_type (IMPLICIT_DEF)),
                     vop_reg_class:$rs1,
                     xop_kind:$rs2,
                     VMV0:$vm, GPR:$vl, sew)>;
}

multiclass VPatBinaryVL_VV_VX<SDNode vop, string instruction_name> {
  foreach vti = AllIntegerVectors in {
    defm : VPatBinaryVL_VV<vop, instruction_name,
                           vti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                           vti.LMul, vti.RegClass, vti.RegClass>;
    defm : VPatBinaryVL_XI<vop, instruction_name, "VX",
                           vti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                           vti.LMul, vti.RegClass, vti.RegClass,
                           SplatPat, GPR>;
  }
}

multiclass VPatBinaryVL_VV_VX_VI<SDNode vop, string instruction_name,
                                 Operand ImmType = simm5>
    : VPatBinaryVL_VV_VX<vop, instruction_name> {
  foreach vti = AllIntegerVectors in {
    defm : VPatBinaryVL_XI<vop, instruction_name, "VI",
                           vti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                           vti.LMul, vti.RegClass, vti.RegClass,
                           !cast<ComplexPattern>(SplatPat#_#ImmType),
                           ImmType>;
  }
}

multiclass VPatBinaryWVL_VV_VX<SDNode vop, string instruction_name> {
  foreach VtiToWti = AllWidenableIntVectors in {
    defvar vti = VtiToWti.Vti;
    defvar wti = VtiToWti.Wti;
    defm : VPatBinaryVL_VV<vop, instruction_name,
                           wti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                           vti.LMul, wti.RegClass, vti.RegClass>;
    defm : VPatBinaryVL_XI<vop, instruction_name, "VX",
                           wti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                           vti.LMul, wti.RegClass, vti.RegClass,
                           SplatPat, GPR>;
  }
}

class VPatBinaryVL_VF<SDNode vop,
                      string instruction_name,
                      ValueType result_type,
                      ValueType vop_type,
                      ValueType mask_type,
                      int sew,
                      LMULInfo vlmul,
                      VReg RetClass,
                      VReg vop_reg_class,
                      RegisterClass scalar_reg_class> :
    Pat<(result_type (vop (vop_type vop_reg_class:$rs1),
                          (vop_type (SplatFPOp scalar_reg_class:$rs2)),
                          (mask_type true_mask),
                          VLOpFrag)),
        (!cast<Instruction>(instruction_name#"_"#vlmul.MX)
                     vop_reg_class:$rs1,
                     scalar_reg_class:$rs2,
                     GPR:$vl, sew)>;

multiclass VPatBinaryFPVL_VV_VF<SDNode vop, string instruction_name> {
  foreach vti = AllFloatVectors in {
    defm : VPatBinaryVL_VV<vop, instruction_name,
                           vti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                           vti.LMul, vti.RegClass, vti.RegClass>;
    def : VPatBinaryVL_VF<vop, instruction_name#"_V"#vti.ScalarSuffix,
                          vti.Vector, vti.Vector, vti.Mask, vti.Log2SEW,
                          vti.LMul, vti.RegClass, vti.RegClass,
                          vti.ScalarRegClass>;
  }
}

multiclass VPatBinaryFPVL_R_VF<SDNode vop, string instruction_name> {
  foreach fvti = AllFloatVectors in
    def : Pat<(fvti.Vector (vop (SplatFPOp fvti.ScalarRegClass:$rs2),
                                fvti.RegClass:$rs1,
                                (fvti.Mask true_mask),
                                VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_V"#fvti.ScalarSuffix#"_"#fvti.LMul.MX)
                           fvti.RegClass:$rs1, fvti.ScalarRegClass:$rs2,
                           GPR:$vl, fvti.Log2SEW)>;
}

multiclass VPatIntegerSetCCVL_VV<VTypeInfo vti, string instruction_name,
                                 CondCode cc> {
  def : Pat<(vti.Mask (primate_setcc_vl (vti.Vector vti.RegClass:$rs1),
                                      vti.RegClass:$rs2, cc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>(instruction_name#"_VV_"#vti.LMul.MX)
                         vti.RegClass:$rs1, vti.RegClass:$rs2, GPR:$vl,
                         vti.Log2SEW)>;
}

// Inherits from VPatIntegerSetCCVL_VV and adds a pattern with operands swapped.
multiclass VPatIntegerSetCCVL_VV_Swappable<VTypeInfo vti, string instruction_name,
                                           CondCode cc, CondCode invcc> :
  VPatIntegerSetCCVL_VV<vti, instruction_name, cc> {
  def : Pat<(vti.Mask (primate_setcc_vl (vti.Vector vti.RegClass:$rs2),
                                      vti.RegClass:$rs1, invcc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>(instruction_name#"_VV_"#vti.LMul.MX)
                         vti.RegClass:$rs1, vti.RegClass:$rs2, GPR:$vl,
                         vti.Log2SEW)>;
}

multiclass VPatIntegerSetCCVL_VX_Swappable<VTypeInfo vti, string instruction_name,
                                           CondCode cc, CondCode invcc> {
  defvar instruction = !cast<Instruction>(instruction_name#"_VX_"#vti.LMul.MX);
  def : Pat<(vti.Mask (primate_setcc_vl (vti.Vector vti.RegClass:$rs1),
                                      (SplatPat (XLenVT GPR:$rs2)), cc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (instruction vti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Mask (primate_setcc_vl (SplatPat (XLenVT GPR:$rs2)),
                                      (vti.Vector vti.RegClass:$rs1), invcc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (instruction vti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
}

multiclass VPatIntegerSetCCVL_VI_Swappable<VTypeInfo vti, string instruction_name,
                                           CondCode cc, CondCode invcc> {
  defvar instruction = !cast<Instruction>(instruction_name#"_VI_"#vti.LMul.MX);
  def : Pat<(vti.Mask (primate_setcc_vl (vti.Vector vti.RegClass:$rs1),
                                      (SplatPat_simm5 simm5:$rs2), cc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (instruction vti.RegClass:$rs1, XLenVT:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Mask (primate_setcc_vl (SplatPat_simm5 simm5:$rs2),
                                      (vti.Vector vti.RegClass:$rs1), invcc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (instruction vti.RegClass:$rs1, simm5:$rs2, GPR:$vl, vti.Log2SEW)>;
}

multiclass VPatIntegerSetCCVL_VIPlus1<VTypeInfo vti, string instruction_name,
                                      CondCode cc, ComplexPattern splatpat_kind> {
  defvar instruction = !cast<Instruction>(instruction_name#"_VI_"#vti.LMul.MX);
  def : Pat<(vti.Mask (primate_setcc_vl (vti.Vector vti.RegClass:$rs1),
                                      (splatpat_kind simm5:$rs2), cc,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (instruction vti.RegClass:$rs1, (DecImm simm5:$rs2),
                         GPR:$vl, vti.Log2SEW)>;
}

multiclass VPatFPSetCCVL_VV_VF_FV<CondCode cc,
                                  string inst_name,
                                  string swapped_op_inst_name> {
  foreach fvti = AllFloatVectors in {
    def : Pat<(fvti.Mask (primate_setcc_vl (fvti.Vector fvti.RegClass:$rs1),
                                         fvti.RegClass:$rs2,
                                         cc,
                                         (fvti.Mask true_mask),
                                         VLOpFrag)),
              (!cast<Instruction>(inst_name#"_VV_"#fvti.LMul.MX)
                  fvti.RegClass:$rs1, fvti.RegClass:$rs2, GPR:$vl, fvti.Log2SEW)>;
    def : Pat<(fvti.Mask (primate_setcc_vl (fvti.Vector fvti.RegClass:$rs1),
                                         (SplatFPOp fvti.ScalarRegClass:$rs2),
                                         cc,
                                         (fvti.Mask true_mask),
                                         VLOpFrag)),
              (!cast<Instruction>(inst_name#"_V"#fvti.ScalarSuffix#"_"#fvti.LMul.MX)
                  fvti.RegClass:$rs1, fvti.ScalarRegClass:$rs2,
                  GPR:$vl, fvti.Log2SEW)>;
    def : Pat<(fvti.Mask (primate_setcc_vl (SplatFPOp fvti.ScalarRegClass:$rs2),
                                         (fvti.Vector fvti.RegClass:$rs1),
                                         cc,
                                         (fvti.Mask true_mask),
                                         VLOpFrag)),
              (!cast<Instruction>(swapped_op_inst_name#"_V"#fvti.ScalarSuffix#"_"#fvti.LMul.MX)
                  fvti.RegClass:$rs1, fvti.ScalarRegClass:$rs2,
                  GPR:$vl, fvti.Log2SEW)>;
  }
}

multiclass VPatExtendSDNode_V_VL<SDNode vop, string inst_name, string suffix,
                                 list <VTypeInfoToFraction> fraction_list> {
  foreach vtiTofti = fraction_list in {
    defvar vti = vtiTofti.Vti;
    defvar fti = vtiTofti.Fti;
    def : Pat<(vti.Vector (vop (fti.Vector fti.RegClass:$rs2),
                               true_mask, VLOpFrag)),
              (!cast<Instruction>(inst_name#"_"#suffix#"_"#vti.LMul.MX)
                  fti.RegClass:$rs2, GPR:$vl, vti.Log2SEW)>;
  }
}

multiclass VPatConvertFP2ISDNode_V_VL<SDNode vop, string instruction_name> {
  foreach fvti = AllFloatVectors in {
    defvar ivti = GetIntVTypeInfo<fvti>.Vti;
    def : Pat<(ivti.Vector (vop (fvti.Vector fvti.RegClass:$rs1),
                                (fvti.Mask true_mask),
                                VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_"#ivti.LMul.MX)
                  fvti.RegClass:$rs1, GPR:$vl, ivti.Log2SEW)>;
  }
}

multiclass VPatConvertI2FPSDNode_V_VL<SDNode vop, string instruction_name> {
  foreach fvti = AllFloatVectors in {
    defvar ivti = GetIntVTypeInfo<fvti>.Vti;
    def : Pat<(fvti.Vector (vop (ivti.Vector ivti.RegClass:$rs1),
                                (ivti.Mask true_mask),
                                VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_"#fvti.LMul.MX)
                  ivti.RegClass:$rs1, GPR:$vl, fvti.Log2SEW)>;
  }
}

multiclass VPatWConvertFP2ISDNode_V_VL<SDNode vop, string instruction_name> {
  foreach fvtiToFWti = AllWidenableFloatVectors in {
    defvar fvti = fvtiToFWti.Vti;
    defvar iwti = GetIntVTypeInfo<fvtiToFWti.Wti>.Vti;
    def : Pat<(iwti.Vector (vop (fvti.Vector fvti.RegClass:$rs1),
                                (fvti.Mask true_mask),
                                VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_"#fvti.LMul.MX)
                  fvti.RegClass:$rs1, GPR:$vl, fvti.Log2SEW)>;
  }
}

multiclass VPatWConvertI2FPSDNode_V_VL<SDNode vop, string instruction_name> {
  foreach vtiToWti = AllWidenableIntToFloatVectors in {
    defvar ivti = vtiToWti.Vti;
    defvar fwti = vtiToWti.Wti;
    def : Pat<(fwti.Vector (vop (ivti.Vector ivti.RegClass:$rs1),
                                (ivti.Mask true_mask),
                                VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_"#ivti.LMul.MX)
                  ivti.RegClass:$rs1, GPR:$vl, ivti.Log2SEW)>;
  }
}

multiclass VPatNConvertFP2ISDNode_V_VL<SDNode vop, string instruction_name> {
  foreach vtiToWti = AllWidenableIntToFloatVectors in {
    defvar vti = vtiToWti.Vti;
    defvar fwti = vtiToWti.Wti;
    def : Pat<(vti.Vector (vop (fwti.Vector fwti.RegClass:$rs1),
                               (fwti.Mask true_mask),
                               VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_"#vti.LMul.MX)
                  fwti.RegClass:$rs1, GPR:$vl, vti.Log2SEW)>;
  }
}

multiclass VPatNConvertI2FPSDNode_V_VL<SDNode vop, string instruction_name> {
  foreach fvtiToFWti = AllWidenableFloatVectors in {
    defvar fvti = fvtiToFWti.Vti;
    defvar iwti = GetIntVTypeInfo<fvtiToFWti.Wti>.Vti;
    def : Pat<(fvti.Vector (vop (iwti.Vector iwti.RegClass:$rs1),
                                (iwti.Mask true_mask),
                                VLOpFrag)),
              (!cast<Instruction>(instruction_name#"_"#fvti.LMul.MX)
                  iwti.RegClass:$rs1, GPR:$vl, fvti.Log2SEW)>;
  }
}

multiclass VPatReductionVL<SDNode vop, string instruction_name, bit is_float> {
  foreach vti = !if(is_float, AllFloatVectors, AllIntegerVectors) in {
    defvar vti_m1 = !cast<VTypeInfo>(!if(is_float, "VF", "VI") # vti.SEW # "M1");
    def: Pat<(vti_m1.Vector (vop (vti.Vector vti.RegClass:$rs1), VR:$rs2,
                                 (vti.Mask true_mask),
                                 VLOpFrag)),
        (!cast<Instruction>(instruction_name#"_VS_"#vti.LMul.MX)
            (vti_m1.Vector (IMPLICIT_DEF)),
            (vti.Vector vti.RegClass:$rs1),
            (vti_m1.Vector VR:$rs2),
            GPR:$vl, vti.Log2SEW)>;
  }
}

//===----------------------------------------------------------------------===//
// Patterns.
//===----------------------------------------------------------------------===//

let Predicates = [HasStdExtV] in {

// 7.4. Vector Unit-Stride Instructions
foreach vti = AllVectors in {
  defvar load_instr = !cast<Instruction>("PseudoVLE"#vti.SEW#"_V_"#vti.LMul.MX);
  defvar store_instr = !cast<Instruction>("PseudoVSE"#vti.SEW#"_V_"#vti.LMul.MX);
  // Load
  def : Pat<(vti.Vector (primate_vle_vl BaseAddr:$rs1, VLOpFrag)),
            (load_instr BaseAddr:$rs1, GPR:$vl, vti.Log2SEW)>;
  // Store
  def : Pat<(primate_vse_vl (vti.Vector vti.RegClass:$rs2), BaseAddr:$rs1,
                          VLOpFrag),
            (store_instr vti.RegClass:$rs2, BaseAddr:$rs1, GPR:$vl, vti.Log2SEW)>;
}

foreach mti = AllMasks in {
  defvar load_instr = !cast<Instruction>("PseudoVLE1_V_"#mti.BX);
  defvar store_instr = !cast<Instruction>("PseudoVSE1_V_"#mti.BX);
  def : Pat<(mti.Mask (primate_vle_vl BaseAddr:$rs1, VLOpFrag)),
            (load_instr BaseAddr:$rs1, GPR:$vl, mti.Log2SEW)>;
  def : Pat<(primate_vse_vl (mti.Mask VR:$rs2), BaseAddr:$rs1,
                          VLOpFrag),
            (store_instr VR:$rs2, BaseAddr:$rs1, GPR:$vl, mti.Log2SEW)>;
}

// 12.1. Vector Single-Width Integer Add and Subtract
defm : VPatBinaryVL_VV_VX_VI<primate_add_vl, "PseudoVADD">;
defm : VPatBinaryVL_VV_VX<primate_sub_vl, "PseudoVSUB">;
// Handle VRSUB specially since it's the only integer binary op with reversed
// pattern operands
foreach vti = AllIntegerVectors in {
  def : Pat<(primate_sub_vl (vti.Vector (SplatPat (XLenVT GPR:$rs2))),
                          (vti.Vector vti.RegClass:$rs1), (vti.Mask true_mask),
                          VLOpFrag),
            (!cast<Instruction>("PseudoVRSUB_VX_"# vti.LMul.MX)
                 vti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(primate_sub_vl (vti.Vector (SplatPat (XLenVT GPR:$rs2))),
                          (vti.Vector vti.RegClass:$rs1), (vti.Mask VMV0:$vm),
                          VLOpFrag),
            (!cast<Instruction>("PseudoVRSUB_VX_"# vti.LMul.MX#"_MASK")
                 (vti.Vector (IMPLICIT_DEF)), vti.RegClass:$rs1, GPR:$rs2,
                 VMV0:$vm, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(primate_sub_vl (vti.Vector (SplatPat_simm5 simm5:$rs2)),
                          (vti.Vector vti.RegClass:$rs1), (vti.Mask true_mask),
                          VLOpFrag),
            (!cast<Instruction>("PseudoVRSUB_VI_"# vti.LMul.MX)
                 vti.RegClass:$rs1, simm5:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(primate_sub_vl (vti.Vector (SplatPat_simm5 simm5:$rs2)),
                          (vti.Vector vti.RegClass:$rs1), (vti.Mask VMV0:$vm),
                          VLOpFrag),
            (!cast<Instruction>("PseudoVRSUB_VI_"# vti.LMul.MX#"_MASK")
                 (vti.Vector (IMPLICIT_DEF)), vti.RegClass:$rs1, simm5:$rs2,
                 VMV0:$vm, GPR:$vl, vti.Log2SEW)>;
}

// 12.3. Vector Integer Extension
defm : VPatExtendSDNode_V_VL<primate_zext_vl, "PseudoVZEXT", "VF2",
                             AllFractionableVF2IntVectors>;
defm : VPatExtendSDNode_V_VL<primate_sext_vl, "PseudoVSEXT", "VF2",
                             AllFractionableVF2IntVectors>;
defm : VPatExtendSDNode_V_VL<primate_zext_vl, "PseudoVZEXT", "VF4",
                             AllFractionableVF4IntVectors>;
defm : VPatExtendSDNode_V_VL<primate_sext_vl, "PseudoVSEXT", "VF4",
                             AllFractionableVF4IntVectors>;
defm : VPatExtendSDNode_V_VL<primate_zext_vl, "PseudoVZEXT", "VF8",
                             AllFractionableVF8IntVectors>;
defm : VPatExtendSDNode_V_VL<primate_sext_vl, "PseudoVSEXT", "VF8",
                                AllFractionableVF8IntVectors>;

// 12.5. Vector Bitwise Logical Instructions
defm : VPatBinaryVL_VV_VX_VI<primate_and_vl, "PseudoVAND">;
defm : VPatBinaryVL_VV_VX_VI<primate_or_vl,  "PseudoVOR">;
defm : VPatBinaryVL_VV_VX_VI<primate_xor_vl, "PseudoVXOR">;

// 12.6. Vector Single-Width Bit Shift Instructions
defm : VPatBinaryVL_VV_VX_VI<primate_shl_vl, "PseudoVSLL", uimm5>;
defm : VPatBinaryVL_VV_VX_VI<primate_srl_vl, "PseudoVSRL", uimm5>;
defm : VPatBinaryVL_VV_VX_VI<primate_sra_vl, "PseudoVSRA", uimm5>;

foreach vti = AllIntegerVectors in {
  // Emit shift by 1 as an add since it might be faster.
  def : Pat<(primate_shl_vl (vti.Vector vti.RegClass:$rs1),
                          (primate_vmv_v_x_vl 1, (XLenVT srcvalue)),
                          (vti.Mask true_mask),
                          VLOpFrag),
            (!cast<Instruction>("PseudoVADD_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs1, vti.RegClass:$rs1, GPR:$vl, vti.Log2SEW)>;
}

// 12.7. Vector Narrowing Integer Right Shift Instructions
foreach vtiTowti = AllWidenableIntVectors in {
  defvar vti = vtiTowti.Vti;
  defvar wti = vtiTowti.Wti;
  def : Pat<(vti.Vector (primate_trunc_vector_vl (wti.Vector wti.RegClass:$rs1),
                                               (vti.Mask true_mask),
                                               VLOpFrag)),
            (!cast<Instruction>("PseudoVNSRL_WI_"#vti.LMul.MX)
                wti.RegClass:$rs1, 0, GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector
             (primate_trunc_vector_vl
              (wti.Vector
               (primate_sra_vl wti.RegClass:$rs1, (SplatPat XLenVT:$rs2),
                             true_mask, VLOpFrag)), true_mask, VLOpFrag)),
            (!cast<Instruction>("PseudoVNSRA_WX_"#vti.LMul.MX)
                wti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector
             (primate_trunc_vector_vl
              (wti.Vector
               (primate_sra_vl wti.RegClass:$rs1, (SplatPat_uimm5 uimm5:$rs2),
                             true_mask, VLOpFrag)), true_mask, VLOpFrag)),
            (!cast<Instruction>("PseudoVNSRA_WI_"#vti.LMul.MX)
                wti.RegClass:$rs1, uimm5:$rs2, GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector
             (primate_trunc_vector_vl
              (wti.Vector
               (primate_srl_vl wti.RegClass:$rs1, (SplatPat XLenVT:$rs2),
                             true_mask, VLOpFrag)), true_mask, VLOpFrag)),
            (!cast<Instruction>("PseudoVNSRL_WX_"#vti.LMul.MX)
                wti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector
             (primate_trunc_vector_vl
              (wti.Vector
               (primate_srl_vl wti.RegClass:$rs1, (SplatPat_uimm5 uimm5:$rs2),
                             true_mask, VLOpFrag)), true_mask, VLOpFrag)),
            (!cast<Instruction>("PseudoVNSRL_WI_"#vti.LMul.MX)
                wti.RegClass:$rs1, uimm5:$rs2, GPR:$vl, vti.Log2SEW)>;
}

// 12.8. Vector Integer Comparison Instructions
foreach vti = AllIntegerVectors in {
  defm : VPatIntegerSetCCVL_VV<vti, "PseudoVMSEQ", SETEQ>;
  defm : VPatIntegerSetCCVL_VV<vti, "PseudoVMSNE", SETNE>;

  defm : VPatIntegerSetCCVL_VV_Swappable<vti, "PseudoVMSLT",  SETLT,  SETGT>;
  defm : VPatIntegerSetCCVL_VV_Swappable<vti, "PseudoVMSLTU", SETULT, SETUGT>;
  defm : VPatIntegerSetCCVL_VV_Swappable<vti, "PseudoVMSLE",  SETLE,  SETGE>;
  defm : VPatIntegerSetCCVL_VV_Swappable<vti, "PseudoVMSLEU", SETULE, SETUGE>;

  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSEQ",  SETEQ,  SETEQ>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSNE",  SETNE,  SETNE>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSLT",  SETLT,  SETGT>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSLTU", SETULT, SETUGT>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSLE",  SETLE,  SETGE>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSLEU", SETULE, SETUGE>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSGT",  SETGT,  SETLT>;
  defm : VPatIntegerSetCCVL_VX_Swappable<vti, "PseudoVMSGTU", SETUGT, SETULT>;
  // There is no VMSGE(U)_VX instruction

  defm : VPatIntegerSetCCVL_VI_Swappable<vti, "PseudoVMSEQ",  SETEQ,  SETEQ>;
  defm : VPatIntegerSetCCVL_VI_Swappable<vti, "PseudoVMSNE",  SETNE,  SETNE>;
  defm : VPatIntegerSetCCVL_VI_Swappable<vti, "PseudoVMSLE",  SETLE,  SETGE>;
  defm : VPatIntegerSetCCVL_VI_Swappable<vti, "PseudoVMSLEU", SETULE, SETUGE>;

  defm : VPatIntegerSetCCVL_VIPlus1<vti, "PseudoVMSLE",  SETLT,
                                    SplatPat_simm5_plus1>;
  defm : VPatIntegerSetCCVL_VIPlus1<vti, "PseudoVMSLEU", SETULT,
                                    SplatPat_simm5_plus1_nonzero>;
  defm : VPatIntegerSetCCVL_VIPlus1<vti, "PseudoVMSGT",  SETGE,
                                    SplatPat_simm5_plus1>;
  defm : VPatIntegerSetCCVL_VIPlus1<vti, "PseudoVMSGTU", SETUGE,
                                    SplatPat_simm5_plus1_nonzero>;
} // foreach vti = AllIntegerVectors

// 12.9. Vector Integer Min/Max Instructions
defm : VPatBinaryVL_VV_VX<primate_umin_vl, "PseudoVMINU">;
defm : VPatBinaryVL_VV_VX<primate_smin_vl, "PseudoVMIN">;
defm : VPatBinaryVL_VV_VX<primate_umax_vl, "PseudoVMAXU">;
defm : VPatBinaryVL_VV_VX<primate_smax_vl, "PseudoVMAX">;

// 12.10. Vector Single-Width Integer Multiply Instructions
defm : VPatBinaryVL_VV_VX<primate_mul_vl, "PseudoVMUL">;
defm : VPatBinaryVL_VV_VX<primate_mulhs_vl, "PseudoVMULH">;
defm : VPatBinaryVL_VV_VX<primate_mulhu_vl, "PseudoVMULHU">;

// 12.11. Vector Integer Divide Instructions
defm : VPatBinaryVL_VV_VX<primate_udiv_vl, "PseudoVDIVU">;
defm : VPatBinaryVL_VV_VX<primate_sdiv_vl, "PseudoVDIV">;
defm : VPatBinaryVL_VV_VX<primate_urem_vl, "PseudoVREMU">;
defm : VPatBinaryVL_VV_VX<primate_srem_vl, "PseudoVREM">;

// 12.12. Vector Widening Integer Multiply Instructions
defm : VPatBinaryWVL_VV_VX<primate_vwmul_vl, "PseudoVWMUL">;
defm : VPatBinaryWVL_VV_VX<primate_vwmulu_vl, "PseudoVWMULU">;

// 12.13 Vector Single-Width Integer Multiply-Add Instructions
foreach vti = AllIntegerVectors in {
  // NOTE: We choose VMADD because it has the most commuting freedom. So it
  // works best with how TwoAddressInstructionPass tries commuting.
  defvar suffix = vti.LMul.MX # "_COMMUTABLE";
  def : Pat<(vti.Vector
             (primate_add_vl vti.RegClass:$rs2,
                           (primate_mul_vl_oneuse vti.RegClass:$rs1,
                                                vti.RegClass:$rd,
                                                (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVMADD_VV_"# suffix)
                 vti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector
             (primate_sub_vl vti.RegClass:$rs2,
                           (primate_mul_vl_oneuse vti.RegClass:$rs1,
                                                vti.RegClass:$rd,
                                                (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVNMSUB_VV_"# suffix)
                 vti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;

  // The choice of VMADD here is arbitrary, vmadd.vx and vmacc.vx are equally
  // commutable.
  def : Pat<(vti.Vector
             (primate_add_vl vti.RegClass:$rs2,
                           (primate_mul_vl_oneuse (SplatPat XLenVT:$rs1),
                                                vti.RegClass:$rd,
                                                (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVMADD_VX_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector
             (primate_sub_vl vti.RegClass:$rs2,
                           (primate_mul_vl_oneuse (SplatPat XLenVT:$rs1),
                                                vti.RegClass:$rd,
                                                (vti.Mask true_mask),
                                                VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVNMSUB_VX_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
}

// 12.14. Vector Widening Integer Multiply-Add Instructions
foreach vtiTowti = AllWidenableIntVectors in {
  defvar vti = vtiTowti.Vti;
  defvar wti = vtiTowti.Wti;
  def : Pat<(wti.Vector
             (primate_add_vl wti.RegClass:$rd,
                           (primate_vwmul_vl_oneuse vti.RegClass:$rs1,
                                                  (vti.Vector vti.RegClass:$rs2),
                                                  (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVWMACC_VV_" # vti.LMul.MX # "_TA")
                 wti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(wti.Vector
             (primate_add_vl wti.RegClass:$rd,
                           (primate_vwmulu_vl_oneuse vti.RegClass:$rs1,
                                                   (vti.Vector vti.RegClass:$rs2),
                                                   (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVWMACCU_VV_" # vti.LMul.MX # "_TA")
                 wti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;

  def : Pat<(wti.Vector
             (primate_add_vl wti.RegClass:$rd,
                           (primate_vwmul_vl_oneuse (SplatPat XLenVT:$rs1),
                                                  (vti.Vector vti.RegClass:$rs2),
                                                  (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVWMACC_VX_" # vti.LMul.MX # "_TA")
                 wti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(wti.Vector
             (primate_add_vl wti.RegClass:$rd,
                           (primate_vwmulu_vl_oneuse (SplatPat XLenVT:$rs1),
                                                   (vti.Vector vti.RegClass:$rs2),
                                                   (vti.Mask true_mask), VLOpFrag),
                           (vti.Mask true_mask), VLOpFrag)),
            (!cast<Instruction>("PseudoVWMACCU_VX_" # vti.LMul.MX # "_TA")
                 wti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
}

// 12.15. Vector Integer Merge Instructions
foreach vti = AllIntegerVectors in {
  def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                          vti.RegClass:$rs1,
                                          vti.RegClass:$rs2,
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVMERGE_VVM_"#vti.LMul.MX)
                 vti.RegClass:$rs2, vti.RegClass:$rs1, VMV0:$vm,
                 GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                          (SplatPat XLenVT:$rs1),
                                          vti.RegClass:$rs2,
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVMERGE_VXM_"#vti.LMul.MX)
                 vti.RegClass:$rs2, GPR:$rs1, VMV0:$vm, GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                          (SplatPat_simm5 simm5:$rs1),
                                          vti.RegClass:$rs2,
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVMERGE_VIM_"#vti.LMul.MX)
                 vti.RegClass:$rs2, simm5:$rs1, VMV0:$vm, GPR:$vl, vti.Log2SEW)>;
}

// 12.16. Vector Integer Move Instructions
foreach vti = AllIntegerVectors in {
  def : Pat<(vti.Vector (primate_vmv_v_x_vl GPR:$rs2, VLOpFrag)),
            (!cast<Instruction>("PseudoVMV_V_X_"#vti.LMul.MX)
             $rs2, GPR:$vl, vti.Log2SEW)>;
  defvar ImmPat = !cast<ComplexPattern>("sew"#vti.SEW#"simm5");
  def : Pat<(vti.Vector (primate_vmv_v_x_vl (ImmPat XLenVT:$imm5),
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVMV_V_I_"#vti.LMul.MX)
             XLenVT:$imm5, GPR:$vl, vti.Log2SEW)>;
}

// 12.1. Vector Single-Width Saturating Add and Subtract
defm : VPatBinaryVL_VV_VX_VI<primate_saddsat_vl, "PseudoVSADD">;
defm : VPatBinaryVL_VV_VX_VI<primate_uaddsat_vl, "PseudoVSADDU">;
defm : VPatBinaryVL_VV_VX<primate_ssubsat_vl, "PseudoVSSUB">;
defm : VPatBinaryVL_VV_VX<primate_usubsat_vl, "PseudoVSSUBU">;

} // Predicates = [HasStdExtV]

// 15.1. Vector Single-Width Integer Reduction Instructions
let Predicates = [HasStdExtV] in {
defm : VPatReductionVL<rvv_vecreduce_ADD_vl,  "PseudoVREDSUM", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_UMAX_vl, "PseudoVREDMAXU", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_SMAX_vl, "PseudoVREDMAX", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_UMIN_vl, "PseudoVREDMINU", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_SMIN_vl, "PseudoVREDMIN", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_AND_vl,  "PseudoVREDAND", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_OR_vl,   "PseudoVREDOR", /*is_float*/0>;
defm : VPatReductionVL<rvv_vecreduce_XOR_vl,  "PseudoVREDXOR", /*is_float*/0>;
} // Predicates = [HasStdExtV]

// 15.3. Vector Single-Width Floating-Point Reduction Instructions
let Predicates = [HasStdExtV, HasStdExtF] in {
defm : VPatReductionVL<rvv_vecreduce_SEQ_FADD_vl, "PseudoVFREDOSUM", /*is_float*/1>;
defm : VPatReductionVL<rvv_vecreduce_FADD_vl,     "PseudoVFREDSUM", /*is_float*/1>;
defm : VPatReductionVL<rvv_vecreduce_FMIN_vl,     "PseudoVFREDMIN", /*is_float*/1>;
defm : VPatReductionVL<rvv_vecreduce_FMAX_vl,     "PseudoVFREDMAX", /*is_float*/1>;
} // Predicates = [HasStdExtV, HasStdExtF]

let Predicates = [HasStdExtV, HasStdExtF] in {

// 14.2. Vector Single-Width Floating-Point Add/Subtract Instructions
defm : VPatBinaryFPVL_VV_VF<primate_fadd_vl, "PseudoVFADD">;
defm : VPatBinaryFPVL_VV_VF<primate_fsub_vl, "PseudoVFSUB">;
defm : VPatBinaryFPVL_R_VF<primate_fsub_vl, "PseudoVFRSUB">;

// 14.4. Vector Single-Width Floating-Point Multiply/Divide Instructions
defm : VPatBinaryFPVL_VV_VF<primate_fmul_vl, "PseudoVFMUL">;
defm : VPatBinaryFPVL_VV_VF<primate_fdiv_vl, "PseudoVFDIV">;
defm : VPatBinaryFPVL_R_VF<primate_fdiv_vl, "PseudoVFRDIV">;

// 14.6 Vector Single-Width Floating-Point Fused Multiply-Add Instructions.
foreach vti = AllFloatVectors in {
  // NOTE: We choose VFMADD because it has the most commuting freedom. So it
  // works best with how TwoAddressInstructionPass tries commuting.
  defvar suffix = vti.LMul.MX # "_COMMUTABLE";
  def : Pat<(vti.Vector (primate_fma_vl vti.RegClass:$rs1, vti.RegClass:$rd,
                                      vti.RegClass:$rs2, (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVFMADD_VV_"# suffix)
                 vti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl vti.RegClass:$rs1, vti.RegClass:$rd,
                                      (primate_fneg_vl vti.RegClass:$rs2,
                                                     (vti.Mask true_mask),
                                                     VLOpFrag),
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVFMSUB_VV_"# suffix)
                 vti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl (primate_fneg_vl vti.RegClass:$rs1,
                                                     (vti.Mask true_mask),
                                                     VLOpFrag),
                                      vti.RegClass:$rd,
                                      (primate_fneg_vl vti.RegClass:$rs2,
                                                     (vti.Mask true_mask),
                                                     VLOpFrag),
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVFNMADD_VV_"# suffix)
                 vti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl (primate_fneg_vl vti.RegClass:$rs1,
                                                     (vti.Mask true_mask),
                                                     VLOpFrag),
                                      vti.RegClass:$rd, vti.RegClass:$rs2,
                                      (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVFNMSUB_VV_"# suffix)
                 vti.RegClass:$rd, vti.RegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;

  // The choice of VFMADD here is arbitrary, vfmadd.vf and vfmacc.vf are equally
  // commutable.
  def : Pat<(vti.Vector (primate_fma_vl (SplatFPOp vti.ScalarRegClass:$rs1),
                                       vti.RegClass:$rd, vti.RegClass:$rs2,
                                       (vti.Mask true_mask),
                                       VLOpFrag)),
            (!cast<Instruction>("PseudoVFMADD_V" # vti.ScalarSuffix # "_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl (SplatFPOp vti.ScalarRegClass:$rs1),
                                       vti.RegClass:$rd,
                                       (primate_fneg_vl vti.RegClass:$rs2,
                                                      (vti.Mask true_mask),
                                                      VLOpFrag),
                                       (vti.Mask true_mask),
                                       VLOpFrag)),
            (!cast<Instruction>("PseudoVFMSUB_V" # vti.ScalarSuffix # "_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl (SplatFPOp vti.ScalarRegClass:$rs1),
                                       (primate_fneg_vl vti.RegClass:$rd,
                                                      (vti.Mask true_mask),
                                                      VLOpFrag),
                                       (primate_fneg_vl vti.RegClass:$rs2,
                                                      (vti.Mask true_mask),
                                                      VLOpFrag),
                                       (vti.Mask true_mask),
                                       VLOpFrag)),
            (!cast<Instruction>("PseudoVFNMADD_V" # vti.ScalarSuffix # "_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl (SplatFPOp vti.ScalarRegClass:$rs1),
                                       (primate_fneg_vl vti.RegClass:$rd,
                                                      (vti.Mask true_mask),
                                                      VLOpFrag),
                                       vti.RegClass:$rs2,
                                       (vti.Mask true_mask),
                                       VLOpFrag)),
            (!cast<Instruction>("PseudoVFNMSUB_V" # vti.ScalarSuffix # "_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;

  // The splat might be negated.
  def : Pat<(vti.Vector (primate_fma_vl (primate_fneg_vl (SplatFPOp vti.ScalarRegClass:$rs1),
                                                     (vti.Mask true_mask),
                                                     VLOpFrag),
                                       vti.RegClass:$rd,
                                       (primate_fneg_vl vti.RegClass:$rs2,
                                                      (vti.Mask true_mask),
                                                      VLOpFrag),
                                       (vti.Mask true_mask),
                                       VLOpFrag)),
            (!cast<Instruction>("PseudoVFNMADD_V" # vti.ScalarSuffix # "_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_fma_vl (primate_fneg_vl (SplatFPOp vti.ScalarRegClass:$rs1),
                                                     (vti.Mask true_mask),
                                                     VLOpFrag),
                                       vti.RegClass:$rd, vti.RegClass:$rs2,
                                       (vti.Mask true_mask),
                                       VLOpFrag)),
            (!cast<Instruction>("PseudoVFNMSUB_V" # vti.ScalarSuffix # "_" # suffix)
                 vti.RegClass:$rd, vti.ScalarRegClass:$rs1, vti.RegClass:$rs2,
                 GPR:$vl, vti.Log2SEW)>;
}

// 14.11. Vector Floating-Point MIN/MAX Instructions
defm : VPatBinaryFPVL_VV_VF<primate_fminnum_vl, "PseudoVFMIN">;
defm : VPatBinaryFPVL_VV_VF<primate_fmaxnum_vl, "PseudoVFMAX">;

// 14.13. Vector Floating-Point Compare Instructions
defm : VPatFPSetCCVL_VV_VF_FV<SETEQ,  "PseudoVMFEQ", "PseudoVMFEQ">;
defm : VPatFPSetCCVL_VV_VF_FV<SETOEQ, "PseudoVMFEQ", "PseudoVMFEQ">;

defm : VPatFPSetCCVL_VV_VF_FV<SETNE,  "PseudoVMFNE", "PseudoVMFNE">;
defm : VPatFPSetCCVL_VV_VF_FV<SETUNE, "PseudoVMFNE", "PseudoVMFNE">;

defm : VPatFPSetCCVL_VV_VF_FV<SETLT,  "PseudoVMFLT", "PseudoVMFGT">;
defm : VPatFPSetCCVL_VV_VF_FV<SETOLT, "PseudoVMFLT", "PseudoVMFGT">;

defm : VPatFPSetCCVL_VV_VF_FV<SETLE,  "PseudoVMFLE", "PseudoVMFGE">;
defm : VPatFPSetCCVL_VV_VF_FV<SETOLE, "PseudoVMFLE", "PseudoVMFGE">;

foreach vti = AllFloatVectors in {
  // 14.8. Vector Floating-Point Square-Root Instruction
  def : Pat<(primate_fsqrt_vl (vti.Vector vti.RegClass:$rs2), (vti.Mask true_mask),
                            VLOpFrag),
            (!cast<Instruction>("PseudoVFSQRT_V_"# vti.LMul.MX)
                 vti.RegClass:$rs2, GPR:$vl, vti.Log2SEW)>;

  // 14.12. Vector Floating-Point Sign-Injection Instructions
  def : Pat<(primate_fabs_vl (vti.Vector vti.RegClass:$rs), (vti.Mask true_mask),
                           VLOpFrag),
            (!cast<Instruction>("PseudoVFSGNJX_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs, vti.RegClass:$rs, GPR:$vl, vti.Log2SEW)>;
  // Handle fneg with VFSGNJN using the same input for both operands.
  def : Pat<(primate_fneg_vl (vti.Vector vti.RegClass:$rs), (vti.Mask true_mask),
                           VLOpFrag),
            (!cast<Instruction>("PseudoVFSGNJN_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs, vti.RegClass:$rs, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(primate_fcopysign_vl (vti.Vector vti.RegClass:$rs1),
                                (vti.Vector vti.RegClass:$rs2),
                                (vti.Mask true_mask),
                                VLOpFrag),
            (!cast<Instruction>("PseudoVFSGNJ_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs1, vti.RegClass:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(primate_fcopysign_vl (vti.Vector vti.RegClass:$rs1),
                                (primate_fneg_vl vti.RegClass:$rs2,
                                               (vti.Mask true_mask),
                                               VLOpFrag),
                                (vti.Mask true_mask),
                                VLOpFrag),
            (!cast<Instruction>("PseudoVFSGNJN_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs1, vti.RegClass:$rs2, GPR:$vl, vti.Log2SEW)>;

  def : Pat<(primate_fcopysign_vl (vti.Vector vti.RegClass:$rs1),
                                (SplatFPOp vti.ScalarRegClass:$rs2),
                                (vti.Mask true_mask),
                                VLOpFrag),
            (!cast<Instruction>("PseudoVFSGNJ_V"#vti.ScalarSuffix#"_"# vti.LMul.MX)
                 vti.RegClass:$rs1, vti.ScalarRegClass:$rs2, GPR:$vl, vti.Log2SEW)>;
}

foreach fvti = AllFloatVectors in {
  // Floating-point vselects:
  // 12.15. Vector Integer Merge Instructions
  // 14.15. Vector Floating-Point Merge Instruction
  def : Pat<(fvti.Vector (primate_vselect_vl (fvti.Mask VMV0:$vm),
                                           fvti.RegClass:$rs1,
                                           fvti.RegClass:$rs2,
                                           VLOpFrag)),
            (!cast<Instruction>("PseudoVMERGE_VVM_"#fvti.LMul.MX)
                 fvti.RegClass:$rs2, fvti.RegClass:$rs1, VMV0:$vm,
                 GPR:$vl, fvti.Log2SEW)>;

  def : Pat<(fvti.Vector (primate_vselect_vl (fvti.Mask VMV0:$vm),
                                           (SplatFPOp fvti.ScalarRegClass:$rs1),
                                           fvti.RegClass:$rs2,
                                           VLOpFrag)),
            (!cast<Instruction>("PseudoVFMERGE_V"#fvti.ScalarSuffix#"M_"#fvti.LMul.MX)
                 fvti.RegClass:$rs2,
                 (fvti.Scalar fvti.ScalarRegClass:$rs1),
                 VMV0:$vm, GPR:$vl, fvti.Log2SEW)>;

  def : Pat<(fvti.Vector (primate_vselect_vl (fvti.Mask VMV0:$vm),
                                           (SplatFPOp (fvti.Scalar fpimm0)),
                                           fvti.RegClass:$rs2,
                                           VLOpFrag)),
            (!cast<Instruction>("PseudoVMERGE_VIM_"#fvti.LMul.MX)
                 fvti.RegClass:$rs2, 0, VMV0:$vm, GPR:$vl, fvti.Log2SEW)>;

  // 14.16. Vector Floating-Point Move Instruction
  // If we're splatting fpimm0, use vmv.v.x vd, x0.
  def : Pat<(fvti.Vector (primate_vfmv_v_f_vl
                         (fvti.Scalar (fpimm0)), VLOpFrag)),
            (!cast<Instruction>("PseudoVMV_V_I_"#fvti.LMul.MX)
             0, GPR:$vl, fvti.Log2SEW)>;

  def : Pat<(fvti.Vector (primate_vfmv_v_f_vl
                         (fvti.Scalar fvti.ScalarRegClass:$rs2), VLOpFrag)),
            (!cast<Instruction>("PseudoVFMV_V_" # fvti.ScalarSuffix # "_" #
                                fvti.LMul.MX)
             (fvti.Scalar fvti.ScalarRegClass:$rs2),
             GPR:$vl, fvti.Log2SEW)>;

  // 14.17. Vector Single-Width Floating-Point/Integer Type-Convert Instructions
  defm : VPatConvertFP2ISDNode_V_VL<primate_fp_to_sint_vl, "PseudoVFCVT_RTZ_X_F_V">;
  defm : VPatConvertFP2ISDNode_V_VL<primate_fp_to_uint_vl, "PseudoVFCVT_RTZ_XU_F_V">;
  defm : VPatConvertI2FPSDNode_V_VL<primate_sint_to_fp_vl, "PseudoVFCVT_F_X_V">;
  defm : VPatConvertI2FPSDNode_V_VL<primate_uint_to_fp_vl, "PseudoVFCVT_F_XU_V">;

  // 14.18. Widening Floating-Point/Integer Type-Convert Instructions
  defm : VPatWConvertFP2ISDNode_V_VL<primate_fp_to_sint_vl, "PseudoVFWCVT_RTZ_X_F_V">;
  defm : VPatWConvertFP2ISDNode_V_VL<primate_fp_to_uint_vl, "PseudoVFWCVT_RTZ_XU_F_V">;
  defm : VPatWConvertI2FPSDNode_V_VL<primate_sint_to_fp_vl, "PseudoVFWCVT_F_X_V">;
  defm : VPatWConvertI2FPSDNode_V_VL<primate_uint_to_fp_vl, "PseudoVFWCVT_F_XU_V">;
  foreach fvtiToFWti = AllWidenableFloatVectors in {
    defvar fvti = fvtiToFWti.Vti;
    defvar fwti = fvtiToFWti.Wti;
    def : Pat<(fwti.Vector (primate_fpextend_vl (fvti.Vector fvti.RegClass:$rs1),
                                              (fvti.Mask true_mask),
                                              VLOpFrag)),
              (!cast<Instruction>("PseudoVFWCVT_F_F_V_"#fvti.LMul.MX)
                  fvti.RegClass:$rs1, GPR:$vl, fvti.Log2SEW)>;
  }

  // 14.19 Narrowing Floating-Point/Integer Type-Convert Instructions
  defm : VPatNConvertFP2ISDNode_V_VL<primate_fp_to_sint_vl, "PseudoVFNCVT_RTZ_X_F_W">;
  defm : VPatNConvertFP2ISDNode_V_VL<primate_fp_to_uint_vl, "PseudoVFNCVT_RTZ_XU_F_W">;
  defm : VPatNConvertI2FPSDNode_V_VL<primate_sint_to_fp_vl, "PseudoVFNCVT_F_X_W">;
  defm : VPatNConvertI2FPSDNode_V_VL<primate_uint_to_fp_vl, "PseudoVFNCVT_F_XU_W">;
  foreach fvtiToFWti = AllWidenableFloatVectors in {
    defvar fvti = fvtiToFWti.Vti;
    defvar fwti = fvtiToFWti.Wti;
    def : Pat<(fvti.Vector (primate_fpround_vl (fwti.Vector fwti.RegClass:$rs1),
                                             (fwti.Mask true_mask),
                                             VLOpFrag)),
              (!cast<Instruction>("PseudoVFNCVT_F_F_W_"#fvti.LMul.MX)
                  fwti.RegClass:$rs1, GPR:$vl, fvti.Log2SEW)>;

    def : Pat<(fvti.Vector (primate_fncvt_rod_vl (fwti.Vector fwti.RegClass:$rs1),
                                               (fwti.Mask true_mask),
                                               VLOpFrag)),
              (!cast<Instruction>("PseudoVFNCVT_ROD_F_F_W_"#fvti.LMul.MX)
                  fwti.RegClass:$rs1, GPR:$vl, fvti.Log2SEW)>;
  }
}

} // Predicates = [HasStdExtV, HasStdExtF]

let Predicates = [HasStdExtV] in {

foreach mti = AllMasks in {
  // 16.1 Vector Mask-Register Logical Instructions
  def : Pat<(mti.Mask (primate_vmset_vl VLOpFrag)),
            (!cast<Instruction>("PseudoVMSET_M_" # mti.BX) GPR:$vl, mti.Log2SEW)>;
  def : Pat<(mti.Mask (primate_vmclr_vl VLOpFrag)),
            (!cast<Instruction>("PseudoVMCLR_M_" # mti.BX) GPR:$vl, mti.Log2SEW)>;

  def : Pat<(mti.Mask (primate_vmand_vl VR:$rs1, VR:$rs2, VLOpFrag)),
            (!cast<Instruction>("PseudoVMAND_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;
  def : Pat<(mti.Mask (primate_vmor_vl VR:$rs1, VR:$rs2, VLOpFrag)),
            (!cast<Instruction>("PseudoVMOR_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;
  def : Pat<(mti.Mask (primate_vmxor_vl VR:$rs1, VR:$rs2, VLOpFrag)),
            (!cast<Instruction>("PseudoVMXOR_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;

  def : Pat<(mti.Mask (primate_vmand_vl VR:$rs1,
                                      (primate_vmnot_vl VR:$rs2, VLOpFrag),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVMANDNOT_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;
  def : Pat<(mti.Mask (primate_vmor_vl VR:$rs1,
                                     (primate_vmnot_vl VR:$rs2, VLOpFrag),
                                     VLOpFrag)),
            (!cast<Instruction>("PseudoVMORNOT_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;
  // XOR is associative so we need 2 patterns for VMXNOR.
  def : Pat<(mti.Mask (primate_vmxor_vl (primate_vmnot_vl VR:$rs1,
                                                      VLOpFrag),
                                     VR:$rs2, VLOpFrag)),
            (!cast<Instruction>("PseudoVMXNOR_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;

  def : Pat<(mti.Mask (primate_vmnot_vl (primate_vmand_vl VR:$rs1, VR:$rs2,
                                                      VLOpFrag),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVMNAND_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;
  def : Pat<(mti.Mask (primate_vmnot_vl (primate_vmor_vl VR:$rs1, VR:$rs2,
                                                     VLOpFrag),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVMNOR_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;
  def : Pat<(mti.Mask (primate_vmnot_vl (primate_vmxor_vl VR:$rs1, VR:$rs2,
                                                      VLOpFrag),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVMXNOR_MM_" # mti.LMul.MX)
                 VR:$rs1, VR:$rs2, GPR:$vl, mti.Log2SEW)>;

  // Match the not idiom to the vmnot.m pseudo.
  def : Pat<(mti.Mask (primate_vmnot_vl VR:$rs, VLOpFrag)),
            (!cast<Instruction>("PseudoVMNAND_MM_" # mti.LMul.MX)
                 VR:$rs, VR:$rs, GPR:$vl, mti.Log2SEW)>;

  // 16.2 Vector Mask Population Count vpopc
  def : Pat<(XLenVT (primate_vpopc_vl (mti.Mask VR:$rs2), (mti.Mask true_mask),
                                    VLOpFrag)),
            (!cast<Instruction>("PseudoVPOPC_M_" # mti.BX)
                 VR:$rs2, GPR:$vl, mti.Log2SEW)>;
}

} // Predicates = [HasStdExtV]

let Predicates = [HasStdExtV] in {
// 17.1. Integer Scalar Move Instructions
// 17.4. Vector Register Gather Instruction
foreach vti = AllIntegerVectors in {
  def : Pat<(vti.Vector (primate_vmv_s_x_vl (vti.Vector vti.RegClass:$merge),
                                          vti.ScalarRegClass:$rs1,
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVMV_S_X_"#vti.LMul.MX)
                vti.RegClass:$merge,
                (vti.Scalar vti.ScalarRegClass:$rs1), GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_vrgather_vv_vl vti.RegClass:$rs2,
                                              (vti.Vector vti.RegClass:$rs1),
                                              (vti.Mask true_mask),
                                              VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs2, vti.RegClass:$rs1, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_vrgather_vx_vl vti.RegClass:$rs2, GPR:$rs1,
                                              (vti.Mask true_mask),
                                              VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VX_"# vti.LMul.MX)
                 vti.RegClass:$rs2, GPR:$rs1, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_vrgather_vx_vl vti.RegClass:$rs2, uimm5:$imm,
                                              (vti.Mask true_mask),
                                              VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VI_"# vti.LMul.MX)
                 vti.RegClass:$rs2, uimm5:$imm, GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                          (primate_vrgather_vv_vl
                                            vti.RegClass:$rs2,
                                            vti.RegClass:$rs1,
                                            (vti.Mask true_mask),
                                            VLOpFrag),
                                          vti.RegClass:$merge,
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VV_"# vti.LMul.MX#"_MASK")
                 vti.RegClass:$merge, vti.RegClass:$rs2, vti.RegClass:$rs1,
                 vti.Mask:$vm, GPR:$vl, vti.Log2SEW)>;

  // emul = lmul * 16 / sew
  defvar vlmul = vti.LMul;
  defvar octuple_lmul = vlmul.octuple;
  defvar octuple_emul = !srl(!mul(octuple_lmul, 16), vti.Log2SEW);
  if !and(!ge(octuple_emul, 1), !le(octuple_emul, 64)) then {
    defvar emul_str = octuple_to_str<octuple_emul>.ret;
    defvar ivti = !cast<VTypeInfo>("VI16" # emul_str);
    defvar inst = "PseudoVRGATHEREI16_VV_" # vti.LMul.MX # "_" # emul_str;
    def : Pat<(vti.Vector (primate_vrgatherei16_vv_vl vti.RegClass:$rs2,
                                                    (ivti.Vector ivti.RegClass:$rs1),
                                                    (vti.Mask true_mask),
                                                    VLOpFrag)),
              (!cast<Instruction>(inst)
                   vti.RegClass:$rs2, ivti.RegClass:$rs1, GPR:$vl, vti.Log2SEW)>;

    def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                            (primate_vrgatherei16_vv_vl
                                              vti.RegClass:$rs2,
                                              (ivti.Vector ivti.RegClass:$rs1),
                                              (vti.Mask true_mask),
                                              VLOpFrag),
                                            vti.RegClass:$merge,
                                            VLOpFrag)),
              (!cast<Instruction>(inst#"_MASK")
                   vti.RegClass:$merge, vti.RegClass:$rs2, ivti.RegClass:$rs1,
                   vti.Mask:$vm, GPR:$vl, vti.Log2SEW)>;
  }
}

} // Predicates = [HasStdExtV]

let Predicates = [HasStdExtV, HasStdExtF] in {

// 17.2. Floating-Point Scalar Move Instructions
foreach vti = AllFloatVectors in {
  def : Pat<(vti.Vector (primate_vfmv_s_f_vl (vti.Vector vti.RegClass:$merge),
                                           vti.ScalarRegClass:$rs1,
                                           VLOpFrag)),
            (!cast<Instruction>("PseudoVFMV_S_"#vti.ScalarSuffix#"_"#vti.LMul.MX)
                vti.RegClass:$merge,
                (vti.Scalar vti.ScalarRegClass:$rs1), GPR:$vl, vti.Log2SEW)>;
  defvar ivti = GetIntVTypeInfo<vti>.Vti;
  def : Pat<(vti.Vector (primate_vrgather_vv_vl vti.RegClass:$rs2,
                                              (ivti.Vector vti.RegClass:$rs1),
                                              (vti.Mask true_mask),
                                              VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VV_"# vti.LMul.MX)
                 vti.RegClass:$rs2, vti.RegClass:$rs1, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_vrgather_vx_vl vti.RegClass:$rs2, GPR:$rs1,
                                              (vti.Mask true_mask),
                                              VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VX_"# vti.LMul.MX)
                 vti.RegClass:$rs2, GPR:$rs1, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_vrgather_vx_vl vti.RegClass:$rs2, uimm5:$imm,
                                              (vti.Mask true_mask),
                                              VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VI_"# vti.LMul.MX)
                 vti.RegClass:$rs2, uimm5:$imm, GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                          (primate_vrgather_vv_vl
                                            vti.RegClass:$rs2,
                                            (ivti.Vector vti.RegClass:$rs1),
                                            (vti.Mask true_mask),
                                            VLOpFrag),
                                          vti.RegClass:$merge,
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVRGATHER_VV_"# vti.LMul.MX#"_MASK")
                 vti.RegClass:$merge, vti.RegClass:$rs2, vti.RegClass:$rs1,
                 vti.Mask:$vm, GPR:$vl, vti.Log2SEW)>;

  defvar vlmul = vti.LMul;
  defvar octuple_lmul = vlmul.octuple;
  defvar octuple_emul = !srl(!mul(octuple_lmul, 16), vti.Log2SEW);
  if !and(!ge(octuple_emul, 1), !le(octuple_emul, 64)) then {
    defvar emul_str = octuple_to_str<octuple_emul>.ret;
    defvar ivti = !cast<VTypeInfo>("VI16" # emul_str);
    defvar inst = "PseudoVRGATHEREI16_VV_" # vti.LMul.MX # "_" # emul_str;
    def : Pat<(vti.Vector (primate_vrgatherei16_vv_vl vti.RegClass:$rs2,
                                                    (ivti.Vector ivti.RegClass:$rs1),
                                                    (vti.Mask true_mask),
                                                    VLOpFrag)),
              (!cast<Instruction>(inst)
                   vti.RegClass:$rs2, ivti.RegClass:$rs1, GPR:$vl, vti.Log2SEW)>;

    def : Pat<(vti.Vector (primate_vselect_vl (vti.Mask VMV0:$vm),
                                            (primate_vrgatherei16_vv_vl
                                              vti.RegClass:$rs2,
                                              (ivti.Vector ivti.RegClass:$rs1),
                                              (vti.Mask true_mask),
                                              VLOpFrag),
                                            vti.RegClass:$merge,
                                            VLOpFrag)),
              (!cast<Instruction>(inst#"_MASK")
                   vti.RegClass:$merge, vti.RegClass:$rs2, ivti.RegClass:$rs1,
                   vti.Mask:$vm, GPR:$vl, vti.Log2SEW)>;
  }
}

} // Predicates = [HasStdExtV, HasStdExtF]

//===----------------------------------------------------------------------===//
// Miscellaneous PrimateISD SDNodes
//===----------------------------------------------------------------------===//

def primate_vid_vl : SDNode<"PrimateISD::VID_VL", SDTypeProfile<1, 2,
                          [SDTCisVec<0>, SDTCVecEltisVT<1, i1>,
                           SDTCisSameNumEltsAs<0, 1>, SDTCisVT<2, XLenVT>]>, []>;

def SDTPRVSlide : SDTypeProfile<1, 5, [
  SDTCisVec<0>, SDTCisSameAs<1, 0>, SDTCisSameAs<2, 0>, SDTCisVT<3, XLenVT>,
  SDTCVecEltisVT<4, i1>, SDTCisSameNumEltsAs<0, 4>, SDTCisVT<5, XLenVT>
]>;
def SDTPRVSlide1 : SDTypeProfile<1, 4, [
  SDTCisVec<0>, SDTCisSameAs<1, 0>, SDTCisInt<0>, SDTCisVT<2, XLenVT>,
  SDTCVecEltisVT<3, i1>, SDTCisSameNumEltsAs<0, 3>, SDTCisVT<4, XLenVT>
]>;

def primate_slideup_vl   : SDNode<"PrimateISD::VSLIDEUP_VL", SDTPRVSlide, []>;
def primate_slide1up_vl  : SDNode<"PrimateISD::VSLIDE1UP_VL", SDTPRVSlide1, []>;
def primate_slidedown_vl : SDNode<"PrimateISD::VSLIDEDOWN_VL", SDTPRVSlide, []>;
def primate_slide1down_vl  : SDNode<"PrimateISD::VSLIDE1DOWN_VL", SDTPRVSlide1, []>;

let Predicates = [HasStdExtV] in {

foreach vti = AllIntegerVectors in {
  def : Pat<(vti.Vector (primate_vid_vl (vti.Mask true_mask),
                                      VLOpFrag)),
            (!cast<Instruction>("PseudoVID_V_"#vti.LMul.MX) GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_slide1up_vl (vti.Vector vti.RegClass:$rs1),
                                           GPR:$rs2, (vti.Mask true_mask),
                                           VLOpFrag)),
            (!cast<Instruction>("PseudoVSLIDE1UP_VX_"#vti.LMul.MX)
                vti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
  def : Pat<(vti.Vector (primate_slide1down_vl (vti.Vector vti.RegClass:$rs1),
                                           GPR:$rs2, (vti.Mask true_mask),
                                           VLOpFrag)),
            (!cast<Instruction>("PseudoVSLIDE1DOWN_VX_"#vti.LMul.MX)
                vti.RegClass:$rs1, GPR:$rs2, GPR:$vl, vti.Log2SEW)>;
}

foreach vti = !listconcat(AllIntegerVectors, AllFloatVectors) in {
  def : Pat<(vti.Vector (primate_slideup_vl (vti.Vector vti.RegClass:$rs3),
                                          (vti.Vector vti.RegClass:$rs1),
                                          uimm5:$rs2, (vti.Mask true_mask),
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVSLIDEUP_VI_"#vti.LMul.MX)
                vti.RegClass:$rs3, vti.RegClass:$rs1, uimm5:$rs2,
                GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_slideup_vl (vti.Vector vti.RegClass:$rs3),
                                          (vti.Vector vti.RegClass:$rs1),
                                          GPR:$rs2, (vti.Mask true_mask),
                                          VLOpFrag)),
            (!cast<Instruction>("PseudoVSLIDEUP_VX_"#vti.LMul.MX)
                vti.RegClass:$rs3, vti.RegClass:$rs1, GPR:$rs2,
                GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_slidedown_vl (vti.Vector vti.RegClass:$rs3),
                                            (vti.Vector vti.RegClass:$rs1),
                                            uimm5:$rs2, (vti.Mask true_mask),
                                            VLOpFrag)),
            (!cast<Instruction>("PseudoVSLIDEDOWN_VI_"#vti.LMul.MX)
                vti.RegClass:$rs3, vti.RegClass:$rs1, uimm5:$rs2,
                GPR:$vl, vti.Log2SEW)>;

  def : Pat<(vti.Vector (primate_slidedown_vl (vti.Vector vti.RegClass:$rs3),
                                            (vti.Vector vti.RegClass:$rs1),
                                            GPR:$rs2, (vti.Mask true_mask),
                                            VLOpFrag)),
            (!cast<Instruction>("PseudoVSLIDEDOWN_VX_"#vti.LMul.MX)
                vti.RegClass:$rs3, vti.RegClass:$rs1, GPR:$rs2,
                GPR:$vl, vti.Log2SEW)>;
}

} // Predicates = [HasStdExtV]
