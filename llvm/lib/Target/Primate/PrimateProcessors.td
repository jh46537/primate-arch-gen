//===-- PrimateProcessors.td - Primate Processors -------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Primate processors supported.
//===----------------------------------------------------------------------===//

class PrimateTuneInfo {
  bits<8> PrefFunctionAlignment = 1;
  bits<8> PrefLoopAlignment = 1;

  // Information needed by LoopDataPrefetch.
  bits<16> CacheLineSize = 0;
  bits<16> PrefetchDistance = 0;
  bits<16> MinPrefetchStride = 1;
  bits<32> MaxPrefetchIterationsAhead = -1;

  bits<32> MinimumJumpTableEntries = 5;
}

def PrimateTuneInfoTable : GenericTable {
  let FilterClass = "PrimateTuneInfo";
  let CppTypeName = "PrimateTuneInfo";
  let Fields = ["Name", "PrefFunctionAlignment", "PrefLoopAlignment",
                "CacheLineSize", "PrefetchDistance",
                "MinPrefetchStride", "MaxPrefetchIterationsAhead",
                "MinimumJumpTableEntries"];
}

def getPrimateTuneInfo : SearchIndex {
  let Table = PrimateTuneInfoTable;
  let Key = ["Name"];
}

class GenericTuneInfo: PrimateTuneInfo;

class PrimateProcessorModel<string n,
                          SchedMachineModel m,
                          list<SubtargetFeature> f,
                          list<SubtargetFeature> tunef = [],
                          string default_march = "">
      :  ProcessorModel<n, m, f, tunef> {
  string DefaultMarch = default_march;
}

class PrimateTuneProcessorModel<string n,
                              SchedMachineModel m,
                              list<SubtargetFeature> tunef = [],
                              list<SubtargetFeature> f = []>
      : ProcessorModel<n, m, f,tunef>;

def GENERIC_PR32 : PrimateProcessorModel<"generic-pr32",
                                       PrimateModel,
                                       [Feature32Bit]>,
                   GenericTuneInfo;
def GENERIC_PR64 : PrimateProcessorModel<"generic-pr64",
                                       NoSchedModel,
                                       [Feature64Bit]>,
                   GenericTuneInfo;
// Support generic for compatibility with other targets. The triple will be used
// to change to the appropriate pr32/pr64 version.
def : ProcessorModel<"generic", NoSchedModel, []>, GenericTuneInfo;

def PRIMATE_PR32 : PrimateProcessorModel<"PrimateModel",
                                      PrimateModel,
                                      [Feature32Bit]>;

