//===- IntrinsicsPrimate.td - Defines Primate intrinsics ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Primate-specific intrinsics.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Atomics

// Atomic Intrinsics have multiple versions for different access widths, which
// all follow one of the following signatures (depending on how many arguments
// they require). We carefully instantiate only specific versions of these for
// specific integer widths, rather than using `llvm_anyint_ty`.
//
// In fact, as these intrinsics take `llvm_anyptr_ty`, the given names are the
// canonical names, and the intrinsics used in the code will have a name
// suffixed with the pointer type they are specialised for (denoted `<p>` in the
// names below), in order to avoid type conflicts.

let TargetPrefix = "primate" in {

  // T @llvm.<name>.T.<p>(any*, T, T, T imm);
  class PrimateMaskedAtomicRMWFourArg<LLVMType itype>
      : Intrinsic<[itype], [llvm_anyptr_ty, itype, itype, itype],
                  [IntrArgMemOnly, NoCapture<ArgIndex<0>>, ImmArg<ArgIndex<3>>]>;
  // T @llvm.<name>.T.<p>(any*, T, T, T, T imm);
  class PrimateMaskedAtomicRMWFiveArg<LLVMType itype>
      : Intrinsic<[itype], [llvm_anyptr_ty, itype, itype, itype, itype],
                  [IntrArgMemOnly, NoCapture<ArgIndex<0>>, ImmArg<ArgIndex<4>>]>;

  // We define 32-bit and 64-bit variants of the above, where T stands for i32
  // or i64 respectively:
  multiclass PrimateMaskedAtomicRMWFourArgIntrinsics {
    // i32 @llvm.<name>.i32.<p>(any*, i32, i32, i32 imm);
    def _i32 : PrimateMaskedAtomicRMWFourArg<llvm_i32_ty>;
    // i64 @llvm.<name>.i32.<p>(any*, i64, i64, i64 imm);
    def _i64 : PrimateMaskedAtomicRMWFourArg<llvm_i64_ty>;
  }

  multiclass PrimateMaskedAtomicRMWFiveArgIntrinsics {
    // i32 @llvm.<name>.i32.<p>(any*, i32, i32, i32, i32 imm);
    def _i32 : PrimateMaskedAtomicRMWFiveArg<llvm_i32_ty>;
    // i64 @llvm.<name>.i64.<p>(any*, i64, i64, i64, i64 imm);
    def _i64 : PrimateMaskedAtomicRMWFiveArg<llvm_i64_ty>;
  }

  // @llvm.primate.masked.atomicrmw.*.{i32,i64}.<p>(...)
  defm int_primate_masked_atomicrmw_xchg : PrimateMaskedAtomicRMWFourArgIntrinsics;
  defm int_primate_masked_atomicrmw_add : PrimateMaskedAtomicRMWFourArgIntrinsics;
  defm int_primate_masked_atomicrmw_sub : PrimateMaskedAtomicRMWFourArgIntrinsics;
  defm int_primate_masked_atomicrmw_nand : PrimateMaskedAtomicRMWFourArgIntrinsics;
  // Signed min and max need an extra operand to do sign extension with.
  defm int_primate_masked_atomicrmw_max : PrimateMaskedAtomicRMWFiveArgIntrinsics;
  defm int_primate_masked_atomicrmw_min : PrimateMaskedAtomicRMWFiveArgIntrinsics;
  // Unsigned min and max don't need the extra operand.
  defm int_primate_masked_atomicrmw_umax : PrimateMaskedAtomicRMWFourArgIntrinsics;
  defm int_primate_masked_atomicrmw_umin : PrimateMaskedAtomicRMWFourArgIntrinsics;

  // @llvm.primate.masked.cmpxchg.{i32,i64}.<p>(...)
  defm int_primate_masked_cmpxchg : PrimateMaskedAtomicRMWFiveArgIntrinsics;

} // TargetPrefix = "primate"

//===----------------------------------------------------------------------===//
// Bitmanip (Bit Manipulation) Extension

let TargetPrefix = "primate" in {

  class PrimateBitManipGPRIntrinsics
      : Intrinsic<[llvm_any_ty],
                  [LLVMMatchType<0>],
                  [IntrNoMem, IntrSpeculatable, IntrWillReturn]>;
  class PrimateBitManipGPRGPRIntrinsics
      : Intrinsic<[llvm_any_ty],
                  [LLVMMatchType<0>, LLVMMatchType<0>],
                  [IntrNoMem, IntrSpeculatable, IntrWillReturn]>;

  class PrimateStructMaipIntrinsics
      : Intrinsic<[llvm_any_ty], // return val
                  [llvm_any_ty, llvm_i32_ty], // Params: gpr w/ struct, imm12
		  [IntrNoMem, IntrSpeculatable, IntrWillReturn]>; // properties

  // primate ops
  def int_primate_extract :  Intrinsic<[llvm_any_ty], // return val
                  [llvm_any_ty, llvm_i32_ty], // Params: gpr w/ struct, imm12
		              [IntrNoMem, IntrSpeculatable, IntrWillReturn]>; // properties;
                  
  def int_primate_insert  :  Intrinsic<[llvm_any_ty], // return val
                  [llvm_any_ty, llvm_any_ty, llvm_i32_ty], // Params: gpr w/ struct, res, imm12
		              [IntrNoMem, IntrSpeculatable, IntrWillReturn]>; // properties;	

  // Zbb
  def int_primate_orc_b : PrimateBitManipGPRIntrinsics;

  // Zbc
  def int_primate_clmul  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_clmulh : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_clmulr : PrimateBitManipGPRGPRIntrinsics;

  // Zbe
  def int_primate_bcompress   : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_bdecompress : PrimateBitManipGPRGPRIntrinsics;

  // Zbp
  def int_primate_grev  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_gorc  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_shfl  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_unshfl  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_xperm_n  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_xperm_b  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_xperm_h  : PrimateBitManipGPRGPRIntrinsics;
  def int_primate_xperm_w  : PrimateBitManipGPRGPRIntrinsics;

  // Zbr
  def int_primate_crc32_b : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32_h : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32_w : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32_d : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32c_b : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32c_h : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32c_w : PrimateBitManipGPRIntrinsics;
  def int_primate_crc32c_d : PrimateBitManipGPRIntrinsics;

} // TargetPrefix = "primate"

//===----------------------------------------------------------------------===//
// Vectors

class PrimateVIntrinsic {
  // These intrinsics may accept illegal integer values in their llvm_any_ty
  // operand, so they have to be extended. If set to zero then the intrinsic
  // does not have any operand that must be extended.
  Intrinsic IntrinsicID = !cast<Intrinsic>(NAME);
  bits<4> SplatOperand = 0;
}

let TargetPrefix = "primate" in {
  // We use anyint here but we only support XLen.
  def int_primate_vsetvli   : Intrinsic<[llvm_anyint_ty],
                           /* AVL */  [LLVMMatchType<0>,
                           /* VSEW */  LLVMMatchType<0>,
                           /* VLMUL */ LLVMMatchType<0>],
                                      [IntrNoMem, IntrHasSideEffects,
                                       ImmArg<ArgIndex<1>>,
                                       ImmArg<ArgIndex<2>>]>;
  def int_primate_vsetvlimax : Intrinsic<[llvm_anyint_ty],
                            /* VSEW */ [LLVMMatchType<0>,
                            /* VLMUL */ LLVMMatchType<0>],
                                      [IntrNoMem, IntrHasSideEffects,
                                       ImmArg<ArgIndex<0>>,
                                       ImmArg<ArgIndex<1>>]>;

  // For unit stride load
  // Input: (pointer, vl)
  class PrimateUSLoad
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMPointerType<LLVMMatchType<0>>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride fault-only-first load
  // Input: (pointer, vl)
  // Output: (data, vl)
  // NOTE: We model this with default memory properties since we model writing
  // VL as a side effect. IntrReadMem, IntrHasSideEffects does not work.
  class PrimateUSLoadFF
        : Intrinsic<[llvm_anyvector_ty, llvm_anyint_ty],
                    [LLVMPointerType<LLVMMatchType<0>>, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>]>,
                    PrimateVIntrinsic;
  // For unit stride load with mask
  // Input: (maskedoff, pointer, mask, vl)
  class PrimateUSLoadMask
        : Intrinsic<[llvm_anyvector_ty ],
                    [LLVMMatchType<0>,
                     LLVMPointerType<LLVMMatchType<0>>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride fault-only-first load with mask
  // Input: (maskedoff, pointer, mask, vl)
  // Output: (data, vl)
  // NOTE: We model this with default memory properties since we model writing
  // VL as a side effect. IntrReadMem, IntrHasSideEffects does not work.
  class PrimateUSLoadFFMask
        : Intrinsic<[llvm_anyvector_ty, llvm_anyint_ty],
                    [LLVMMatchType<0>,
                     LLVMPointerType<LLVMMatchType<0>>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>]>, PrimateVIntrinsic;
  // For strided load
  // Input: (pointer, stride, vl)
  class PrimateSLoad
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMPointerType<LLVMMatchType<0>>,
                     llvm_anyint_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For strided load with mask
  // Input: (maskedoff, pointer, stride, mask, vl)
  class PrimateSLoadMask
        : Intrinsic<[llvm_anyvector_ty ],
                    [LLVMMatchType<0>,
                     LLVMPointerType<LLVMMatchType<0>>, llvm_anyint_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>, IntrReadMem]>, PrimateVIntrinsic;
  // For indexed load
  // Input: (pointer, index, vl)
  class PrimateILoad
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMPointerType<LLVMMatchType<0>>,
                     llvm_anyvector_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For indexed load with mask
  // Input: (maskedoff, pointer, index, mask, vl)
  class PrimateILoadMask
        : Intrinsic<[llvm_anyvector_ty ],
                    [LLVMMatchType<0>,
                     LLVMPointerType<LLVMMatchType<0>>, llvm_anyvector_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride store
  // Input: (vector_in, pointer, vl)
  class PrimateUSStore
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     LLVMPointerType<LLVMMatchType<0>>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For unit stride store with mask
  // Input: (vector_in, pointer, mask, vl)
  class PrimateUSStoreMask
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     LLVMPointerType<LLVMMatchType<0>>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For strided store
  // Input: (vector_in, pointer, stride, vl)
  class PrimateSStore
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     LLVMPointerType<LLVMMatchType<0>>,
                     llvm_anyint_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For stride store with mask
  // Input: (vector_in, pointer, stirde, mask, vl)
  class PrimateSStoreMask
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     LLVMPointerType<LLVMMatchType<0>>, llvm_anyint_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For indexed store
  // Input: (vector_in, pointer, index, vl)
  class PrimateIStore
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     LLVMPointerType<LLVMMatchType<0>>,
                     llvm_anyint_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For indexed store with mask
  // Input: (vector_in, pointer, index, mask, vl)
  class PrimateIStoreMask
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     LLVMPointerType<LLVMMatchType<0>>, llvm_anyvector_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as source vector.
  // Input: (vector_in, vl)
  class PrimateUnaryAANoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first source vector (with mask).
  // Input: (vector_in, mask, vl)
  class PrimateUnaryAAMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first and second source vector.
  // Input: (vector_in, vector_in, vl)
  class PrimateBinaryAAANoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first and second source vector.
  // Input: (vector_in, int_vector_in, vl)
  class PrimateRGatherVVNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMVectorOfBitcastsToInt<0>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first and second source vector.
  // Input: (vector_in, vector_in, int_vector_in, vl)
  class PrimateRGatherVVMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, LLVMVectorOfBitcastsToInt<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // Input: (vector_in, int16_vector_in, vl)
  class PrimateRGatherEI16VVNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMScalarOrSameVectorWidth<0, llvm_i16_ty>,
                     llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first and second source vector.
  // Input: (vector_in, vector_in, int16_vector_in, vl)
  class PrimateRGatherEI16VVMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>,
                    LLVMScalarOrSameVectorWidth<0, llvm_i16_ty>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first source vector, and the
  // second operand is XLen.
  // Input: (vector_in, xlen_in, vl)
  class PrimateGatherVXNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyint_ty, LLVMMatchType<1>],
                    [IntrNoMem]>, PrimateVIntrinsic {
  }
  // For destination vector type is the same as first source vector (with mask).
  // Second operand is XLen.
  // Input: (maskedoff, vector_in, xlen_in, mask, vl)
  class PrimateGatherVXMask
       : Intrinsic<[llvm_anyvector_ty],
                   [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyint_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                   [IntrNoMem]>, PrimateVIntrinsic {
  }
  // For destination vector type is the same as first source vector.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryAAXNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For destination vector type is the same as first source vector (with mask).
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateBinaryAAXMask
       : Intrinsic<[llvm_anyvector_ty],
                   [LLVMMatchType<0>, LLVMMatchType<0>, llvm_any_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                   [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 3;
  }
  // For destination vector type is the same as first source vector. The
  // second source operand must match the destination type or be an XLen scalar.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryAAShiftNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is the same as first source vector (with mask).
  // The second source operand must match the destination type or be an XLen scalar.
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateBinaryAAShiftMask
       : Intrinsic<[llvm_anyvector_ty],
                   [LLVMMatchType<0>, LLVMMatchType<0>, llvm_any_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                   [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is NOT the same as first source vector.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryABXNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For destination vector type is NOT the same as first source vector (with mask).
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateBinaryABXMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 3;
  }
  // For destination vector type is NOT the same as first source vector. The
  // second source operand must match the destination type or be an XLen scalar.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryABShiftNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is NOT the same as first source vector (with mask).
  // The second source operand must match the destination type or be an XLen scalar.
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateBinaryABShiftMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For binary operations with V0 as input.
  // Input: (vector_in, vector_in/scalar_in, V0, vl)
  class PrimateBinaryWithV0
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For binary operations with mask type output and V0 as input.
  // Output: (mask type output)
  // Input: (vector_in, vector_in/scalar_in, V0, vl)
  class PrimateBinaryMOutWithV0
        :Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                   [llvm_anyvector_ty, llvm_any_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                    llvm_anyint_ty],
                   [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For binary operations with mask type output.
  // Output: (mask type output)
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryMOut
        : Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For binary operations with mask type output without mask.
  // Output: (mask type output)
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateCompareNoMask
        : Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For binary operations with mask type output with mask.
  // Output: (mask type output)
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateCompareMask
        : Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                    [LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     llvm_anyvector_ty, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 3;
  }
  // For FP classify operations.
  // Output: (bit mask type output)
  // Input: (vector_in, vl)
  class PrimateClassifyNoMask
        : Intrinsic<[LLVMVectorOfBitcastsToInt<0>],
                    [llvm_anyvector_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For FP classify operations with mask.
  // Output: (bit mask type output)
  // Input: (maskedoff, vector_in, mask, vl)
  class PrimateClassifyMask
        : Intrinsic<[LLVMVectorOfBitcastsToInt<0>],
                    [LLVMVectorOfBitcastsToInt<0>, llvm_anyvector_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For Saturating binary operations.
  // The destination vector type is the same as first source vector.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateSaturatingBinaryAAXNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For Saturating binary operations with mask.
  // The destination vector type is the same as first source vector.
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateSaturatingBinaryAAXMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic {
    let SplatOperand = 3;
  }
  // For Saturating binary operations.
  // The destination vector type is the same as first source vector.
  // The second source operand matches the destination type or is an XLen scalar.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateSaturatingBinaryAAShiftNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic;
  // For Saturating binary operations with mask.
  // The destination vector type is the same as first source vector.
  // The second source operand matches the destination type or is an XLen scalar.
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateSaturatingBinaryAAShiftMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic;
  // For Saturating binary operations.
  // The destination vector type is NOT the same as first source vector.
  // The second source operand matches the destination type or is an XLen scalar.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateSaturatingBinaryABShiftNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic;
  // For Saturating binary operations with mask.
  // The destination vector type is NOT the same as first source vector (with mask).
  // The second source operand matches the destination type or is an XLen scalar.
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateSaturatingBinaryABShiftMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic;
  class PrimateTernaryAAAXNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyint_ty,
                     LLVMMatchType<1>],
                    [IntrNoMem]>, PrimateVIntrinsic;
  class PrimateTernaryAAAXMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyint_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                    [IntrNoMem]>, PrimateVIntrinsic;
  class PrimateTernaryAAXANoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, LLVMMatchType<0>,
                     llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  class PrimateTernaryAAXAMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  class PrimateTernaryWideNoMask
        : Intrinsic< [llvm_anyvector_ty],
                     [LLVMMatchType<0>, llvm_any_ty, llvm_anyvector_ty,
                      llvm_anyint_ty],
                     [IntrNoMem] >, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  class PrimateTernaryWideMask
        : Intrinsic< [llvm_anyvector_ty],
                     [LLVMMatchType<0>, llvm_any_ty, llvm_anyvector_ty,
                      LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                     [IntrNoMem]>, PrimateVIntrinsic {
    let SplatOperand = 2;
  }
  // For Reduction ternary operations.
  // For destination vector type is the same as first and third source vector.
  // Input: (vector_in, vector_in, vector_in, vl)
  class PrimateReductionNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty, LLVMMatchType<0>,
                     llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For Reduction ternary operations with mask.
  // For destination vector type is the same as first and third source vector.
  // The mask type come from second source vector.
  // Input: (maskedoff, vector_in, vector_in, vector_in, mask, vl)
  class PrimateReductionMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty, LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<1, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For unary operations with scalar type output without mask
  // Output: (scalar type)
  // Input: (vector_in, vl)
  class PrimateMaskUnarySOutNoMask
        : Intrinsic<[LLVMMatchType<1>],
                    [llvm_anyvector_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For unary operations with scalar type output with mask
  // Output: (scalar type)
  // Input: (vector_in, mask, vl)
  class PrimateMaskUnarySOutMask
        : Intrinsic<[LLVMMatchType<1>],
                    [llvm_anyvector_ty, LLVMMatchType<0>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is NOT the same as source vector.
  // Input: (vector_in, vl)
  class PrimateUnaryABNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_anyvector_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For destination vector type is NOT the same as source vector (with mask).
  // Input: (maskedoff, vector_in, mask, vl)
  class PrimateUnaryABMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty,
                     LLVMScalarOrSameVectorWidth<1, llvm_i1_ty>,
                     llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For unary operations with the same vector type in/out without mask
  // Output: (vector)
  // Input: (vector_in, vl)
  class PrimateUnaryNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For mask unary operations with mask type in/out with mask
  // Output: (mask type output)
  // Input: (mask type maskedoff, mask type vector_in, mask, vl)
  class PrimateMaskUnaryMOutMask
        : Intrinsic<[llvm_anyint_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>,
                     LLVMMatchType<0>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // Output: (vector)
  // Input: (vl)
  class PrimateNullaryIntrinsic
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For Conversion unary operations.
  // Input: (vector_in, vl)
  class PrimateConversionNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_anyvector_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For Conversion unary operations with mask.
  // Input: (maskedoff, vector_in, mask, vl)
  class PrimateConversionMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic;
  // For atomic operations without mask
  // Input: (base, index, value, vl)
  class PrimateAMONoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMPointerType<LLVMMatchType<0>>, llvm_anyvector_ty, LLVMMatchType<0>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>]>, PrimateVIntrinsic;
  // For atomic operations with mask
  // Input: (base, index, value, mask, vl)
  class PrimateAMOMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMPointerType<LLVMMatchType<0>>, llvm_anyvector_ty, LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>]>, PrimateVIntrinsic;

  // For unit stride segment load
  // Input: (pointer, vl)
  class PrimateUSSegLoad<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    [LLVMPointerToElt<0>, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride segment load with mask
  // Input: (maskedoff, pointer, mask, vl)
  class PrimateUSSegLoadMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                                [LLVMPointerToElt<0>,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrReadMem]>, PrimateVIntrinsic;

  // For unit stride fault-only-first segment load
  // Input: (pointer, vl)
  // Output: (data, vl)
  // NOTE: We model this with default memory properties since we model writing
  // VL as a side effect. IntrReadMem, IntrHasSideEffects does not work.
  class PrimateUSSegLoadFF<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1)), [llvm_anyint_ty]),
                    [LLVMPointerToElt<0>, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>]>, PrimateVIntrinsic;
  // For unit stride fault-only-first segment load with mask
  // Input: (maskedoff, pointer, mask, vl)
  // Output: (data, vl)
  // NOTE: We model this with default memory properties since we model writing
  // VL as a side effect. IntrReadMem, IntrHasSideEffects does not work.
  class PrimateUSSegLoadFFMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1)), [llvm_anyint_ty]),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                     [LLVMPointerToElt<0>,
                      LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                      LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>]>, PrimateVIntrinsic;

  // For stride segment load
  // Input: (pointer, offset, vl)
  class PrimateSSegLoad<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    [LLVMPointerToElt<0>, llvm_anyint_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For stride segment load with mask
  // Input: (maskedoff, pointer, offset, mask, vl)
  class PrimateSSegLoadMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                                [LLVMPointerToElt<0>,
                                 llvm_anyint_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>, IntrReadMem]>, PrimateVIntrinsic;

  // For indexed segment load
  // Input: (pointer, index, vl)
  class PrimateISegLoad<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    [LLVMPointerToElt<0>, llvm_anyvector_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For indexed segment load with mask
  // Input: (maskedoff, pointer, index, mask, vl)
  class PrimateISegLoadMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                                [LLVMPointerToElt<0>,
                                 llvm_anyvector_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrReadMem]>, PrimateVIntrinsic;

  // For unit stride segment store
  // Input: (value, pointer, vl)
  class PrimateUSSegStore<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [LLVMPointerToElt<0>, llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For unit stride segment store with mask
  // Input: (value, pointer, mask, vl)
  class PrimateUSSegStoreMask<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [LLVMPointerToElt<0>,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;

  // For stride segment store
  // Input: (value, pointer, offset, vl)
  class PrimateSSegStore<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [LLVMPointerToElt<0>, llvm_anyint_ty,
                                 LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For stride segment store with mask
  // Input: (value, pointer, offset, mask, vl)
  class PrimateSSegStoreMask<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [LLVMPointerToElt<0>, llvm_anyint_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;

  // For indexed segment store
  // Input: (value, pointer, offset, vl)
  class PrimateISegStore<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [LLVMPointerToElt<0>, llvm_anyvector_ty,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For indexed segment store with mask
  // Input: (value, pointer, offset, mask, vl)
  class PrimateISegStoreMask<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [LLVMPointerToElt<0>, llvm_anyvector_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;

  multiclass PrimateUSLoad {
    def "int_primate_" # NAME : PrimateUSLoad;
    def "int_primate_" # NAME # "_mask" : PrimateUSLoadMask;
  }
  multiclass PrimateUSLoadFF {
    def "int_primate_" # NAME : PrimateUSLoadFF;
    def "int_primate_" # NAME # "_mask" : PrimateUSLoadFFMask;
  }
  multiclass PrimateSLoad {
    def "int_primate_" # NAME : PrimateSLoad;
    def "int_primate_" # NAME # "_mask" : PrimateSLoadMask;
  }
  multiclass PrimateILoad {
    def "int_primate_" # NAME : PrimateILoad;
    def "int_primate_" # NAME # "_mask" : PrimateILoadMask;
  }
  multiclass PrimateUSStore {
    def "int_primate_" # NAME : PrimateUSStore;
    def "int_primate_" # NAME # "_mask" : PrimateUSStoreMask;
  }
  multiclass PrimateSStore {
    def "int_primate_" # NAME : PrimateSStore;
    def "int_primate_" # NAME # "_mask" : PrimateSStoreMask;
  }

  multiclass PrimateIStore {
    def "int_primate_" # NAME : PrimateIStore;
    def "int_primate_" # NAME # "_mask" : PrimateIStoreMask;
  }
  multiclass PrimateUnaryAA {
    def "int_primate_" # NAME : PrimateUnaryAANoMask;
    def "int_primate_" # NAME # "_mask" : PrimateUnaryAAMask;
  }
  multiclass PrimateUnaryAB {
    def "int_primate_" # NAME : PrimateUnaryABNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateUnaryABMask;
  }
  // AAX means the destination type(A) is the same as the first source
  // type(A). X means any type for the second source operand.
  multiclass PrimateBinaryAAX {
    def "int_primate_" # NAME : PrimateBinaryAAXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryAAXMask;
  }
  // Like PrimateBinaryAAX, but the second operand is used a shift amount so it
  // must be a vector or an XLen scalar.
  multiclass PrimateBinaryAAShift {
    def "int_primate_" # NAME : PrimateBinaryAAShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryAAShiftMask;
  }
  multiclass PrimateRGatherVV {
    def "int_primate_" # NAME : PrimateRGatherVVNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateRGatherVVMask;
  }
  multiclass PrimateRGatherVX {
    def "int_primate_" # NAME : PrimateGatherVXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateGatherVXMask;
  }
  multiclass PrimateRGatherEI16VV {
    def "int_primate_" # NAME : PrimateRGatherEI16VVNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateRGatherEI16VVMask;
  }
  // ABX means the destination type(A) is different from the first source
  // type(B). X means any type for the second source operand.
  multiclass PrimateBinaryABX {
    def "int_primate_" # NAME : PrimateBinaryABXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryABXMask;
  }
  // Like PrimateBinaryABX, but the second operand is used a shift amount so it
  // must be a vector or an XLen scalar.
  multiclass PrimateBinaryABShift {
    def "int_primate_" # NAME : PrimateBinaryABShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryABShiftMask;
  }
  multiclass PrimateBinaryWithV0 {
    def "int_primate_" # NAME : PrimateBinaryWithV0;
  }
  multiclass PrimateBinaryMaskOutWithV0 {
    def "int_primate_" # NAME : PrimateBinaryMOutWithV0;
  }
  multiclass PrimateBinaryMaskOut {
    def "int_primate_" # NAME : PrimateBinaryMOut;
  }
  multiclass PrimateSaturatingBinaryAAX {
    def "int_primate_" # NAME : PrimateSaturatingBinaryAAXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateSaturatingBinaryAAXMask;
  }
  multiclass PrimateSaturatingBinaryAAShift {
    def "int_primate_" # NAME : PrimateSaturatingBinaryAAShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateSaturatingBinaryAAShiftMask;
  }
  multiclass PrimateSaturatingBinaryABShift {
    def "int_primate_" # NAME : PrimateSaturatingBinaryABShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateSaturatingBinaryABShiftMask;
  }
  multiclass PrimateTernaryAAAX {
    def "int_primate_" # NAME : PrimateTernaryAAAXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateTernaryAAAXMask;
  }
  multiclass PrimateTernaryAAXA {
    def "int_primate_" # NAME : PrimateTernaryAAXANoMask;
    def "int_primate_" # NAME # "_mask" : PrimateTernaryAAXAMask;
  }
  multiclass PrimateCompare {
    def "int_primate_" # NAME : PrimateCompareNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateCompareMask;
  }
  multiclass PrimateClassify {
    def "int_primate_" # NAME : PrimateClassifyNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateClassifyMask;
  }
  multiclass PrimateTernaryWide {
    def "int_primate_" # NAME : PrimateTernaryWideNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateTernaryWideMask;
  }
  multiclass PrimateReduction {
    def "int_primate_" # NAME : PrimateReductionNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateReductionMask;
  }
  multiclass PrimateMaskUnarySOut {
    def "int_primate_" # NAME : PrimateMaskUnarySOutNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateMaskUnarySOutMask;
  }
  multiclass PrimateMaskUnaryMOut {
    def "int_primate_" # NAME : PrimateUnaryNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateMaskUnaryMOutMask;
  }
  multiclass PrimateConversion {
    def "int_primate_" #NAME :PrimateConversionNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateConversionMask;
  }
  multiclass PrimateAMO {
    def "int_primate_" # NAME : PrimateAMONoMask;
    def "int_primate_" # NAME # "_mask" : PrimateAMOMask;
  }
  multiclass PrimateUSSegLoad<int nf> {
    def "int_primate_" # NAME : PrimateUSSegLoad<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateUSSegLoadMask<nf>;
  }
  multiclass PrimateUSSegLoadFF<int nf> {
    def "int_primate_" # NAME : PrimateUSSegLoadFF<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateUSSegLoadFFMask<nf>;
  }
  multiclass PrimateSSegLoad<int nf> {
    def "int_primate_" # NAME : PrimateSSegLoad<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateSSegLoadMask<nf>;
  }
  multiclass PrimateISegLoad<int nf> {
    def "int_primate_" # NAME : PrimateISegLoad<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateISegLoadMask<nf>;
  }
  multiclass PrimateUSSegStore<int nf> {
    def "int_primate_" # NAME : PrimateUSSegStore<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateUSSegStoreMask<nf>;
  }
  multiclass PrimateSSegStore<int nf> {
    def "int_primate_" # NAME : PrimateSSegStore<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateSSegStoreMask<nf>;
  }
  multiclass PrimateISegStore<int nf> {
    def "int_primate_" # NAME : PrimateISegStore<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateISegStoreMask<nf>;
  }

  defm vle : PrimateUSLoad;
  defm vleff : PrimateUSLoadFF;
  defm vse : PrimateUSStore;
  defm vlse: PrimateSLoad;
  defm vsse: PrimateSStore;
  defm vluxei : PrimateILoad;
  defm vloxei : PrimateILoad;
  defm vsoxei : PrimateIStore;
  defm vsuxei : PrimateIStore;

  def int_primate_vle1 : PrimateUSLoad;
  def int_primate_vse1 : PrimateUSStore;

  defm vamoswap : PrimateAMO;
  defm vamoadd : PrimateAMO;
  defm vamoxor : PrimateAMO;
  defm vamoand : PrimateAMO;
  defm vamoor : PrimateAMO;
  defm vamomin : PrimateAMO;
  defm vamomax : PrimateAMO;
  defm vamominu : PrimateAMO;
  defm vamomaxu : PrimateAMO;

  defm vadd : PrimateBinaryAAX;
  defm vsub : PrimateBinaryAAX;
  defm vrsub : PrimateBinaryAAX;

  defm vwaddu : PrimateBinaryABX;
  defm vwadd : PrimateBinaryABX;
  defm vwaddu_w : PrimateBinaryAAX;
  defm vwadd_w : PrimateBinaryAAX;
  defm vwsubu : PrimateBinaryABX;
  defm vwsub : PrimateBinaryABX;
  defm vwsubu_w : PrimateBinaryAAX;
  defm vwsub_w : PrimateBinaryAAX;

  defm vzext : PrimateUnaryAB;
  defm vsext : PrimateUnaryAB;

  defm vadc : PrimateBinaryWithV0;
  defm vmadc_carry_in : PrimateBinaryMaskOutWithV0;
  defm vmadc : PrimateBinaryMaskOut;

  defm vsbc : PrimateBinaryWithV0;
  defm vmsbc_borrow_in : PrimateBinaryMaskOutWithV0;
  defm vmsbc : PrimateBinaryMaskOut;

  defm vand : PrimateBinaryAAX;
  defm vor : PrimateBinaryAAX;
  defm vxor : PrimateBinaryAAX;

  defm vsll : PrimateBinaryAAShift;
  defm vsrl : PrimateBinaryAAShift;
  defm vsra : PrimateBinaryAAShift;

  defm vnsrl : PrimateBinaryABShift;
  defm vnsra : PrimateBinaryABShift;

  defm vmseq : PrimateCompare;
  defm vmsne : PrimateCompare;
  defm vmsltu : PrimateCompare;
  defm vmslt : PrimateCompare;
  defm vmsleu : PrimateCompare;
  defm vmsle : PrimateCompare;
  defm vmsgtu : PrimateCompare;
  defm vmsgt : PrimateCompare;
  defm vmsgeu : PrimateCompare;
  defm vmsge : PrimateCompare;

  defm vminu : PrimateBinaryAAX;
  defm vmin : PrimateBinaryAAX;
  defm vmaxu : PrimateBinaryAAX;
  defm vmax : PrimateBinaryAAX;

  defm vmul : PrimateBinaryAAX;
  defm vmulh : PrimateBinaryAAX;
  defm vmulhu : PrimateBinaryAAX;
  defm vmulhsu : PrimateBinaryAAX;

  defm vdivu : PrimateBinaryAAX;
  defm vdiv : PrimateBinaryAAX;
  defm vremu : PrimateBinaryAAX;
  defm vrem : PrimateBinaryAAX;

  defm vwmul : PrimateBinaryABX;
  defm vwmulu : PrimateBinaryABX;
  defm vwmulsu : PrimateBinaryABX;

  defm vmacc : PrimateTernaryAAXA;
  defm vnmsac : PrimateTernaryAAXA;
  defm vmadd : PrimateTernaryAAXA;
  defm vnmsub : PrimateTernaryAAXA;

  defm vwmaccu  : PrimateTernaryWide;
  defm vwmacc   : PrimateTernaryWide;
  defm vwmaccus : PrimateTernaryWide;
  defm vwmaccsu : PrimateTernaryWide;

  defm vfadd : PrimateBinaryAAX;
  defm vfsub : PrimateBinaryAAX;
  defm vfrsub : PrimateBinaryAAX;

  defm vfwadd : PrimateBinaryABX;
  defm vfwsub : PrimateBinaryABX;
  defm vfwadd_w : PrimateBinaryAAX;
  defm vfwsub_w : PrimateBinaryAAX;

  defm vsaddu : PrimateSaturatingBinaryAAX;
  defm vsadd : PrimateSaturatingBinaryAAX;
  defm vssubu : PrimateSaturatingBinaryAAX;
  defm vssub : PrimateSaturatingBinaryAAX;

  def int_primate_vmerge : PrimateBinaryWithV0;

  def int_primate_vmv_v_v : Intrinsic<[llvm_anyvector_ty],
                                    [LLVMMatchType<0>, llvm_anyint_ty],
                                    [IntrNoMem]>, PrimateVIntrinsic;
  def int_primate_vmv_v_x : Intrinsic<[llvm_anyint_ty],
                                    [LLVMVectorElementType<0>, llvm_anyint_ty],
                                    [IntrNoMem]>, PrimateVIntrinsic;
  def int_primate_vfmv_v_f : Intrinsic<[llvm_anyfloat_ty],
                                     [LLVMVectorElementType<0>, llvm_anyint_ty],
                                     [IntrNoMem]>, PrimateVIntrinsic;

  def int_primate_vmv_x_s : Intrinsic<[LLVMVectorElementType<0>],
                                    [llvm_anyint_ty],
                                    [IntrNoMem]>, PrimateVIntrinsic;
  def int_primate_vmv_s_x : Intrinsic<[llvm_anyint_ty],
                                    [LLVMMatchType<0>, LLVMVectorElementType<0>,
                                     llvm_anyint_ty],
                                    [IntrNoMem]>, PrimateVIntrinsic;

  def int_primate_vfmv_f_s : Intrinsic<[LLVMVectorElementType<0>],
                                     [llvm_anyfloat_ty],
                                     [IntrNoMem]>, PrimateVIntrinsic;
  def int_primate_vfmv_s_f : Intrinsic<[llvm_anyfloat_ty],
                                     [LLVMMatchType<0>, LLVMVectorElementType<0>,
                                      llvm_anyint_ty],
                                     [IntrNoMem]>, PrimateVIntrinsic;

  defm vfmul : PrimateBinaryAAX;
  defm vfdiv : PrimateBinaryAAX;
  defm vfrdiv : PrimateBinaryAAX;

  defm vfwmul : PrimateBinaryABX;

  defm vfmacc : PrimateTernaryAAXA;
  defm vfnmacc : PrimateTernaryAAXA;
  defm vfmsac : PrimateTernaryAAXA;
  defm vfnmsac : PrimateTernaryAAXA;
  defm vfmadd : PrimateTernaryAAXA;
  defm vfnmadd : PrimateTernaryAAXA;
  defm vfmsub : PrimateTernaryAAXA;
  defm vfnmsub : PrimateTernaryAAXA;

  defm vfwmacc : PrimateTernaryWide;
  defm vfwnmacc : PrimateTernaryWide;
  defm vfwmsac : PrimateTernaryWide;
  defm vfwnmsac : PrimateTernaryWide;

  defm vfsqrt : PrimateUnaryAA;
  defm vfrsqrt7 : PrimateUnaryAA;
  defm vfrec7 : PrimateUnaryAA;

  defm vfmin : PrimateBinaryAAX;
  defm vfmax : PrimateBinaryAAX;

  defm vfsgnj : PrimateBinaryAAX;
  defm vfsgnjn : PrimateBinaryAAX;
  defm vfsgnjx : PrimateBinaryAAX;

  defm vfclass : PrimateClassify;

  defm vfmerge : PrimateBinaryWithV0;

  defm vslideup : PrimateTernaryAAAX;
  defm vslidedown : PrimateTernaryAAAX;

  defm vslide1up : PrimateBinaryAAX;
  defm vslide1down : PrimateBinaryAAX;
  defm vfslide1up : PrimateBinaryAAX;
  defm vfslide1down : PrimateBinaryAAX;

  defm vrgather_vv : PrimateRGatherVV;
  defm vrgather_vx : PrimateRGatherVX;
  defm vrgatherei16_vv : PrimateRGatherEI16VV;

  def "int_primate_vcompress" : PrimateUnaryAAMask;

  defm vaaddu : PrimateSaturatingBinaryAAX;
  defm vaadd : PrimateSaturatingBinaryAAX;
  defm vasubu : PrimateSaturatingBinaryAAX;
  defm vasub : PrimateSaturatingBinaryAAX;

  defm vsmul : PrimateSaturatingBinaryAAX;

  defm vssrl : PrimateSaturatingBinaryAAShift;
  defm vssra : PrimateSaturatingBinaryAAShift;

  defm vnclipu : PrimateSaturatingBinaryABShift;
  defm vnclip : PrimateSaturatingBinaryABShift;

  defm vmfeq : PrimateCompare;
  defm vmfne : PrimateCompare;
  defm vmflt : PrimateCompare;
  defm vmfle : PrimateCompare;
  defm vmfgt : PrimateCompare;
  defm vmfge : PrimateCompare;

  defm vredsum : PrimateReduction;
  defm vredand : PrimateReduction;
  defm vredor : PrimateReduction;
  defm vredxor : PrimateReduction;
  defm vredminu : PrimateReduction;
  defm vredmin : PrimateReduction;
  defm vredmaxu : PrimateReduction;
  defm vredmax : PrimateReduction;

  defm vwredsumu : PrimateReduction;
  defm vwredsum : PrimateReduction;

  defm vfredosum : PrimateReduction;
  defm vfredsum : PrimateReduction;
  defm vfredmin : PrimateReduction;
  defm vfredmax : PrimateReduction;

  defm vfwredsum : PrimateReduction;
  defm vfwredosum : PrimateReduction;

  def int_primate_vmand: PrimateBinaryAAANoMask;
  def int_primate_vmnand: PrimateBinaryAAANoMask;
  def int_primate_vmandnot: PrimateBinaryAAANoMask;
  def int_primate_vmxor: PrimateBinaryAAANoMask;
  def int_primate_vmor: PrimateBinaryAAANoMask;
  def int_primate_vmnor: PrimateBinaryAAANoMask;
  def int_primate_vmornot: PrimateBinaryAAANoMask;
  def int_primate_vmxnor: PrimateBinaryAAANoMask;
  def int_primate_vmclr : PrimateNullaryIntrinsic;
  def int_primate_vmset : PrimateNullaryIntrinsic;

  defm vpopc : PrimateMaskUnarySOut;
  defm vfirst : PrimateMaskUnarySOut;
  defm vmsbf : PrimateMaskUnaryMOut;
  defm vmsof : PrimateMaskUnaryMOut;
  defm vmsif : PrimateMaskUnaryMOut;

  defm vfcvt_xu_f_v : PrimateConversion;
  defm vfcvt_x_f_v : PrimateConversion;
  defm vfcvt_rtz_xu_f_v : PrimateConversion;
  defm vfcvt_rtz_x_f_v : PrimateConversion;
  defm vfcvt_f_xu_v : PrimateConversion;
  defm vfcvt_f_x_v : PrimateConversion;

  defm vfwcvt_f_xu_v : PrimateConversion;
  defm vfwcvt_f_x_v : PrimateConversion;
  defm vfwcvt_xu_f_v : PrimateConversion;
  defm vfwcvt_x_f_v : PrimateConversion;
  defm vfwcvt_rtz_xu_f_v : PrimateConversion;
  defm vfwcvt_rtz_x_f_v : PrimateConversion;
  defm vfwcvt_f_f_v : PrimateConversion;

  defm vfncvt_f_xu_w : PrimateConversion;
  defm vfncvt_f_x_w : PrimateConversion;
  defm vfncvt_xu_f_w : PrimateConversion;
  defm vfncvt_x_f_w : PrimateConversion;
  defm vfncvt_rtz_xu_f_w : PrimateConversion;
  defm vfncvt_rtz_x_f_w : PrimateConversion;
  defm vfncvt_f_f_w : PrimateConversion;
  defm vfncvt_rod_f_f_w : PrimateConversion;

  // Output: (vector)
  // Input: (mask type input, vl)
  def int_primate_viota : Intrinsic<[llvm_anyvector_ty],
                                  [LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                   llvm_anyint_ty],
                                  [IntrNoMem]>, PrimateVIntrinsic;
  // Output: (vector)
  // Input: (maskedoff, mask type vector_in, mask, vl)
  def int_primate_viota_mask : Intrinsic<[llvm_anyvector_ty],
                                       [LLVMMatchType<0>,
                                        LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                        LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                        llvm_anyint_ty],
                                       [IntrNoMem]>, PrimateVIntrinsic;
  // Output: (vector)
  // Input: (vl)
  def int_primate_vid : PrimateNullaryIntrinsic;

  // Output: (vector)
  // Input: (maskedoff, mask, vl)
  def int_primate_vid_mask : Intrinsic<[llvm_anyvector_ty],
                                     [LLVMMatchType<0>,
                                      LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                      llvm_anyint_ty],
                                     [IntrNoMem]>, PrimateVIntrinsic;

  foreach nf = [2, 3, 4, 5, 6, 7, 8] in {
    defm vlseg # nf : PrimateUSSegLoad<nf>;
    defm vlseg # nf # ff : PrimateUSSegLoadFF<nf>;
    defm vlsseg # nf : PrimateSSegLoad<nf>;
    defm vloxseg # nf : PrimateISegLoad<nf>;
    defm vluxseg # nf : PrimateISegLoad<nf>;
    defm vsseg # nf : PrimateUSSegStore<nf>;
    defm vssseg # nf : PrimateSSegStore<nf>;
    defm vsoxseg # nf : PrimateISegStore<nf>;
    defm vsuxseg # nf : PrimateISegStore<nf>;
  }

} // TargetPrefix = "primate"
