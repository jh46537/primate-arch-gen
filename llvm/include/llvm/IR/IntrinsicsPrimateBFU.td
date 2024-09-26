
  //===- IntrinsicsPrimateBFU.td - Defines Primate BFU intrinsics ---*- tablegen -*-===//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===-----------------------------------------------------------------------------===//
  // BFU Extension

  let TargetPrefix = "primate" in {  
    def int_primate_BFU_0 :  Intrinsic<[llvm_any_ty], // return val
                    [llvm_any_ty], // Params: gpr w/ struct
                    [IntrHasSideEffects]>; // properties;
                  
  } // TargetPrefix = "primate"
  
