//=- PrimateCombine.td - Define Primate Combine Rules -----------*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

include "llvm/Target/GlobalISel/Combine.td"

def PrimatePreLegalizerCombiner: GICombiner<
  "PrimatePreLegalizerCombinerImpl", [all_combines]> {
}

def PrimateO0PreLegalizerCombiner: GICombiner<
  "PrimateO0PreLegalizerCombinerImpl", [optnone_combines]> {
}

// Post-legalization combines which are primarily optimizations.
// TODO: Add more combines.
def PrimatePostLegalizerCombiner
    : GICombiner<"PrimatePostLegalizerCombinerImpl",
                 [redundant_and, identity_combines]> {
}
