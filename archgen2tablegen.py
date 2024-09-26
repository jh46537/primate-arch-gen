#!/bin/python3

import re
import os
import sys
import argparse

parser = argparse.ArgumentParser(
                    prog='archgen2tablegen',
                    description='converts the output of archgen to tablegen files',
                    epilog='Shout at kayvan if this fails :D')
parser.add_argument('-b', '--bfu_list', type=str, help='Path to BFU_list.txt')
parser.add_argument('-p', '--primate_cfg', type=str, help='Path to primate.cfg')
parser.add_argument('--output', type=str, help='Output directory for tablegen files', default='./primate-compiler-gen/')
parser.add_argument('--FrontendOnly', action='store_true', help='only generates the intrinsics and builtins, not the full schedule', default=False)
parser.add_argument('--dry-run', action='store_true', help='Prints the output to stdout instead of writing to files', default=False)
parser.add_argument('--verbose', action='store_true', help='Prints debug information', default=False)

# globals go hard fuck you
gen_file_dir = ""
DRY_RUN = False
VERBOSE = False

def comb_str(in_str, num_iter):
    return "".join([in_str.format(i) for i in range(num_iter)])

# returns the number of unique BFUs
def parse_BFU_list(file_path):
    with open(file_path, 'r') as f:
      # just return the number of { in the file
      return len(re.findall(r'{', f.read()))

# returns the number of BFUs and ALUs archgen instanced
def parse_arch_config(file_path):
  with open(file_path, 'r') as f:
    for line in f:
      if(VERBOSE):
        print(line)
      toks = line.split("=")
      if toks[0] == "NUM_ALUS":
        numALUs = int(toks[1])
      if toks[0] == "NUM_BFUS":
        numBFUs = int(toks[1]) + 2 # IO and LSU are hidden
  return numALUs, numBFUs

# write the schedule itself in VLIW slot order
# num_bfus is number of instanced BFUs
# will need to be updated to have BFU ordering for now is in order of definition in 
# bfu_list.txt 
def write_schedule(num_bfus: int, num_alus: int):
  numSlots = max(num_bfus, num_alus)

  # BFUs and ALUs are merged starting with the last BFU slot. 
  if num_alus >= num_bfus:
    hasGFU = [True] * numSlots
    hasBFU = [True] * (num_bfus) + [False] * (numSlots-num_bfus)
  else:
    hasGFU = [True] * (num_alus) + [False] * (num_bfus - num_alus)
    hasBFU = [True] * numSlots

  IOSlot = num_bfus - 1
  LSUSlot = num_bfus - 2

  if(VERBOSE):
    print(f"hasGFU: {hasGFU}")
    print(f"hasBFU: {hasBFU}")

  unitDefTemplate = """def {0}      : FuncUnit;\n"""
  BFUItinDataTemplate = """InstrItinData<ItinBlue{0},         [InstrStage<1, [{1}]>]>,\n"""

  extractaUnitDef = """ExtractUnit{0}a"""
  extractbUnitDef = """ExtractUnit{0}b"""
  insertUnitDef = """InsertUnit{0}"""
  mergedUnitDef = """GreenBlueUnit{0}"""
  greenUnitDef = """GreenUnit{0}"""
  blueUnitDef = """BlueUnit{0}"""
  LSUnitMergedDef = """GreenLSUUnit"""
  IOUnitMergedDef = """GreenIOUnit"""
  IOUnitDef = """IOUnit"""
  LSUnitDef = """LSUUnit"""


  funcUnitDef = ""
  BFUItinData = ""
  allExtractUnitNames = []
  allInsertUnitNames = []
  allBFUnitNames = []
  allGFUnitNames = []
  allIOUnitNames = []
  allLSUnitNames = []
  packetOrderUnitNames = []
  for slot, (gfu, bfu) in enumerate(zip(hasGFU, hasBFU)):
    if gfu:
      allExtractUnitNames += [extractaUnitDef.format(slot), extractbUnitDef.format(slot)]
      packetOrderUnitNames += [extractaUnitDef.format(slot), extractbUnitDef.format(slot)]
      funcUnitDef += unitDefTemplate.format(extractaUnitDef).format(slot)
      funcUnitDef += unitDefTemplate.format(extractbUnitDef).format(slot)
    if gfu and bfu:
      if slot == IOSlot:
        allGFUnitNames += [IOUnitMergedDef.format(slot)]
        allIOUnitNames += [IOUnitMergedDef.format(slot)]
        packetOrderUnitNames += [IOUnitMergedDef.format(slot)]
        funcUnitDef += unitDefTemplate.format(IOUnitMergedDef).format(slot)
      elif slot == LSUSlot:
        allGFUnitNames += [LSUnitMergedDef.format(slot)]
        allLSUnitNames += [LSUnitMergedDef.format(slot)]
        packetOrderUnitNames += [LSUnitMergedDef.format(slot)]
        funcUnitDef += unitDefTemplate.format(LSUnitMergedDef).format(slot)
      else:
        BFUItinData += BFUItinDataTemplate.format(slot, mergedUnitDef.format(slot))
        allGFUnitNames += [mergedUnitDef.format(slot)]
        allBFUnitNames += [mergedUnitDef.format(slot)]
        packetOrderUnitNames += [mergedUnitDef.format(slot)]
        funcUnitDef += unitDefTemplate.format(mergedUnitDef).format(slot)
    elif gfu:
      allGFUnitNames += [greenUnitDef.format(slot)]
      packetOrderUnitNames += [greenUnitDef.format(slot)]
      funcUnitDef += unitDefTemplate.format(greenUnitDef).format(slot)
    elif bfu:
      if slot == IOSlot:
        allIOUnitNames += [IOUnitDef.format(slot)]
        packetOrderUnitNames += [IOUnitDef.format(slot)]
        funcUnitDef += unitDefTemplate.format(IOUnitDef).format(slot)
      elif slot == LSUSlot:
        allLSUnitNames += [LSUnitDef.format(slot)]
        packetOrderUnitNames += [LSUnitDef.format(slot)]
        funcUnitDef += unitDefTemplate.format(LSUnitDef).format(slot)
      else:
        BFUItinData += BFUItinDataTemplate.format(slot, blueUnitDef.format(slot))
        allBFUnitNames += [blueUnitDef.format(slot)]
        packetOrderUnitNames += [blueUnitDef.format(slot)]
        funcUnitDef += unitDefTemplate.format(blueUnitDef).format(slot)
    else:
      print("slot is neither GFU or BFU. Should never happen")
      exit(-1)

    if gfu:
      allInsertUnitNames += [insertUnitDef.format(slot)]
      packetOrderUnitNames += [insertUnitDef.format(slot)]
      funcUnitDef += unitDefTemplate.format(insertUnitDef).format(slot)
    funcUnitDef += "\n"

  if(VERBOSE):
    print(funcUnitDef)
    print(packetOrderUnitNames)

  pipeDefTemplate = """def {0}         : ProcResource<1>;\n"""
  packetOrderPipes = []
  PipeDefs = ""
  greenPipes = []
  bluePipes = []
  for i in packetOrderUnitNames:
    pipeName = i + "Pipe"
    if i in allGFUnitNames:
      greenPipes.append(pipeName)
    if i in allBFUnitNames:
      bluePipes.append(pipeName)
    packetOrderPipes.append(pipeName)
    PipeDefs += pipeDefTemplate.format(pipeName)

  if(VERBOSE):
    print(PipeDefs)


  ProcItinTemplate = """
  InsertUnit{0},
  GreenBlueUnit{0},
  ExtractUnit{0}b,
  ExtractUnit{0}a,"""

  PipeDefsTemplate = """
  def InsertPipe{0}        : ProcResource<1>;
  def GreenBluePipe{0}     : ProcResource<1>;
  def ExtractPipe{0}b      : ProcResource<1>;
  def ExtractPipe{0}a      : ProcResource<1>;"""

  PipeInstancesTemplate = """
  InsertPipe{0},
  GreenBluePipe{0},
  ExtractPipe{0}b,
  ExtractPipe{0}a,"""

  PrimateSchedPrimate = f"""//===- PrimateSchedPrimate.td - Primate Scheduling Defs ----*- tablegen -*-===//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===---------------------------------r------------------------------------===//

  {funcUnitDef}
  def BranchUnit : FuncUnit;

  def PrimateItinList {{
    list<InstrItinData> ItinList = [
      InstrItinData<ItinExtract,       [InstrStage<1, [{",".join(allExtractUnitNames)}]>]>,
      InstrItinData<ItinInsert,        [InstrStage<1, [{",".join(allInsertUnitNames)}]>]>,
      InstrItinData<ItinGreen,         [InstrStage<1, [{",".join(allGFUnitNames)}]>]>,
      {BFUItinData}
      InstrItinData<ItinIO,            [InstrStage<1, [{",".join(allIOUnitNames)}]>]>,
      InstrItinData<ItinBranch,        [InstrStage<1, [BranchUnit]>]>,
      InstrItinData<ItinMem,           [InstrStage<1, [{",".join(allLSUnitNames)}]>]>
    ];
  }}

  def PrimateItineraries :
      ProcessorItineraries<[  
          {",".join(packetOrderUnitNames)},
          BranchUnit
      ],
      [],
      PrimateItinList.ItinList>;

  // Primate machine model for scheduling
  def PrimateModel : SchedMachineModel {{
    let MicroOpBufferSize = 0;
    let IssueWidth = 7;        // 2 micro-ops are dispatched per cycle.
    let Itineraries = PrimateItineraries;
    let LoadLatency = 3;
    let MispredictPenalty = 0;
    let CompleteModel = 1;
    let UnsupportedFeatures = [
      HasFullI,
      HasStdExtM,
      HasStdExtA,
      HasStdExtF,
      HasStdExtD,
      HasStdExtZfh,
      HasStdExtC,
      HasStdExtZba,
      HasStdExtZbb,
      HasStdExtZbc,
      HasStdExtZbs,
      HasPRCHints
    ];
  }}

  let SchedModel = PrimateModel in {{
  let BufferSize = 0 in {{
  {PipeDefs}
  def BranchPipe         : ProcResource<1>;
  }}

  def PrimatePipes : ProcResGroup<[
    {",".join(packetOrderPipes)},
    BranchPipe
  ]>;
  def GreenPipes : ProcResGroup<[{",".join(greenPipes)}]>;
  def BluePipes : ProcResGroup<[{",".join(bluePipes)}]>;


  // Branching
  def : WriteRes<WriteJmp, [BranchPipe]>;
  def : WriteRes<WriteJal, [BranchPipe]>;
  def : WriteRes<WriteJalr, [BranchPipe]>;
  def : WriteRes<WriteJmpReg, [BranchPipe]>;

  // Integer arithmetic and logic
  let Latency = 3 in {{
  def : WriteRes<WriteIALU, [GreenPipes]>;
  def : WriteRes<WriteIALU32, [GreenPipes]>;
  def : WriteRes<WriteShiftImm, [GreenPipes]>;
  def : WriteRes<WriteShiftImm32, [GreenPipes]>;
  def : WriteRes<WriteShiftReg, [GreenPipes]>;
  def : WriteRes<WriteShiftReg32, [GreenPipes]>;
  }}

  // Integer multiplication
  let Latency = 3 in {{
  def : WriteRes<WriteIMul, [GreenPipes]>;
  def : WriteRes<WriteIMul32, [GreenPipes]>;
  }}

  // Integer division
  def : WriteRes<WriteIDiv, [GreenPipes]> {{
    let Latency = 16;
    let ReleaseAtCycles = [15];
  }}
  def : WriteRes<WriteIDiv32,  [GreenPipes]> {{
    let Latency = 16;
    let ReleaseAtCycles = [15];
  }}

  let Latency = 1000 in {{
  // Memory
  def : WriteRes<WriteSTB, [BluePipes]>;
  def : WriteRes<WriteSTH, [BluePipes]>;
  def : WriteRes<WriteSTW, [BluePipes]>;
  def : WriteRes<WriteSTD, [BluePipes]>;
  def : WriteRes<WriteFST32, [BluePipes]>;
  def : WriteRes<WriteFST64, [BluePipes]>;

  def : WriteRes<WriteLDB, [BluePipes]>;
  def : WriteRes<WriteLDH, [BluePipes]>;
  def : WriteRes<WriteLDW, [BluePipes]>;
  def : WriteRes<WriteLDWU, [BluePipes]>;
  def : WriteRes<WriteLDD, [BluePipes]>;

  def : WriteRes<WriteFLD32, [BluePipes]>;
  def : WriteRes<WriteFLD64, [BluePipes]>;

  // Atomic memory
  def : WriteRes<WriteAtomicSTW, [BluePipes]>;
  def : WriteRes<WriteAtomicSTD, [BluePipes]>;

  def : WriteRes<WriteAtomicW, [BluePipes]>;
  def : WriteRes<WriteAtomicD, [BluePipes]>;
  def : WriteRes<WriteAtomicLDW, [BluePipes]>;
  def : WriteRes<WriteAtomicLDD, [BluePipes]>;
  }}

  // Single precision.
  let Latency = 5 in {{
  def : WriteRes<WriteFALU32, [GreenPipes]>;
  def : WriteRes<WriteFMul32, [GreenPipes]>;
  def : WriteRes<WriteFMA32, [GreenPipes]>;
  }}
  let Latency = 3 in {{
  def : WriteRes<WriteFSGNJ32, [GreenPipes]>;
  def : WriteRes<WriteFMinMax32, [GreenPipes]>;
  }}

  def : WriteRes<WriteFDiv32, [GreenPipes]> {{
    let Latency = 27;
    let ReleaseAtCycles = [26];
  }}
  def : WriteRes<WriteFSqrt32, [GreenPipes]> {{
    let Latency = 27;
    let ReleaseAtCycles = [26];
  }}

  // Double precision
  let Latency = 7 in {{
  def : WriteRes<WriteFALU64, [GreenPipes]>;
  def : WriteRes<WriteFMul64, [GreenPipes]>;
  def : WriteRes<WriteFMA64, [GreenPipes]>;
  }}
  let Latency = 3 in {{
  def : WriteRes<WriteFSGNJ64, [GreenPipes]>;
  def : WriteRes<WriteFMinMax64, [GreenPipes]>;
  }}

  def : WriteRes<WriteFDiv64, [GreenPipes]> {{
    let Latency = 56;
    let ReleaseAtCycles = [55];
  }}
  def : WriteRes<WriteFSqrt64, [GreenPipes]> {{
    let Latency = 56;
    let ReleaseAtCycles = [55];
  }}

  // Conversions
  let Latency = 3 in {{
  def : WriteRes<WriteFCvtI32ToF32, [GreenPipes]>;
  def : WriteRes<WriteFCvtI32ToF64, [GreenPipes]>;
  def : WriteRes<WriteFCvtI64ToF32, [GreenPipes]>;
  def : WriteRes<WriteFCvtI64ToF64, [GreenPipes]>;
  def : WriteRes<WriteFCvtF32ToI32, [GreenPipes]>;
  def : WriteRes<WriteFCvtF32ToI64, [GreenPipes]>;
  def : WriteRes<WriteFCvtF32ToF64, [GreenPipes]>;
  def : WriteRes<WriteFCvtF64ToI32, [GreenPipes]>;
  def : WriteRes<WriteFCvtF64ToI64, [GreenPipes]>;
  def : WriteRes<WriteFCvtF64ToF32, [GreenPipes]>;

  def : WriteRes<WriteFClass32, [GreenPipes]>;
  def : WriteRes<WriteFClass64, [GreenPipes]>;
  def : WriteRes<WriteFCmp32, [GreenPipes]>;
  def : WriteRes<WriteFCmp64, [GreenPipes]>;
  def : WriteRes<WriteFMovI32ToF32, [GreenPipes]>;
  def : WriteRes<WriteFMovF32ToI32, [GreenPipes]>;
  def : WriteRes<WriteFMovI64ToF64, [GreenPipes]>;
  def : WriteRes<WriteFMovF64ToI64, [GreenPipes]>;
  }}

  // Others
  def : WriteRes<WriteCSR, [GreenPipes]>;
  def : WriteRes<WriteNop, []>;

  def : InstRW<[WriteIALU], (instrs COPY)>;

  // Bypass and advance
  def : ReadAdvance<ReadJmp, 0>;
  def : ReadAdvance<ReadJalr, 0>;
  def : ReadAdvance<ReadCSR, 0>;
  def : ReadAdvance<ReadStoreData, 0>;
  def : ReadAdvance<ReadMemBase, 0>;
  def : ReadAdvance<ReadIALU, 0>;
  def : ReadAdvance<ReadIALU32, 0>;
  def : ReadAdvance<ReadShiftImm, 0>;
  def : ReadAdvance<ReadShiftImm32, 0>;
  def : ReadAdvance<ReadShiftReg, 0>;
  def : ReadAdvance<ReadShiftReg32, 0>;
  def : ReadAdvance<ReadIDiv, 0>;
  def : ReadAdvance<ReadIDiv32, 0>;
  def : ReadAdvance<ReadIMul, 0>;
  def : ReadAdvance<ReadIMul32, 0>;
  def : ReadAdvance<ReadAtomicWA, 0>;
  def : ReadAdvance<ReadAtomicWD, 0>;
  def : ReadAdvance<ReadAtomicDA, 0>;
  def : ReadAdvance<ReadAtomicDD, 0>;
  def : ReadAdvance<ReadAtomicLDW, 0>;
  def : ReadAdvance<ReadAtomicLDD, 0>;
  def : ReadAdvance<ReadAtomicSTW, 0>;
  def : ReadAdvance<ReadAtomicSTD, 0>;
  def : ReadAdvance<ReadFMemBase, 0>;
  def : ReadAdvance<ReadFALU32, 0>;
  def : ReadAdvance<ReadFALU64, 0>;
  def : ReadAdvance<ReadFAdd64, 0>;
  def : ReadAdvance<ReadFAdd32, 0>;
  def : ReadAdvance<ReadFAdd16, 0>;
  def : ReadAdvance<ReadFMul32, 0>;
  def : ReadAdvance<ReadFMA32, 0>;
  def : ReadAdvance<ReadFMA32Addend, 0>;
  def : ReadAdvance<ReadFStoreData, 0>;
  def : ReadAdvance<ReadFMul64, 0>;
  def : ReadAdvance<ReadFMA64, 0>;
  def : ReadAdvance<ReadFMA64Addend, 0>;
  def : ReadAdvance<ReadFDiv32, 0>;
  def : ReadAdvance<ReadFDiv64, 0>;
  def : ReadAdvance<ReadFSqrt32, 0>;
  def : ReadAdvance<ReadFSqrt64, 0>;
  def : ReadAdvance<ReadFCmp32, 0>;
  def : ReadAdvance<ReadFCmp64, 0>;
  def : ReadAdvance<ReadFSGNJ32, 0>;
  def : ReadAdvance<ReadFSGNJ64, 0>;
  def : ReadAdvance<ReadFMinMax32, 0>;
  def : ReadAdvance<ReadFMinMax64, 0>;
  def : ReadAdvance<ReadFCvtF32ToI32, 0>;
  def : ReadAdvance<ReadFCvtF32ToI64, 0>;
  def : ReadAdvance<ReadFCvtF64ToI32, 0>;
  def : ReadAdvance<ReadFCvtF64ToI64, 0>;
  def : ReadAdvance<ReadFCvtI32ToF32, 0>;
  def : ReadAdvance<ReadFCvtI32ToF64, 0>;
  def : ReadAdvance<ReadFCvtI64ToF32, 0>;
  def : ReadAdvance<ReadFCvtI64ToF64, 0>;
  def : ReadAdvance<ReadFCvtF32ToF64, 0>;
  def : ReadAdvance<ReadFCvtF64ToF32, 0>;
  def : ReadAdvance<ReadFMovF32ToI32, 0>;
  def : ReadAdvance<ReadFMovI32ToF32, 0>;
  def : ReadAdvance<ReadFMovF64ToI64, 0>;
  def : ReadAdvance<ReadFMovI64ToF64, 0>;
  def : ReadAdvance<ReadFClass32, 0>;
  def : ReadAdvance<ReadFClass64, 0>;

  // Unsupported extensions
  defm : UnsupportedSchedV;
  defm : UnsupportedSchedZba;
  defm : UnsupportedSchedZbb;
  defm : UnsupportedSchedZfh;
  }}
  """

  if DRY_RUN:
    return

  with open(os.path.join(gen_file_dir, "./PrimateSchedPrimate.td"), "w") as f:
    print(PrimateSchedPrimate, file=f)

# write the schedule resources that are used in the schedule
# num_bfu is number of unique BFUs
def write_sched_resources_def(num_bfus: int):
  NewItinDefTemplate = "def ItinBlue{0}   : InstrItinClass;\n"

  NewItinDef = comb_str(NewItinDefTemplate, max(num_bfus-2, 1))

  PrimateSchedule = f"""
  //===- PrimateScheduleBFUs.td - Primate Scheduling Definitions -*- tablegen -*-===//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===----------------------------------------------------------------------===//

  {NewItinDef}
  """

  if DRY_RUN:
    return

  with open(os.path.join(gen_file_dir, "./PrimateScheduleBFU.td"), "w") as f:
      print(PrimateSchedule, file=f)

# writes the machine instructions AND their codegen patterns for each unique BFU 
def write_BFU_instr_info(num_bfus: int):
  BFUInstPatternTemplate = ("def : Pat<(int_primate_BFU_{0} WIDEREG:$rs1), (BFU{0} WIDEREG:$rs1)>;\n")

  BFUInstPattern = comb_str(BFUInstPatternTemplate, max(num_bfus-2, 1))

  BFUInstDefsTemplate = """let Itinerary = ItinBlue{0} in
  let hasSideEffects = 1, mayLoad = 1, mayStore = 1 in
  def BFU{0} :
      PRInstI<0b000, OPC_PR_ASCII, (outs WIDEREG:$rd), (ins WIDEREG:$rs1),
          "bfu{0}", "$rd, $rs1">, Sched<[WriteIALU, ReadIALU]> {{
            let IsBFUInstruction = 1;
            let imm12 = 0;
          }}
  """

  BFUInstDefs = comb_str(BFUInstDefsTemplate, max(num_bfus-2, 1))

  PrimateInstrInfo = f"""
  //===- PrimateInstrInfoBFU.td - Target Description for Primate *- tablegen -*-===//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===-------------------------------------------------------------------------===//
  //
  // This file describes the Primate BFU instructions in TableGen format.
  //
  //===-------------------------------------------------------------------------===//

  {BFUInstDefs}

  {BFUInstPattern}

  {BFUInstPattern}
  """

  if DRY_RUN:
    return

  with open(os.path.join(gen_file_dir, "./PrimateInstrInfoBFU.td"), "w") as f:
      print(PrimateInstrInfo, file=f)
    
# writes the bfu tablegen for number of UNIQUE bfus
def write_bfu_intrins(num_bfus: int): 
  PrimateIntrinsDefTemplate = """def int_primate_BFU_{0} :  Intrinsic<[llvm_any_ty], // return val
                    [llvm_any_ty], // Params: gpr w/ struct
                    [IntrHasSideEffects]>; // properties;
                  """
  PrimateIntrinsDef = comb_str(PrimateIntrinsDefTemplate, max(num_bfus-2, 1))
      
  IntrinsicsPrimate = f"""
  //===- IntrinsicsPrimateBFU.td - Defines Primate BFU intrinsics ---*- tablegen -*-===//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===-----------------------------------------------------------------------------===//
  // BFU Extension

  let TargetPrefix = "primate" in {{  
    {PrimateIntrinsDef}
  }} // TargetPrefix = "primate"
  """

  if DRY_RUN:
    return

  with open(os.path.join(gen_file_dir, "./IntrinsicsPrimateBFU.td"), "w") as f:
      print(IntrinsicsPrimate, file=f)

# generate builtin frontend shit
def write_bfu_clang_builtins(num_bfus: int): 
  # /primate/primate-compiler/clang/include/clang/Basic/BuiltinsPrimate.def
  builtin_single = """TARGET_BUILTIN(__primate_bfu_{0}, "v", "nt", "")"""
  BFU_BUILTINS = comb_str(builtin_single, max(num_bfus-2, 1))

  builtins_str = f"""
  //==- BuiltinsPrimate.def - Primate Builtin function database ----*- C++ -*-==//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===----------------------------------------------------------------------===//
  //
  // This file defines the Primate-specific builtin function database.  Users of
  // this file must define the BUILTIN macro to make use of this information.
  //
  //===----------------------------------------------------------------------===//

  #if defined(BUILTIN) && !defined(TARGET_BUILTIN)
  #   define TARGET_BUILTIN(ID, TYPE, ATTRS, FEATURE) BUILTIN(ID, TYPE, ATTRS)
  #endif

  //#include "clang/Basic/primate_vector_builtins.inc"

  // Primate BFUs
  TARGET_BUILTIN(__primate_input, "v*Ci", "nt", "")
  TARGET_BUILTIN(__primate_input_done, "v", "nt", "")
  TARGET_BUILTIN(__primate_output, "vv*Ci", "nt", "")
  TARGET_BUILTIN(__primate_output_done, "v", "nt", "")
  {BFU_BUILTINS}

  // Zbb extension
  TARGET_BUILTIN(__builtin_primate_orc_b_32, "ZiZi", "nc", "experimental-zbb")
  TARGET_BUILTIN(__builtin_primate_orc_b_64, "WiWi", "nc", "experimental-zbb,64bit")

  // Zbc extension
  TARGET_BUILTIN(__builtin_primate_clmul, "LiLiLi", "nc", "experimental-zbc")
  TARGET_BUILTIN(__builtin_primate_clmulh, "LiLiLi", "nc", "experimental-zbc")
  TARGET_BUILTIN(__builtin_primate_clmulr, "LiLiLi", "nc", "experimental-zbc")

  // Zbe extension
  TARGET_BUILTIN(__builtin_primate_bcompress_32, "ZiZiZi", "nc", "experimental-zbe")
  TARGET_BUILTIN(__builtin_primate_bcompress_64, "WiWiWi", "nc",
                "experimental-zbe,64bit")
  TARGET_BUILTIN(__builtin_primate_bdecompress_32, "ZiZiZi", "nc",
                "experimental-zbe")
  TARGET_BUILTIN(__builtin_primate_bdecompress_64, "WiWiWi", "nc",
                "experimental-zbe,64bit")

  // Zbp extension
  TARGET_BUILTIN(__builtin_primate_grev_32, "ZiZiZi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_grev_64, "WiWiWi", "nc", "experimental-zbp,64bit")
  TARGET_BUILTIN(__builtin_primate_gorc_32, "ZiZiZi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_gorc_64, "WiWiWi", "nc", "experimental-zbp,64bit")
  TARGET_BUILTIN(__builtin_primate_shfl_32, "ZiZiZi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_shfl_64, "WiWiWi", "nc", "experimental-zbp,64bit")
  TARGET_BUILTIN(__builtin_primate_unshfl_32, "ZiZiZi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_unshfl_64, "WiWiWi", "nc", "experimental-zbp,64bit")
  TARGET_BUILTIN(__builtin_primate_xperm_n, "LiLiLi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_xperm_b, "LiLiLi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_xperm_h, "LiLiLi", "nc", "experimental-zbp")
  TARGET_BUILTIN(__builtin_primate_xperm_w, "WiWiWi", "nc", "experimental-zbp,64bit")

  // Zbr extension
  TARGET_BUILTIN(__builtin_primate_crc32_b, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32_h, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32_w, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32c_b, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32c_h, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32c_w, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32_d, "LiLi", "nc", "experimental-zbr")
  TARGET_BUILTIN(__builtin_primate_crc32c_d, "LiLi", "nc", "experimental-zbr")

  #undef BUILTIN
  #undef TARGET_BUILTIN
  """

  if DRY_RUN:
    return

  with open(os.path.join(gen_file_dir, "./BuiltinsPrimate.def"), "w") as f:
      print(builtins_str, file=f)

# front end language tablegen 
def write_bfu_CG(num_bfus: int):
  # /primate/primate-compiler/clang/include/clang/Basic/primate_bfu.td

  BFU_BUILTINS_TEMPS = """def BFU_{0}:      PrimateBuiltin<"__primate_BFU_{0}", "BB", "primate_BFU_{0}">;"""
  BFU_BUILTINS = comb_str(BFU_BUILTINS_TEMPS, max(num_bfus-2, 1))

  front_end_stuff_template = f"""
  // name is the builtin name from clang/include/clang/Basic/BuiltinsPrimate.def
  //
  // prototype gets one char per argument
  // example: BBi is a built in that returns BFU, and takes BFU, integer
  // supported types:
  //      B: BFU type
  //      v: void
  //      i: integers
  //
  // intrin_name is the name of the backend intrinsic to use
  // defined in llvm/include/llvm/IR/IntrinsicsPrimate.td (strip the int_ from the tablegen name)


  class PrimateBuiltin<string name, string prototype, string intrin_name> {{
      string Name = name;
      string PType = prototype;
      string IntrinName = intrin_name;
  }}


  def input:      PrimateBuiltin<"__primate_input", "B", "primate_input">;
  def inputDone:  PrimateBuiltin<"__primate_input_done", "", "primate_input_done">;
  def output:     PrimateBuiltin<"__primate_output", "B", "primate_output">;
  def outputDone: PrimateBuiltin<"__primate_output_done", "", "primate_output_done">;
  {BFU_BUILTINS}
  """

  if DRY_RUN:
    return

  with open(os.path.join(gen_file_dir, "primate_bfu.td"), "w") as f:
      print(front_end_stuff_template, file=f)

def main():
  global gen_file_dir 
  global DRY_RUN
  global VERBOSE
  args = parser.parse_args()
  gen_file_dir = args.output
  DRY_RUN = args.dry_run
  VERBOSE = args.verbose
  os.makedirs(gen_file_dir, exist_ok=True)
  num_unique_bfus = parse_BFU_list(args.bfu_list) + 2 # IO and LSUs are hidden

  write_bfu_intrins(num_unique_bfus)
  write_bfu_clang_builtins(num_unique_bfus)
  write_bfu_CG(num_unique_bfus) 
  write_BFU_instr_info(num_unique_bfus)
  write_sched_resources_def(num_unique_bfus)

  if not args.FrontendOnly:
    numALUs, numBFUs = parse_arch_config(args.primate_cfg)
    write_schedule(numBFUs, numALUs)

if __name__ == "__main__":
  main()
