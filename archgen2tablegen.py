#!/bin/python3

import re
import os
import sys

if len(sys.argv) != 3: 
    print("wrong number of arguments....")
    print("Expected: " + sys.argv[0] + " <Path to BFU_list.txt> <Path to primate.cfg>")
    exit(-1)
    
gen_file_dir = "./primate-compiler-gen/"
os.makedirs(gen_file_dir, exist_ok=True)

def combStr(in_str, num_iter):
    return "".join([in_str.format(i) for i in range(num_iter)])


with open(sys.argv[1]) as f:
    f_str = f.read()
    numBFUs = len(re.findall(r"(.+\n?)\{([^\}]*\n?)+\}", f_str))

# add the slots for IO unit and LSU
numBFUs += 2

with open(sys.argv[2]) as f:
    for line in f:
        toks = line.split("=")
        if toks[0] == "NUM_ALUS":
            numALUs = int(toks[1])

numSlots = max(numBFUs, numALUs)

# BFUs and ALUs are merged starting with the last BFU slot. 
if numALUs >= numBFUs:
  hasGFU = [True] * numSlots
  hasBFU = [True] * (numBFUs) + [False] * (numSlots-numBFUs)
else:
  hasGFU = [True] * (numALUs) + [False] * (numBFUs - numALUs)
  hasBFU = [True] * numSlots

IOSlot = numBFUs - 1
LSUSlot = numBFUs - 2

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

with open(os.path.join(gen_file_dir, "./PrimateSchedPrimate.td"), "w") as f:
  print(PrimateSchedPrimate, file=f)

NewItinDefTemplate = "def ItinBlue{0}   : InstrItinClass;\n"

NewItinDef = combStr(NewItinDefTemplate, max(numBFUs-2, 1))

PrimateSchedule = f"""
//===- PrimateSchedule.td - Primate Scheduling Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

def ItinExtract : InstrItinClass;
def ItinInsert  : InstrItinClass;
def ItinGreen   : InstrItinClass;
{NewItinDef}
def ItinMem     : InstrItinClass;
def ItinIO      : InstrItinClass;
def ItinBranch  : InstrItinClass;

/// Define scheduler resources associated with def operands.
def WriteIALU       : SchedWrite;    // 32 or 64-bit integer ALU operations
def WriteIALU32     : SchedWrite;    // 32-bit integer ALU operations on PR64I
def WriteShiftImm   : SchedWrite;    // 32 or 64-bit shift by immediate operations
def WriteShiftImm32 : SchedWrite;    // 32-bit shift by immediate operations on PR64Ix
def WriteShiftReg   : SchedWrite;    // 32 or 64-bit shift by immediate operations
def WriteShiftReg32 : SchedWrite;    // 32-bit shift by immediate operations on PR64Ix
def WriteIDiv       : SchedWrite;    // 32-bit or 64-bit divide and remainder
def WriteIDiv32     : SchedWrite;    // 32-bit divide and remainder on PR64I
def WriteIMul       : SchedWrite;    // 32-bit or 64-bit multiply
def WriteIMul32     : SchedWrite;    // 32-bit multiply on PR64I
def WriteJmp        : SchedWrite;    // Jump
def WriteJal        : SchedWrite;    // Jump and link
def WriteJalr       : SchedWrite;    // Jump and link register
def WriteJmpReg     : SchedWrite;    // Jump register
def WriteNop        : SchedWrite;
def WriteLDB        : SchedWrite;    // Load byte
def WriteLDH        : SchedWrite;    // Load half-word
def WriteLDW        : SchedWrite;    // Load word
def WriteLDWU       : SchedWrite;    // Load word unsigned
def WriteLDD        : SchedWrite;    // Load double-word
def WriteCSR        : SchedWrite;    // CSR instructions
def WriteSTB        : SchedWrite;    // Store byte
def WriteSTH        : SchedWrite;    // Store half-word
def WriteSTW        : SchedWrite;    // Store word
def WriteSTD        : SchedWrite;    // Store double-word
def WriteAtomicW    : SchedWrite;    //Atomic memory operation word size
def WriteAtomicD    : SchedWrite;    //Atomic memory operation double word size
def WriteAtomicLDW  : SchedWrite;    // Atomic load word
def WriteAtomicLDD  : SchedWrite;    // Atomic load double word
def WriteAtomicSTW  : SchedWrite;    // Atomic store word
def WriteAtomicSTD  : SchedWrite;    // Atomic store double word
def WriteFALU16     : SchedWrite;    // FP 16-bit computation
def WriteFALU32     : SchedWrite;    // FP 32-bit computation
def WriteFALU64     : SchedWrite;    // FP 64-bit computation
def WriteFMul16     : SchedWrite;    // 16-bit floating point multiply
def WriteFMA16      : SchedWrite;    // 16-bit floating point fused multiply-add
def WriteFMA16Addend: SchedWrite;    // 16-bit floating point fused multiply-add
def WriteFMul32     : SchedWrite;    // 32-bit floating point multiply
def WriteFMA32      : SchedWrite;    // 32-bit floating point fused multiply-add
def WriteFMul64     : SchedWrite;    // 64-bit floating point multiply
def WriteFMA64      : SchedWrite;    // 64-bit floating point fused multiply-add
def WriteFMA64Addend: SchedWrite;
def WriteFDiv16     : SchedWrite;    // 16-bit floating point divide
def WriteFDiv32     : SchedWrite;    // 32-bit floating point divide
def WriteFDiv64     : SchedWrite;    // 64-bit floating point divide
def WriteFSqrt16    : SchedWrite;    // 16-bit floating point sqrt
def WriteFSqrt32    : SchedWrite;    // 32-bit floating point sqrt
def WriteFSqrt64    : SchedWrite;    // 64-bit floating point sqrt
def WriteFAdd16     : SchedWrite;
def WriteFAdd32     : SchedWrite; 
def WriteFAdd64     : SchedWrite; 

// Integer to float conversions
def WriteFCvtI32ToF16  : SchedWrite;
def WriteFCvtI32ToF32  : SchedWrite;
def WriteFCvtI32ToF64  : SchedWrite;
def WriteFCvtI64ToF16  : SchedWrite;    // PR64I only
def WriteFCvtI64ToF32  : SchedWrite;    // PR64I only
def WriteFCvtI64ToF64  : SchedWrite;    // PR64I only

//Float to integer conversions
def WriteFCvtF16ToI32  : SchedWrite;
def WriteFCvtF16ToI64  : SchedWrite;    // PR64I only
def WriteFCvtF32ToI32  : SchedWrite;
def WriteFCvtF32ToI64  : SchedWrite;    // PR64I only
def WriteFCvtF64ToI32  : SchedWrite;
def WriteFCvtF64ToI64  : SchedWrite;    // PR64I only

// Float to float conversions
def WriteFCvtF32ToF64  : SchedWrite;
def WriteFCvtF64ToF32  : SchedWrite;
def WriteFCvtF16ToF32  : SchedWrite;
def WriteFCvtF32ToF16  : SchedWrite;
def WriteFCvtF16ToF64  : SchedWrite;
def WriteFCvtF64ToF16  : SchedWrite;

def WriteFClass16   : SchedWrite;    // 16-bit floating point classify
def WriteFClass32   : SchedWrite;    // 32-bit floating point classify
def WriteFClass64   : SchedWrite;    // 64-bit floating point classify
def WriteFCmp16     : SchedWrite;    // 16-bit floating point compare
def WriteFCmp32     : SchedWrite;    // 32-bit floating point compare
def WriteFCmp64     : SchedWrite;    // 64-bit floating point compare
def WriteFSGNJ16    : SchedWrite;    // 16-bit floating point sign-injection
def WriteFSGNJ32    : SchedWrite;    // 32-bit floating point sign-injection
def WriteFSGNJ64    : SchedWrite;    // 64-bit floating point sign-injection
def WriteFMinMax16  : SchedWrite;    // 16-bit floating point min or max
def WriteFMinMax32  : SchedWrite;    // 32-bit floating point min or max
def WriteFMinMax64  : SchedWrite;    // 64-bit floating point min or max

def WriteFMovF16ToI16     : SchedWrite;
def WriteFMovI16ToF16     : SchedWrite;
def WriteFMovF32ToI32     : SchedWrite;
def WriteFMovI32ToF32     : SchedWrite;
def WriteFMovF64ToI64     : SchedWrite;    // PR64I only
def WriteFMovI64ToF64     : SchedWrite;    // PR64I only

def WriteFLD16        : SchedWrite;    // Floating point sp load
def WriteFLD32        : SchedWrite;    // Floating point sp load
def WriteFLD64        : SchedWrite;    // Floating point dp load
def WriteFST16        : SchedWrite;    // Floating point sp store
def WriteFST32        : SchedWrite;    // Floating point sp store
def WriteFST64        : SchedWrite;    // Floating point dp store

/// Define scheduler resources associated with use operands.
def ReadJmp         : SchedRead;
def ReadJalr        : SchedRead;
def ReadCSR         : SchedRead;
def ReadMemBase     : SchedRead;
def ReadFMemBase    : SchedRead;
def ReadStoreData   : SchedRead;
def ReadFStoreData  : SchedRead;
def ReadFMA32Addend : SchedRead;
def ReadFMA32       : SchedRead;
def ReadIALU        : SchedRead;
def ReadIALU32      : SchedRead;    // 32-bit integer ALU operations on PR64I
def ReadShiftImm    : SchedRead;
def ReadShiftImm32  : SchedRead;    // 32-bit shift by immediate operations on PR64Ix
def ReadShiftReg    : SchedRead;
def ReadShiftReg32  : SchedRead;    // 32-bit shift by register operations on PR64Ix
def ReadIDiv        : SchedRead;
def ReadIDiv32      : SchedRead;
def ReadIMul        : SchedRead;
def ReadIMul32      : SchedRead;
def ReadAtomicWA    : SchedRead;
def ReadAtomicWD    : SchedRead;
def ReadAtomicDA    : SchedRead;
def ReadAtomicDD    : SchedRead;
def ReadAtomicLDW   : SchedRead;    // Atomic load word
def ReadAtomicLDD   : SchedRead;    // Atomic load double word
def ReadAtomicSTW   : SchedRead;    // Atomic store word
def ReadAtomicSTD   : SchedRead;    // Atomic store double word
def ReadFALU16      : SchedRead;    // FP 16-bit computation
def ReadFALU32      : SchedRead;    // FP 32-bit computation
def ReadFALU64      : SchedRead;    // FP 64-bit computation
def ReadFMul16      : SchedRead;    // 16-bit floating point multiply
def ReadFMA16       : SchedRead;    // 16-bit floating point fused multiply-add
def ReadFMA16Addend : SchedRead;    // 16-bit floating point fused multiply-add
def ReadFMul32      : SchedRead;    // 32-bit floating point multiply
def ReadFMA64Addend : SchedRead;
def ReadFMul64      : SchedRead;    // 64-bit floating point multiply
def ReadFMA64       : SchedRead;    // 64-bit floating point fused multiply-add
def ReadFDiv16      : SchedRead;    // 16-bit floating point divide
def ReadFDiv32      : SchedRead;    // 32-bit floating point divide
def ReadFDiv64      : SchedRead;    // 64-bit floating point divide
def ReadFSqrt16     : SchedRead;    // 16-bit floating point sqrt
def ReadFSqrt32     : SchedRead;    // 32-bit floating point sqrt
def ReadFSqrt64     : SchedRead;    // 64-bit floating point sqrt
def ReadFCmp16      : SchedRead;
def ReadFCmp32      : SchedRead;
def ReadFCmp64      : SchedRead;
def ReadFSGNJ16     : SchedRead;
def ReadFSGNJ32     : SchedRead;
def ReadFSGNJ64     : SchedRead;
def ReadFMinMax16   : SchedRead;
def ReadFMinMax32   : SchedRead;
def ReadFMinMax64   : SchedRead;
def ReadFCvtF16ToI32     : SchedRead;
def ReadFCvtF16ToI64     : SchedRead;
def ReadFCvtF32ToI32     : SchedRead;
def ReadFCvtF32ToI64     : SchedRead;
def ReadFCvtF64ToI32     : SchedRead;
def ReadFCvtF64ToI64     : SchedRead;
def ReadFCvtI32ToF16     : SchedRead;
def ReadFCvtI32ToF32     : SchedRead;
def ReadFCvtI32ToF64     : SchedRead;
def ReadFCvtI64ToF16     : SchedRead;
def ReadFCvtI64ToF32     : SchedRead;
def ReadFCvtI64ToF64     : SchedRead;
def ReadFMovF16ToI16     : SchedRead;
def ReadFMovI16ToF16     : SchedRead;
def ReadFMovF32ToI32     : SchedRead;
def ReadFMovI32ToF32     : SchedRead;
def ReadFMovF64ToI64     : SchedRead;
def ReadFMovI64ToF64     : SchedRead;
def ReadFCvtF32ToF64     : SchedRead;
def ReadFCvtF64ToF32     : SchedRead;
def ReadFCvtF16ToF32     : SchedRead;
def ReadFCvtF32ToF16     : SchedRead;
def ReadFCvtF16ToF64     : SchedRead;
def ReadFCvtF64ToF16     : SchedRead;
def ReadFClass16         : SchedRead;
def ReadFClass32         : SchedRead;
def ReadFClass64         : SchedRead;
def ReadFAdd16           : SchedRead;
def ReadFAdd32           : SchedRead;
def ReadFAdd64           : SchedRead;

multiclass UnsupportedSchedZfh {{
let Unsupported = true in {{
def : WriteRes<WriteFALU16, []>;
def : WriteRes<WriteFClass16, []>;
def : WriteRes<WriteFCvtF16ToF64, []>;
def : WriteRes<WriteFCvtF64ToF16, []>;
def : WriteRes<WriteFCvtI64ToF16, []>;
def : WriteRes<WriteFCvtF32ToF16, []>;
def : WriteRes<WriteFCvtI32ToF16, []>;
def : WriteRes<WriteFCvtF16ToI64, []>;
def : WriteRes<WriteFCvtF16ToF32, []>;
def : WriteRes<WriteFCvtF16ToI32, []>;
def : WriteRes<WriteFDiv16, []>;
def : WriteRes<WriteFCmp16, []>;
def : WriteRes<WriteFLD16, []>;
def : WriteRes<WriteFMA16, []>;
def : WriteRes<WriteFMinMax16, []>;
def : WriteRes<WriteFMul16, []>;
def : WriteRes<WriteFMovI16ToF16, []>;
def : WriteRes<WriteFMovF16ToI16, []>;
def : WriteRes<WriteFSGNJ16, []>;
def : WriteRes<WriteFST16, []>;
def : WriteRes<WriteFSqrt16, []>;
def : WriteRes<WriteFAdd64, []>;
def : WriteRes<WriteFAdd32, []>;
def : WriteRes<WriteFAdd16, []>;

def : ReadAdvance<ReadFALU16, 0>;
def : ReadAdvance<ReadFClass16, 0>;
def : ReadAdvance<ReadFCvtF16ToF64, 0>;
def : ReadAdvance<ReadFCvtF64ToF16, 0>;
def : ReadAdvance<ReadFCvtI64ToF16, 0>;
def : ReadAdvance<ReadFCvtF32ToF16, 0>;
def : ReadAdvance<ReadFCvtI32ToF16, 0>;
def : ReadAdvance<ReadFCvtF16ToI64, 0>;
def : ReadAdvance<ReadFCvtF16ToF32, 0>;
def : ReadAdvance<ReadFCvtF16ToI32, 0>;
def : ReadAdvance<ReadFDiv16, 0>;
def : ReadAdvance<ReadFCmp16, 0>;
def : ReadAdvance<ReadFMA16, 0>;
def : ReadAdvance<ReadFMinMax16, 0>;
def : ReadAdvance<ReadFMul16, 0>;
def : ReadAdvance<ReadFMovI16ToF16, 0>;
def : ReadAdvance<ReadFMovF16ToI16, 0>;
def : ReadAdvance<ReadFSGNJ16, 0>;
def : ReadAdvance<ReadFSqrt16, 0>;
}} // Unsupported = true
}}

// Include the scheduler resources for other instruction extensions.
include "PrimateScheduleB.td"
include "PrimateScheduleV.td"
"""

with open(os.path.join(gen_file_dir, "./PrimateSchedule.td"), "w") as f:
    print(PrimateSchedule, file=f)

BFUInstPatternTemplate = ("def : Pat<(int_primate_BFU_{0} WIDEREG:$rs1), (BFU{0} WIDEREG:$rs1)>;\n")

BFUInstPattern = combStr(BFUInstPatternTemplate, max(numBFUs-2, 1))

BFUInstDefsTemplate = """let Itinerary = ItinBlue{0} in
let hasSideEffects = 1, mayLoad = 1, mayStore = 1 in
def BFU{0} :
    PRInstI<0b000, OPC_PR_ASCII, (outs WIDEREG:$rd), (ins WIDEREG:$rs1),
        "bfu{0}", "$rd, $rs1">, Sched<[WriteIALU, ReadIALU]> {{
          let IsBFUInstruction = 1;
          let imm12 = 0;
        }}
"""

BFUInstDefs = combStr(BFUInstDefsTemplate, max(numBFUs-2, 1))

PrimateInstrInfo = f"""
//===- PrimateInstrInfo.td - Target Description for Primate *- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Primate instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Primate specific DAG Nodes.
//===----------------------------------------------------------------------===//

// Target-independent type requirements, but with target-specific formats.
def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                       SDTCisVT<1, i32>]>;
def SDT_CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                     SDTCisVT<1, i32>]>;

// Target-dependent type requirements.
def SDT_PrimateCall     : SDTypeProfile<0, -1, [SDTCisVT<0, XLenVT>]>;
def SDT_PrimateSelectCC : SDTypeProfile<1, 5, [SDTCisSameAs<1, 2>,
                                             SDTCisSameAs<0, 4>,
                                             SDTCisSameAs<4, 5>]>;
def SDT_PrimateBrCC : SDTypeProfile<0, 4, [SDTCisSameAs<0, 1>,
                                         SDTCisVT<2, OtherVT>,
                                         SDTCisVT<3, OtherVT>]>;
def SDT_PrimateReadCSR  : SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisInt<1>]>;
def SDT_PrimateWriteCSR : SDTypeProfile<0, 2, [SDTCisInt<0>, SDTCisInt<1>]>;
def SDT_PrimateSwapCSR  : SDTypeProfile<1, 2, [SDTCisInt<0>, SDTCisInt<1>,
                                             SDTCisInt<2>]>;

def SDT_PrimateExtract  : SDTypeProfile<1, 2, [SDTCisInt<0>, SDTCisInt<1>,
                                             SDTCisInt<2>]>;
                                             
def SDT_PrimateReadCycleWide : SDTypeProfile<2, 0, [SDTCisVT<0, i32>,
                                                  SDTCisVT<1, i32>]>;
def SDT_PrimateIntUnaryOpW : SDTypeProfile<1, 1, [
  SDTCisSameAs<0, 1>, SDTCisVT<0, i64>
]>;
def SDT_PrimateIntBinOpW : SDTypeProfile<1, 2, [
  SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisVT<0, i64>
]>;
def SDT_PrimateIntShiftDOpW : SDTypeProfile<1, 3, [
  SDTCisSameAs<0, 1>, SDTCisSameAs<0, 2>, SDTCisVT<0, i64>, SDTCisVT<3, i64>
]>;

// Target-independent nodes, but with target-specific formats.
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_CallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

// Target-dependent nodes.
def primate_call      : SDNode<"PrimateISD::CALL", SDT_PrimateCall,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;
def primate_ret_flag  : SDNode<"PrimateISD::RET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def primate_uret_flag : SDNode<"PrimateISD::URET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue]>;
def primate_sret_flag : SDNode<"PrimateISD::SRET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue]>;
def primate_mret_flag : SDNode<"PrimateISD::MRET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue]>;
def primate_selectcc  : SDNode<"PrimateISD::SELECT_CC", SDT_PrimateSelectCC>;
def primate_brcc      : SDNode<"PrimateISD::BR_CC", SDT_PrimateBrCC,
                             [SDNPHasChain]>;
def primate_tail      : SDNode<"PrimateISD::TAIL", SDT_PrimateCall,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;
def primate_sllw      : SDNode<"PrimateISD::SLLW", SDT_PrimateIntBinOpW>;
def primate_sraw      : SDNode<"PrimateISD::SRAW", SDT_PrimateIntBinOpW>;
def primate_srlw      : SDNode<"PrimateISD::SRLW", SDT_PrimateIntBinOpW>;
def primate_read_csr  : SDNode<"PrimateISD::READ_CSR", SDT_PrimateReadCSR,
                             [SDNPHasChain]>;
def primate_write_csr : SDNode<"PrimateISD::WRITE_CSR", SDT_PrimateWriteCSR,
                             [SDNPHasChain]>;
def primate_swap_csr  : SDNode<"PrimateISD::SWAP_CSR", SDT_PrimateSwapCSR,
                             [SDNPHasChain]>;

def primate_extract   : SDNode<"PrimateISD::EXTRACT", SDT_PrimateExtract, 
                                []>;

def primate_read_cycle_wide : SDNode<"PrimateISD::READ_CYCLE_WIDE",
                                   SDT_PrimateReadCycleWide,
                                   [SDNPHasChain, SDNPSideEffect]>;

def primate_add_lo : SDNode<"PrimateISD::ADD_LO", SDTIntBinOp>;
def primate_hi : SDNode<"PrimateISD::HI", SDTIntUnaryOp>;
def primate_lla : SDNode<"PrimateISD::LLA", SDTIntUnaryOp>;
def primate_add_tprel : SDNode<"PrimateISD::ADD_TPREL",
                             SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                  SDTCisSameAs<0, 2>,
                                                  SDTCisSameAs<0, 3>,
                                                  SDTCisInt<0>]>>;


//===----------------------------------------------------------------------===//
// Operand and SDNode transformation definitions.
//===----------------------------------------------------------------------===//

class ImmXLenAsmOperand<string prefix, string suffix = ""> : AsmOperandClass {{
  let Name = prefix # "ImmXLen" # suffix;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", Name);
}}

class ImmAsmOperand<string prefix, int width, string suffix> : AsmOperandClass {{
  let Name = prefix # "Imm" # width # suffix;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", Name);
}}

def ImmZeroAsmOperand : AsmOperandClass {{
  let Name = "ImmZero";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", Name);
}}

// A parse method for (${{gpr}}) or 0(${{gpr}}), where the 0 is be silently ignored.
def ZeroOffsetMemOpOperand : AsmOperandClass {{
  let Name = "ZeroOffsetMemOpOperand";
  let RenderMethod = "addRegOperands";
  let PredicateMethod = "isGPR";
  let ParserMethod = "parseZeroOffsetMemOp";
}}

class MemOperand<RegisterClass regClass> : RegisterOperand<regClass>{{
  let OperandType = "OPERAND_MEMORY";
}}

def GPRMemZeroOffset : MemOperand<GPR> {{
  let ParserMatchClass = ZeroOffsetMemOpOperand;
  let PrintMethod = "printZeroOffsetMemOp";
}}

def GPRMem : MemOperand<GPR>;

def SPMem : MemOperand<SP>;

def GPRCMem : MemOperand<GPRC>;

class SImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"S", width, suffix> {{
}}

class UImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"U", width, suffix> {{
}}

class PrimateOp<ValueType vt = XLenVT> : Operand<vt> {{
  let OperandNamespace = "PrimateOp";
}}

class PrimateUImmOp<int bitsNum> : PrimateOp {{
  let ParserMatchClass = UImmAsmOperand<bitsNum>;
  let DecoderMethod = "decodeUImmOperand<" # bitsNum # ">";
  let OperandType = "OPERAND_UIMM" # bitsNum;
}}

class PrimateUImmLeafOp<int bitsNum> :
  PrimateUImmOp<bitsNum>, ImmLeaf<XLenVT, "return isUInt<" # bitsNum # ">(Imm);">;

class PrimateSImmOp<int bitsNum> : PrimateOp {{
  let ParserMatchClass = SImmAsmOperand<bitsNum>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<" # bitsNum # ">";
  let OperandType = "OPERAND_SIMM" # bitsNum;
}}

class PrimateSImmLeafOp<int bitsNum> :
  PrimateSImmOp<bitsNum>, ImmLeaf<XLenVT, "return isInt<" # bitsNum # ">(Imm);">;

def FenceArg : AsmOperandClass {{
  let Name = "FenceArg";
  let RenderMethod = "addFenceArgOperands";
  let DiagnosticType = "InvalidFenceArg";
}}

def fencearg : Operand<XLenVT> {{
  let ParserMatchClass = FenceArg;
  let PrintMethod = "printFenceArg";
  let DecoderMethod = "decodeUImmOperand<4>";
  let OperandType = "OPERAND_UIMM4";
  let OperandNamespace = "PrimateOp";
}}

def UImmLog2XLenAsmOperand : AsmOperandClass {{
  let Name = "UImmLog2XLen";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImmLog2XLen";
}}

def uimmlog2xlen : Operand<XLenVT>, ImmLeaf<XLenVT, [{{
  if (Subtarget->is64Bit())
    return isUInt<6>(Imm);
  return isUInt<5>(Imm);
}}]> {{
  let ParserMatchClass = UImmLog2XLenAsmOperand;
  // TODO: should ensure invalid shamt is rejected when decoding.
  let DecoderMethod = "decodeUImmOperand<6>";
  let MCOperandPredicate = [{{
    int64_t Imm;
    if (!MCOp.evaluateAsConstantImm(Imm))
      return false;
    if (STI.getTargetTriple().isArch64Bit())
      return isUInt<6>(Imm);
    return isUInt<5>(Imm);
  }}];
  let OperandType = "OPERAND_UIMMLOG2XLEN";
  let OperandNamespace = "PrimateOp";
}}

def uimm2 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isUInt<2>(Imm);}}]> {{
  let ParserMatchClass = UImmAsmOperand<2>;
  let DecoderMethod = "decodeUImmOperand<2>";
  let OperandType = "OPERAND_UIMM2";
  let OperandNamespace = "PrimateOp";
}}

def uimm3 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isUInt<3>(Imm);}}]> {{
  let ParserMatchClass = UImmAsmOperand<3>;
  let DecoderMethod = "decodeUImmOperand<3>";
  let OperandType = "OPERAND_UIMM3";
  let OperandNamespace = "PrimateOp";
}}

def uimm4 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isUInt<4>(Imm);}}]> {{
  let ParserMatchClass = UImmAsmOperand<4>;
  let DecoderMethod = "decodeUImmOperand<4>";
  let OperandType = "OPERAND_UIMM4";
  let OperandNamespace = "PrimateOp";
}}

def uimm5 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isUInt<5>(Imm);}}]> {{
  let ParserMatchClass = UImmAsmOperand<5>;
  let DecoderMethod = "decodeUImmOperand<5>";
  let OperandType = "OPERAND_UIMM5";
  let OperandNamespace = "PrimateOp";
}}

def uimm6 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isUInt<6>(Imm);}}]> {{
  let ParserMatchClass = UImmAsmOperand<6>;
  let DecoderMethod = "decodeUImmOperand<5>";
  let OperandType = "OPERAND_UIMM6";
  let OperandNamespace = "PrimateOp";
}}

def uimm8 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isUInt<8>(Imm);}}]> {{
  let ParserMatchClass = UImmAsmOperand<8>;
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_UIMM8";
  let OperandNamespace = "PrimateOp";
}}

def simm12 : Operand<XLenVT>, ImmLeaf<XLenVT, [{{return isInt<12>(Imm);}}]> {{
  let ParserMatchClass = SImmAsmOperand<12>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<12>";
  let MCOperandPredicate = [{{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<12>(Imm);
    return MCOp.isBareSymbolRef();
  }}];
  let OperandType = "OPERAND_SIMM12";
  let OperandNamespace = "PrimateOp";
}}

// A 13-bit signed immediate where the least significant bit is zero.
def simm13_lsb0 : Operand<OtherVT> {{
  let ParserMatchClass = SImmAsmOperand<13, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<13>";
  let MCOperandPredicate = [{{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<12, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }}];
  let OperandType = "OPERAND_PCREL";
}}

class UImm20Operand : Operand<XLenVT> {{
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<20>";
  let MCOperandPredicate = [{{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<20>(Imm);
    return MCOp.isBareSymbolRef();
  }}];
  let OperandType = "OPERAND_UIMM20";
  let OperandNamespace = "PrimateOp";
}}

def uimm20_lui : UImm20Operand {{
  let ParserMatchClass = UImmAsmOperand<20, "LUI">;
}}
def uimm20_auipc : UImm20Operand {{
  let ParserMatchClass = UImmAsmOperand<20, "AUIPC">;
}}

def Simm21Lsb0JALAsmOperand : SImmAsmOperand<21, "Lsb0JAL"> {{
  let ParserMethod = "parseJALOffset";
}}

// A 21-bit signed immediate where the least significant bit is zero.
def simm21_lsb0_jal : Operand<OtherVT> {{
  let ParserMatchClass = Simm21Lsb0JALAsmOperand;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<21>";
  let MCOperandPredicate = [{{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }}];
  let OperandType = "OPERAND_PCREL";
}}

def BareSymbol : AsmOperandClass {{
  let Name = "BareSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidBareSymbol";
  let ParserMethod = "parseBareSymbol";
}}

// A bare symbol.
def bare_symbol : Operand<XLenVT> {{
  let ParserMatchClass = BareSymbol;
}}

def CallSymbol : AsmOperandClass {{
  let Name = "CallSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidCallSymbol";
  let ParserMethod = "parseCallSymbol";
}}

// A bare symbol used in call/tail only.
def call_symbol : Operand<XLenVT> {{
  let ParserMatchClass = CallSymbol;
}}

def PseudoJumpSymbol : AsmOperandClass {{
  let Name = "PseudoJumpSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidPseudoJumpSymbol";
  let ParserMethod = "parsePseudoJumpSymbol";
}}

// A bare symbol used for pseudo jumps only.
def pseudo_jump_symbol : Operand<XLenVT> {{
  let ParserMatchClass = PseudoJumpSymbol;
}}

def TPRelAddSymbol : AsmOperandClass {{
  let Name = "TPRelAddSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidTPRelAddSymbol";
  let ParserMethod = "parseOperandWithModifier";
}}

// A bare symbol with the %tprel_add variant.
def tprel_add_symbol : Operand<XLenVT> {{
  let ParserMatchClass = TPRelAddSymbol;
}}

def CSRSystemRegister : AsmOperandClass {{
  let Name = "CSRSystemRegister";
  let ParserMethod = "parseCSRSystemRegister";
  let DiagnosticType = "InvalidCSRSystemRegister";
}}

def csr_sysreg : Operand<XLenVT> {{
  let ParserMatchClass = CSRSystemRegister;
  let PrintMethod = "printCSRSystemRegister";
  let DecoderMethod = "decodeUImmOperand<12>";
  let OperandType = "OPERAND_UIMM12";
  let OperandNamespace = "PrimateOp";
}}

// A parameterized register class alternative to i32imm/i64imm from Target.td.
def ixlenimm : Operand<XLenVT>;

def ixlenimm_li : Operand<XLenVT> {{
  let ParserMatchClass = ImmXLenAsmOperand<"", "LI">;
}}

// Standalone (codegen-only) immleaf patterns.

// A 12-bit signed immediate plus one where the imm range will be -2047~2048.
def simm12_plus1 : ImmLeaf<XLenVT,
  [{{return (isInt<12>(Imm) && Imm != -2048) || Imm == 2048;}}]>;

// A 6-bit constant greater than 32.
def uimm6gt32 : ImmLeaf<XLenVT, [{{
  return isUInt<6>(Imm) && Imm > 32;
}}]>;

// Addressing modes.
// Necessary because a frameindex can't be matched directly in a pattern.
def FrameAddrRegImm : ComplexPattern<iPTR, 2, "SelectFrameAddrRegImm",
                                     [frameindex, or, add]>;
def AddrRegImm : ComplexPattern<iPTR, 2, "SelectAddrRegImm">;

// Addressing modes.
// Necessary because a frameindex can't be matched directly in a pattern.
def AddrFI : ComplexPattern<iPTR, 1, "SelectAddrFI", [frameindex], []>;
def BaseAddr : ComplexPattern<iPTR, 1, "SelectBaseAddr">;

// Return the negation of an immediate value.
def NegImm : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(-N->getSExtValue(), SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Return an immediate value minus 32.
def ImmSub32 : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(N->getSExtValue() - 32, SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Return an immediate value plus 32.
def ImmPlus32 : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(N->getSExtValue() + 32, SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Return an immediate subtracted from XLen.
def ImmSubFromXLen : SDNodeXForm<imm, [{{
  uint64_t XLen = Subtarget->getXLen();
  return CurDAG->getTargetConstant(XLen - N->getZExtValue(), SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Return an immediate subtracted from 32.
def ImmSubFrom32 : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(32 - N->getZExtValue(), SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Check if (add r, imm) can be optimized to (ADDI (ADDI r, imm0), imm1),
// in which imm = imm0 + imm1 and both imm0 and imm1 are simm12.
def AddiPair : PatLeaf<(imm), [{{
  if (!N->hasOneUse())
    return false;
  // The immediate operand must be in range [-4096,-2049] or [2048,4094].
  int64_t Imm = N->getSExtValue();
  return (-4096 <= Imm && Imm <= -2049) || (2048 <= Imm && Imm <= 4094);
}}]>;

// Return imm/2.
def AddiPairImmA : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(N->getSExtValue() / 2, SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Return imm - imm/2.
def AddiPairImmB : SDNodeXForm<imm, [{{
  int64_t Imm = N->getSExtValue();
  return CurDAG->getTargetConstant(Imm - Imm / 2, SDLoc(N),
                                   N->getValueType(0));
}}]>;


def TrailingZeros : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(llvm::countr_zero(N->getZExtValue()),
                                   SDLoc(N), N->getValueType(0));
}}]>;

def XLenSubTrailingOnes : SDNodeXForm<imm, [{{
  uint64_t XLen = Subtarget->getXLen();
  uint64_t TrailingOnes = llvm::countr_one(N->getZExtValue());
  return CurDAG->getTargetConstant(XLen - TrailingOnes, SDLoc(N),
                                   N->getValueType(0));
}}]>;

// Checks if this mask is a non-empty sequence of ones starting at the
// most/least significant bit with the remainder zero and exceeds simm32/simm12.
def LeadingOnesMask : PatLeaf<(imm), [{{
  if (!N->hasOneUse())
    return false;
  return !isInt<32>(N->getSExtValue()) && isMask_64(~N->getSExtValue());
}}], TrailingZeros>;

def TrailingOnesMask : PatLeaf<(imm), [{{
  if (!N->hasOneUse())
    return false;
  return !isInt<12>(N->getSExtValue()) && isMask_64(N->getZExtValue());
}}], XLenSubTrailingOnes>;

// Similar to LeadingOnesMask, but only consider leading ones in the lower 32
// bits.
def LeadingOnesWMask : PatLeaf<(imm), [{{
  if (!N->hasOneUse())
    return false;
  // If the value is a uint32 but not an int32, it must have bit 31 set and
  // bits 63:32 cleared. After that we're looking for a shifted mask but not
  // an all ones mask.
  int64_t Imm = N->getSExtValue();
  return !isInt<32>(Imm) && isUInt<32>(Imm) && isShiftedMask_64(Imm) &&
         Imm != UINT64_C(0xffffffff);
}}], TrailingZeros>;

//===----------------------------------------------------------------------===//
// Instruction Formats
//===----------------------------------------------------------------------===//

include "PrimateInstrFormats.td"

//===----------------------------------------------------------------------===//
// Instruction Class Templates
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class BranchCC_rri<bits<3> funct3, string opcodestr>
    : PRInstB<funct3, OPC_BRANCH, (outs),
              (ins GPR:$rs1, GPR:$rs2, simm13_lsb0:$imm12),
              opcodestr, "$rs1, $rs2, $imm12">,
      Sched<[WriteJmp, ReadJmp, ReadJmp]> {{
  let isBranch = 1;
  let isTerminator = 1;
}}

let Predicates = [HasFullI] in
let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in
class Load_ri<bits<3> funct3, string opcodestr>
    : PRInstI<funct3, OPC_LOAD, (outs GPR:$rd), (ins GPRMem:$rs1, simm12:$imm12),
              opcodestr, "$rd, ${{imm12}}(${{rs1}})">;

// Operands for stores are in the order srcreg, base, offset rather than
// reflecting the order these fields are specified in the instruction
// encoding.
let Predicates = [HasFullI] in
let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in
class Store_rri<bits<3> funct3, string opcodestr>
    : PRInstS<funct3, OPC_STORE, (outs),
              (ins GPR:$rs2, GPRMem:$rs1, simm12:$imm12),
              opcodestr, "$rs2, ${{imm12}}(${{rs1}})">;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class ALU_ri<bits<3> funct3, string opcodestr>
    : PRInstI<funct3, OPC_OP_IMM, (outs GPR:$rd), (ins GPR:$rs1, simm12:$imm12),
              opcodestr, "$rd, $rs1, $imm12">,
      Sched<[WriteIALU, ReadIALU]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Shift_ri<bits<5> imm11_7, bits<3> funct3, string opcodestr>
    : PRInstIShift<imm11_7, funct3, OPC_OP_IMM, (outs GPR:$rd),
                   (ins GPR:$rs1, uimmlog2xlen:$shamt), opcodestr,
                   "$rd, $rs1, $shamt">,
      Sched<[WriteShiftImm, ReadShiftImm]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class ALU_rr<bits<7> funct7, bits<3> funct3, string opcodestr>
    : PRInstR<funct7, funct3, OPC_OP, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
              opcodestr, "$rd, $rs1, $rs2">;

let Predicates = [HasFullI] in
let hasNoSchedulingInfo = 1,
    hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
class CSR_ir<bits<3> funct3, string opcodestr>
    : PRInstI<funct3, OPC_SYSTEM, (outs GPR:$rd), (ins csr_sysreg:$imm12, GPR:$rs1),
              opcodestr, "$rd, $imm12, $rs1">, Sched<[WriteCSR, ReadCSR]>;

let Predicates = [HasFullI] in
let hasNoSchedulingInfo = 1,
    hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
class CSR_ii<bits<3> funct3, string opcodestr>
    : PRInstI<funct3, OPC_SYSTEM, (outs GPR:$rd),
              (ins csr_sysreg:$imm12, uimm5:$rs1),
              opcodestr, "$rd, $imm12, $rs1">, Sched<[WriteCSR]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class ShiftW_ri<bits<7> imm11_5, bits<3> funct3, string opcodestr>
    : PRInstIShiftW<imm11_5, funct3, OPC_OP_IMM_32, (outs GPR:$rd),
                    (ins GPR:$rs1, uimm5:$shamt), opcodestr,
                    "$rd, $rs1, $shamt">,
      Sched<[WriteShiftImm32, ReadShiftImm32]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class ALUW_rr<bits<7> funct7, bits<3> funct3, string opcodestr>
    : PRInstR<funct7, funct3, OPC_OP_32, (outs GPR:$rd),
              (ins GPR:$rs1, GPR:$rs2), opcodestr, "$rd, $rs1, $rs2">;

let Predicates = [HasFullI] in
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
class Priv<string opcodestr, bits<7> funct7>
    : PRInstR<funct7, 0b000, OPC_SYSTEM, (outs), (ins GPR:$rs1, GPR:$rs2),
              opcodestr, "">;

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {{
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
def LUI : PRInstU<OPC_LUI, (outs GPR:$rd), (ins uimm20_lui:$imm20),
                  "lui", "$rd, $imm20">, Sched<[WriteIALU]>;

let Predicates = [HasFullI] in
def AUIPC : PRInstU<OPC_AUIPC, (outs GPR:$rd), (ins uimm20_auipc:$imm20),
                    "auipc", "$rd, $imm20">, Sched<[WriteIALU]>;

let isCall = 1 in
def JAL : PRInstJ<OPC_JAL, (outs GPR:$rd), (ins simm21_lsb0_jal:$imm20),
                  "jal", "$rd, $imm20">, Sched<[WriteJal]>;

let Predicates = [HasFullI] in
let isCall = 1 in
def JALR : PRInstI<0b000, OPC_JALR, (outs GPR:$rd),
                   (ins GPR:$rs1, simm12:$imm12),
                   "jalr", "$rd, ${{imm12}}(${{rs1}})", ItinBranch>,
           Sched<[WriteJalr, ReadJalr]>;
}} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

def BEQ  : BranchCC_rri<0b000, "beq">;
def BNE  : BranchCC_rri<0b001, "bne">;
def BLT  : BranchCC_rri<0b100, "blt">;
def BGE  : BranchCC_rri<0b101, "bge">;
def BLTU : BranchCC_rri<0b110, "bltu">;
def BGEU : BranchCC_rri<0b111, "bgeu">;

let Predicates = [HasFullI], Itinerary = ItinMem in {{
def LB  : Load_ri<0b000, "lb">, Sched<[WriteLDB, ReadMemBase]>;
def LH  : Load_ri<0b001, "lh">, Sched<[WriteLDH, ReadMemBase]>;
def LW  : Load_ri<0b010, "lw">, Sched<[WriteLDW, ReadMemBase]>;
def LBU : Load_ri<0b100, "lbu">, Sched<[WriteLDB, ReadMemBase]>;
def LHU : Load_ri<0b101, "lhu">, Sched<[WriteLDH, ReadMemBase]>;
}} // Predicates = [HasFullI]

let Predicates = [HasFullI], Itinerary = ItinMem in {{
def SB : Store_rri<0b000, "sb">, Sched<[WriteSTB, ReadStoreData, ReadMemBase]>;
def SH : Store_rri<0b001, "sh">, Sched<[WriteSTH, ReadStoreData, ReadMemBase]>;
def SW : Store_rri<0b010, "sw">, Sched<[WriteSTW, ReadStoreData, ReadMemBase]>;
}} // Predicates = [HasFullI]

// ADDI isn't always rematerializable, but isReMaterializable will be used as
// a hint which is verified in isReallyTriviallyReMaterializable.
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
def ADDI  : ALU_ri<0b000, "addi">;

def SLTI  : ALU_ri<0b010, "slti">;
def SLTIU : ALU_ri<0b011, "sltiu">;

let isReMaterializable = 1, isAsCheapAsAMove = 1 in {{
def XORI  : ALU_ri<0b100, "xori">;
def ORI   : ALU_ri<0b110, "ori">;
}}

def ANDI  : ALU_ri<0b111, "andi">;

def SLLI : Shift_ri<0b00000, 0b001, "slli">;
def SRLI : Shift_ri<0b00000, 0b101, "srli">;
def SRAI : Shift_ri<0b01000, 0b101, "srai">;

def ADD  : ALU_rr<0b0000000, 0b000, "add">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def SUB  : ALU_rr<0b0100000, 0b000, "sub">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def SLL  : ALU_rr<0b0000000, 0b001, "sll">, Sched<[WriteShiftReg, ReadShiftReg, ReadShiftReg]>;
def SLT  : ALU_rr<0b0000000, 0b010, "slt">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def SLTU : ALU_rr<0b0000000, 0b011, "sltu">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def XOR  : ALU_rr<0b0000000, 0b100, "xor">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def SRL  : ALU_rr<0b0000000, 0b101, "srl">, Sched<[WriteShiftReg, ReadShiftReg, ReadShiftReg]>;
def SRA  : ALU_rr<0b0100000, 0b101, "sra">, Sched<[WriteShiftReg, ReadShiftReg, ReadShiftReg]>;
def OR   : ALU_rr<0b0000000, 0b110, "or">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;
def AND  : ALU_rr<0b0000000, 0b111, "and">, Sched<[WriteIALU, ReadIALU, ReadIALU]>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {{
let Predicates = [HasFullI] in {{
def FENCE : PRInstI<0b000, OPC_MISC_MEM, (outs),
                    (ins fencearg:$pred, fencearg:$succ),
                    "fence", "$pred, $succ">, Sched<[]> {{
  bits<4> pred;
  bits<4> succ;

  let rs1 = 0;
  let rd = 0;
  let imm12 = {{0b0000,pred,succ}};
}}

def FENCE_TSO : PRInstI<0b000, OPC_MISC_MEM, (outs), (ins), "fence.tso", "">, Sched<[]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = {{0b1000,0b0011,0b0011}};
}}

def FENCE_I : PRInstI<0b001, OPC_MISC_MEM, (outs), (ins), "fence.i", "">, Sched<[]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = 0;
}}

def ECALL : PRInstI<0b000, OPC_SYSTEM, (outs), (ins), "ecall", "">, Sched<[WriteJmp]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = 0;
}}

def EBREAK : PRInstI<0b000, OPC_SYSTEM, (outs), (ins), "ebreak", "">,
             Sched<[]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = 1;
}}
}} // Predicates = [HasFullI]

// This is a de facto standard (as set by GNU binutils) 32-bit unimplemented
// instruction (i.e., it should always trap, if your implementation has invalid
// instruction traps).
def UNIMP : PRInstI<0b001, OPC_SYSTEM, (outs), (ins), "unimp", "">,
            Sched<[]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = 0b110000000000;
}}
}} // hasSideEffects = 1, mayLoad = 0, mayStore = 0

let Predicates = [HasFullI] in {{
def CSRRW : CSR_ir<0b001, "csrrw">;
def CSRRS : CSR_ir<0b010, "csrrs">;
def CSRRC : CSR_ir<0b011, "csrrc">;

def CSRRWI : CSR_ii<0b101, "csrrwi">;
def CSRRSI : CSR_ii<0b110, "csrrsi">;
def CSRRCI : CSR_ii<0b111, "csrrci">;
}} // Predicates = [HasFullI]

/// PR64I instructions

let Predicates = [IsPR64] in {{
def LWU   : Load_ri<0b110, "lwu">, Sched<[WriteLDWU, ReadMemBase]>;
def LD    : Load_ri<0b011, "ld">, Sched<[WriteLDD, ReadMemBase]>;
def SD    : Store_rri<0b011, "sd">, Sched<[WriteSTD, ReadStoreData, ReadMemBase]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def ADDIW : PRInstI<0b000, OPC_OP_IMM_32, (outs GPR:$rd),
                    (ins GPR:$rs1, simm12:$imm12),
                    "addiw", "$rd, $rs1, $imm12">,
            Sched<[WriteIALU32, ReadIALU32]>;

def SLLIW : ShiftW_ri<0b0000000, 0b001, "slliw">;
def SRLIW : ShiftW_ri<0b0000000, 0b101, "srliw">;
def SRAIW : ShiftW_ri<0b0100000, 0b101, "sraiw">;

def ADDW  : ALUW_rr<0b0000000, 0b000, "addw">,
            Sched<[WriteIALU32, ReadIALU32, ReadIALU32]>;
def SUBW  : ALUW_rr<0b0100000, 0b000, "subw">,
            Sched<[WriteIALU32, ReadIALU32, ReadIALU32]>;
def SLLW  : ALUW_rr<0b0000000, 0b001, "sllw">,
            Sched<[WriteShiftReg32, ReadShiftReg32, ReadShiftReg32]>;
def SRLW  : ALUW_rr<0b0000000, 0b101, "srlw">,
            Sched<[WriteShiftReg32, ReadShiftReg32, ReadShiftReg32]>;
def SRAW  : ALUW_rr<0b0100000, 0b101, "sraw">,
            Sched<[WriteShiftReg32, ReadShiftReg32, ReadShiftReg32]>;
}} // Predicates = [IsPR64]

//===----------------------------------------------------------------------===//
// Privileged instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasFullI] in {{
let isBarrier = 1, isReturn = 1, isTerminator = 1 in {{
def URET : Priv<"uret", 0b0000000>, Sched<[]> {{
  let rd = 0;
  let rs1 = 0;
  let rs2 = 0b00010;
}}

def SRET : Priv<"sret", 0b0001000>, Sched<[]> {{
  let rd = 0;
  let rs1 = 0;
  let rs2 = 0b00010;
}}

def MRET : Priv<"mret", 0b0011000>, Sched<[]> {{
  let rd = 0;
  let rs1 = 0;
  let rs2 = 0b00010;
}}
}} // isBarrier = 1, isReturn = 1, isTerminator = 1

def WFI : Priv<"wfi", 0b0001000>, Sched<[]> {{
  let rd = 0;
  let rs1 = 0;
  let rs2 = 0b00101;
}}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def SFENCE_VMA : PRInstR<0b0001001, 0b000, OPC_SYSTEM, (outs),
                         (ins GPR:$rs1, GPR:$rs2),
                         "sfence.vma", "$rs1, $rs2">, Sched<[]> {{
  let rd = 0;
}}
}} // Predicates = [HasFullI]

//===----------------------------------------------------------------------===//
// Debug instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasFullI] in
let isBarrier = 1, isReturn = 1, isTerminator = 1 in {{
def DRET : Priv<"dret", 0b0111101>, Sched<[]> {{
  let rd = 0;
  let rs1 = 0;
  let rs2 = 0b10010;
}}
}} // isBarrier = 1, isReturn = 1, isTerminator = 1

//===----------------------------------------------------------------------===//
// Assembler Pseudo Instructions (User-Level ISA, Version 2.2, Chapter 20)
//===----------------------------------------------------------------------===//

def : InstAlias<"nop",           (ADDI      X0,      X0,       0)>;

// Note that the size is 32 because up to 8 32-bit instructions are needed to
// generate an arbitrary 64-bit immediate. However, the size does not really
// matter since PseudoLI is currently only used in the AsmParser where it gets
// expanded to real instructions immediately.
let hasSideEffects = 0, mayLoad = 0, mayStore = 0, Size = 32,
    isCodeGenOnly = 0, isAsmParserOnly = 1 in
def PseudoLI : Pseudo<(outs GPR:$rd), (ins ixlenimm_li:$imm), [],
                      "li", "$rd, $imm">;

let Predicates = [HasFullI] in {{
def PseudoLB  : PseudoLoad<"lb">;
def PseudoLBU : PseudoLoad<"lbu">;
def PseudoLH  : PseudoLoad<"lh">;
def PseudoLHU : PseudoLoad<"lhu">;
def PseudoLW  : PseudoLoad<"lw">;

def PseudoSB  : PseudoStore<"sb">;
def PseudoSH  : PseudoStore<"sh">;
def PseudoSW  : PseudoStore<"sw">;

let Predicates = [IsPR64] in {{
def PseudoLWU : PseudoLoad<"lwu">;
def PseudoLD  : PseudoLoad<"ld">;
def PseudoSD  : PseudoStore<"sd">;
}} // Predicates = [IsPR64]
}} // Predicates = [HasFullI]

def : InstAlias<"mv $rd, $rs",   (ADDI GPR:$rd, GPR:$rs,       0)>;
def : InstAlias<"not $rd, $rs",  (XORI GPR:$rd, GPR:$rs,      -1)>;
def : InstAlias<"neg $rd, $rs",  (SUB  GPR:$rd,      X0, GPR:$rs)>;

let Predicates = [IsPR64] in {{
def : InstAlias<"negw $rd, $rs",   (SUBW  GPR:$rd,      X0, GPR:$rs)>;
def : InstAlias<"sext.w $rd, $rs", (ADDIW GPR:$rd, GPR:$rs,       0)>;
}} // Predicates = [IsPR64]

def : InstAlias<"seqz $rd, $rs", (SLTIU GPR:$rd, GPR:$rs,       1)>;
def : InstAlias<"snez $rd, $rs", (SLTU  GPR:$rd,      X0, GPR:$rs)>;
def : InstAlias<"sltz $rd, $rs", (SLT   GPR:$rd, GPR:$rs,      X0)>;
def : InstAlias<"sgtz $rd, $rs", (SLT   GPR:$rd,      X0, GPR:$rs)>;

// sgt/sgtu are recognised by the GNU assembler but the canonical slt/sltu
// form will always be printed. Therefore, set a zero weight.
def : InstAlias<"sgt $rd, $rs, $rt", (SLT GPR:$rd, GPR:$rt, GPR:$rs), 0>;
def : InstAlias<"sgtu $rd, $rs, $rt", (SLTU GPR:$rd, GPR:$rt, GPR:$rs), 0>;

def : InstAlias<"beqz $rs, $offset",
                (BEQ GPR:$rs,      X0, simm13_lsb0:$offset)>;
def : InstAlias<"bnez $rs, $offset",
                (BNE GPR:$rs,      X0, simm13_lsb0:$offset)>;
def : InstAlias<"blez $rs, $offset",
                (BGE      X0, GPR:$rs, simm13_lsb0:$offset)>;
def : InstAlias<"bgez $rs, $offset",
                (BGE GPR:$rs,      X0, simm13_lsb0:$offset)>;
def : InstAlias<"bltz $rs, $offset",
                (BLT GPR:$rs,      X0, simm13_lsb0:$offset)>;
def : InstAlias<"bgtz $rs, $offset",
                (BLT      X0, GPR:$rs, simm13_lsb0:$offset)>;

// Always output the canonical mnemonic for the pseudo branch instructions.
// The GNU tools emit the canonical mnemonic for the branch pseudo instructions
// as well (e.g. "bgt" will be recognised by the assembler but never printed by
// objdump). Match this behaviour by setting a zero weight.
def : InstAlias<"bgt $rs, $rt, $offset",
                (BLT  GPR:$rt, GPR:$rs, simm13_lsb0:$offset), 0>;
def : InstAlias<"ble $rs, $rt, $offset",
                (BGE  GPR:$rt, GPR:$rs, simm13_lsb0:$offset), 0>;
def : InstAlias<"bgtu $rs, $rt, $offset",
                (BLTU GPR:$rt, GPR:$rs, simm13_lsb0:$offset), 0>;
def : InstAlias<"bleu $rs, $rt, $offset",
                (BGEU GPR:$rt, GPR:$rs, simm13_lsb0:$offset), 0>;

def : InstAlias<"j $offset",   (JAL X0, simm21_lsb0_jal:$offset)>;
def : InstAlias<"jal $offset", (JAL X1, simm21_lsb0_jal:$offset)>;

let Predicates = [HasFullI] in {{
// Non-zero offset aliases of "jalr" are the lowest weight, followed by the
// two-register form, then the one-register forms and finally "ret".
def : InstAlias<"jr $rs",                (JALR      X0, GPR:$rs, 0), 3>;
def : InstAlias<"jr ${{offset}}(${{rs}})",   (JALR      X0, GPR:$rs, simm12:$offset)>;
def : InstAlias<"jalr $rs",              (JALR      X1, GPR:$rs, 0), 3>;
def : InstAlias<"jalr ${{offset}}(${{rs}})", (JALR      X1, GPR:$rs, simm12:$offset)>;
def : InstAlias<"jalr $rd, $rs",         (JALR GPR:$rd, GPR:$rs, 0), 2>;
def : InstAlias<"ret",                   (JALR      X0,      X1, 0), 4>;

// Non-canonical forms for jump targets also accepted by the assembler.
def : InstAlias<"jr $rs, $offset",        (JALR      X0, GPR:$rs, simm12:$offset), 0>;
def : InstAlias<"jalr $rs, $offset",      (JALR      X1, GPR:$rs, simm12:$offset), 0>;
def : InstAlias<"jalr $rd, $rs, $offset", (JALR GPR:$rd, GPR:$rs, simm12:$offset), 0>;

def : InstAlias<"fence", (FENCE 0xF, 0xF)>; // 0xF == iorw

def : InstAlias<"rdinstret $rd", (CSRRS GPR:$rd, INSTRET.Encoding, X0)>;
def : InstAlias<"rdcycle $rd",   (CSRRS GPR:$rd, CYCLE.Encoding, X0)>;
def : InstAlias<"rdtime $rd",    (CSRRS GPR:$rd, TIME.Encoding, X0)>;

let Predicates = [IsPR32] in {{
def : InstAlias<"rdinstreth $rd", (CSRRS GPR:$rd, INSTRETH.Encoding, X0)>;
def : InstAlias<"rdcycleh $rd",   (CSRRS GPR:$rd, CYCLEH.Encoding, X0)>;
def : InstAlias<"rdtimeh $rd",    (CSRRS GPR:$rd, TIMEH.Encoding, X0)>;
}} // Predicates = [IsPR32]

def : InstAlias<"csrr $rd, $csr", (CSRRS GPR:$rd, csr_sysreg:$csr,      X0)>;
def : InstAlias<"csrw $csr, $rs", (CSRRW      X0, csr_sysreg:$csr, GPR:$rs)>;
def : InstAlias<"csrs $csr, $rs", (CSRRS      X0, csr_sysreg:$csr, GPR:$rs)>;
def : InstAlias<"csrc $csr, $rs", (CSRRC      X0, csr_sysreg:$csr, GPR:$rs)>;

def : InstAlias<"csrwi $csr, $imm", (CSRRWI X0, csr_sysreg:$csr, uimm5:$imm)>;
def : InstAlias<"csrsi $csr, $imm", (CSRRSI X0, csr_sysreg:$csr, uimm5:$imm)>;
def : InstAlias<"csrci $csr, $imm", (CSRRCI X0, csr_sysreg:$csr, uimm5:$imm)>;

let EmitPriority = 0 in {{
def : InstAlias<"csrw $csr, $imm", (CSRRWI X0, csr_sysreg:$csr, uimm5:$imm)>;
def : InstAlias<"csrs $csr, $imm", (CSRRSI X0, csr_sysreg:$csr, uimm5:$imm)>;
def : InstAlias<"csrc $csr, $imm", (CSRRCI X0, csr_sysreg:$csr, uimm5:$imm)>;

def : InstAlias<"csrrw $rd, $csr, $imm", (CSRRWI GPR:$rd, csr_sysreg:$csr, uimm5:$imm)>;
def : InstAlias<"csrrs $rd, $csr, $imm", (CSRRSI GPR:$rd, csr_sysreg:$csr, uimm5:$imm)>;
def : InstAlias<"csrrc $rd, $csr, $imm", (CSRRCI GPR:$rd, csr_sysreg:$csr, uimm5:$imm)>;
}}

def : InstAlias<"sfence.vma",     (SFENCE_VMA      X0, X0)>;
def : InstAlias<"sfence.vma $rs", (SFENCE_VMA GPR:$rs, X0)>;
}} // Predicates = [HasFullI]

let EmitPriority = 0 in {{
let Predicates = [HasFullI] in {{
def : InstAlias<"lb $rd, (${{rs1}})",
                (LB  GPR:$rd, GPR:$rs1, 0)>;
def : InstAlias<"lh $rd, (${{rs1}})",
                (LH  GPR:$rd, GPR:$rs1, 0)>;
def : InstAlias<"lw $rd, (${{rs1}})",
                (LW  GPR:$rd, GPR:$rs1, 0)>;
def : InstAlias<"lbu $rd, (${{rs1}})",
                (LBU  GPR:$rd, GPR:$rs1, 0)>;
def : InstAlias<"lhu $rd, (${{rs1}})",
                (LHU  GPR:$rd, GPR:$rs1, 0)>;

def : InstAlias<"sb $rs2, (${{rs1}})",
                (SB  GPR:$rs2, GPR:$rs1, 0)>;
def : InstAlias<"sh $rs2, (${{rs1}})",
                (SH  GPR:$rs2, GPR:$rs1, 0)>;
def : InstAlias<"sw $rs2, (${{rs1}})",
                (SW  GPR:$rs2, GPR:$rs1, 0)>;
}} // Predicates = [HasFullI]

def : InstAlias<"add $rd, $rs1, $imm12",
                (ADDI  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
def : InstAlias<"and $rd, $rs1, $imm12",
                (ANDI  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
def : InstAlias<"xor $rd, $rs1, $imm12",
                (XORI  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
def : InstAlias<"or $rd, $rs1, $imm12",
                (ORI  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
def : InstAlias<"sll $rd, $rs1, $shamt",
                (SLLI  GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt)>;
def : InstAlias<"srl $rd, $rs1, $shamt",
                (SRLI  GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt)>;
def : InstAlias<"sra $rd, $rs1, $shamt",
                (SRAI  GPR:$rd, GPR:$rs1, uimmlog2xlen:$shamt)>;
let Predicates = [IsPR64] in {{
let Predicates = [HasFullI] in {{
def : InstAlias<"lwu $rd, (${{rs1}})",
                (LWU  GPR:$rd, GPR:$rs1, 0)>;
def : InstAlias<"ld $rd, (${{rs1}})",
                (LD  GPR:$rd, GPR:$rs1, 0)>;
def : InstAlias<"sd $rs2, (${{rs1}})",
                (SD  GPR:$rs2, GPR:$rs1, 0)>;
}} // Predicates = [HasFullI]

def : InstAlias<"addw $rd, $rs1, $imm12",
                (ADDIW  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
def : InstAlias<"sllw $rd, $rs1, $shamt",
                (SLLIW  GPR:$rd, GPR:$rs1, uimm5:$shamt)>;
def : InstAlias<"srlw $rd, $rs1, $shamt",
                (SRLIW  GPR:$rd, GPR:$rs1, uimm5:$shamt)>;
def : InstAlias<"sraw $rd, $rs1, $shamt",
                (SRAIW  GPR:$rd, GPR:$rs1, uimm5:$shamt)>;
}} // Predicates = [IsPR64]
def : InstAlias<"slt $rd, $rs1, $imm12",
                (SLTI  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
def : InstAlias<"sltu $rd, $rs1, $imm12",
                (SLTIU  GPR:$rd, GPR:$rs1, simm12:$imm12)>;
}}

def : MnemonicAlias<"move", "mv">;

// The SCALL and SBREAK instructions wererenamed to ECALL and EBREAK in
// version 2.1 of the user-level ISA. Like the GNU toolchain, we still accept
// the old name for backwards compatibility.
let Predicates = [HasFullI] in {{
def : MnemonicAlias<"scall", "ecall">;
def : MnemonicAlias<"sbreak", "ebreak">;
}} // Predicates = [HasFullI]

// This alias was added to the spec in December 2020. Don't print it by default
// to allow assembly we print to be compatible with versions of GNU assembler
// that don't support this alias.
def : InstAlias<"zext.b $rd, $rs", (ANDI GPR:$rd, GPR:$rs, 0xFF), 0>;

//===----------------------------------------------------------------------===//
// Primate special ops
//===----------------------------------------------------------------------===//
def : InstAlias<"end", (JAL X0, -1)>;

let Itinerary = ItinIO in {{

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def INPUT_READ :
    PRInstI<0b011, OPC_PR_INPUT, (outs WIDEREG:$rd), (ins GPR:$rs1, simm12:$imm12),
        "inputread", "$rd, $rs1, $imm12">, Sched<[WriteIALU, ReadIALU]> {{
          let IsBFUInstruction = 1;
        }}

def INPUT_DONE :
    PRInstI<0b100, OPC_PR_INPUT, (outs), (ins),
        "inputdone", "">, Sched<[WriteIALU, ReadIALU]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = 0;
  let IsBFUInstruction = 1;
}}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def INPUT_SEEK :
    PRInstI<0b001, OPC_PR_INPUT, (outs GPR:$rd), (ins GPR:$rs1, simm12:$imm12),
        "inputseek", "$rd, $rs1, $imm12">, Sched<[WriteIALU, ReadIALU]> {{
          let IsBFUInstruction = 1;
        }}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def OUTPUT_WRITE :
    PRInstI<0b001, OPC_PR_OUTPUT, (outs), (ins WIDEREG:$rs1, simm12:$imm12),
        "outputwrite", "$rs1, $imm12">, Sched<[WriteIALU, ReadIALU]> {{
          let rd = 0;
          let IsBFUInstruction = 1;
        }}

def OUTPUT_DONE:
    PRInstI<0b010, OPC_PR_OUTPUT, (outs), (ins),
        "outputdone", "">, Sched<[WriteIALU, ReadIALU]> {{
  let rs1 = 0;
  let rd = 0;
  let imm12 = 0;
  let IsBFUInstruction = 1;
}}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def OUTPUT_SEEK :
    PRInstI<0b011, OPC_PR_OUTPUT, (outs GPR:$rd), (ins GPR:$rs1, simm12:$imm12),
        "outputseek", "$rd, $rs1, $imm12">, Sched<[WriteIALU, ReadIALU]> {{
      let IsBFUInstruction = 1;
    }}
}}


let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def EXTRACT :
    PRInstI<0b000, OPC_PR_REG, (outs GPR:$rd), (ins WIDEREG:$rs1, simm12:$imm12),
        "extract", "$rd, $rs1, $imm12">, Sched<[WriteIALU, ReadIALU]> 
        {{
          let Itinerary = ItinGreen;
}}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def EXTRACT_hang :
    PRInstI<0b010, OPC_PR_REG, (outs GPR128:$rd), (ins WIDEREG:$rs1, simm12:$imm12),
        "extracth", "$rd, $rs1, $imm12">, Sched<[WriteIALU, ReadIALU]> 
        {{
          let Itinerary = ItinGreen;
}}

def : Pat<(extract_value WIDEREG:$rs1, simm12:$rs2), (EXTRACT_hang WIDEREG:$rs1, simm12:$rs2)>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def INSERT :
    PRInstI<0b001, OPC_PR_REG, (outs WIDEREG:$rd), (ins WIDEREG:$rs1, GPR:$rs2, simm12:$imm12),
        "insert", "$rd, $rs1, $rs2, $imm12">, Sched<[WriteIALU, ReadIALU]> 
        {{
          let Constraints = "$rd = $rs1";
          let Itinerary = ItinGreen;
}}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def INSERT_hang :
    PRInstI<0b011, OPC_PR_REG, (outs WIDEREG:$rd), (ins WIDEREG:$rs1, GPR128:$rs2, simm12:$imm12),
        "inserth", "$rd, $rs1, $rs2, $imm12">, Sched<[WriteIALU, ReadIALU]> 
        {{
          let Constraints = "$rd = $rs1";
          let Itinerary = ItinGreen;
}}
def : Pat<(insert_value WIDEREG:$rs0, GPR128:$rs1, simm12:$rs2), (INSERT_hang WIDEREG:$rs0, GPR128:$rs1, simm12:$rs2)>;

{BFUInstDefs}

{BFUInstPattern}

//===----------------------------------------------------------------------===//
// Pseudo-instructions and codegen patterns
//
// Naming convention: For 'generic' pattern classes, we use the naming
// convention PatTy1Ty2. For pattern classes which offer a more complex
// expansion, prefix the class name, e.g. BccPat.
//===----------------------------------------------------------------------===//

/// Generic pattern classes

class PatGpr<SDPatternOperator OpNode, PRInst Inst, ValueType vt = XLenVT>
    : Pat<(vt (OpNode (vt GPR:$rs1))), (Inst GPR:$rs1)>;
class PatGprGpr<SDPatternOperator OpNode, PRInst Inst, ValueType vt1 = XLenVT,
                ValueType vt2 = XLenVT>
    : Pat<(vt1 (OpNode (vt1 GPR:$rs1), (vt2 GPR:$rs2))), (Inst GPR:$rs1, GPR:$rs2)>;

class PatGprImm<SDPatternOperator OpNode, PRInst Inst, ImmLeaf ImmType,
                ValueType vt = XLenVT>
    : Pat<(vt (OpNode (vt GPR:$rs1), ImmType:$imm)),
          (Inst GPR:$rs1, ImmType:$imm)>;
class PatGprSimm12<SDPatternOperator OpNode, PRInstI Inst>
    : PatGprImm<OpNode, Inst, simm12>;
class PatGprUimmLog2XLen<SDPatternOperator OpNode, PRInstIShift Inst>
    : PatGprImm<OpNode, Inst, uimmlog2xlen>;

/// Predicates

def IsOrAdd: PatFrag<(ops node:$A, node:$B), (or node:$A, node:$B), [{{
  return isOrEquivalentToAdd(N);
}}]>;
def assertsexti32 : PatFrag<(ops node:$src), (assertsext node:$src), [{{
  return cast<VTSDNode>(N->getOperand(1))->getVT().bitsLE(MVT::i32);
}}]>;
def sexti32 : ComplexPattern<i64, 1, "selectSExti32">;
def assertzexti32 : PatFrag<(ops node:$src), (assertzext node:$src), [{{
  return cast<VTSDNode>(N->getOperand(1))->getVT().bitsLE(MVT::i32);
}}]>;
def zexti32 : ComplexPattern<i64, 1, "selectZExti32">;

def add_oneuse : PatFrag<(ops node:$A, node:$B), (add node:$A, node:$B), [{{
  return N->hasOneUse();
}}]>;

def mul_oneuse : PatFrag<(ops node:$A, node:$B), (mul node:$A, node:$B), [{{
  return N->hasOneUse();
}}]>;

def sexti16 : ComplexPattern<XLenVT, 1, "selectSExtBits<16>">;
def zexti16 : ComplexPattern<XLenVT, 1, "selectZExtBits<16>">;
def zexti16i32 : ComplexPattern<i32, 1, "selectZExtBits<16>">;
def zexti8 : ComplexPattern<XLenVT, 1, "selectZExtBits<8>">;
def zexti8i32 : ComplexPattern<i32, 1, "selectZExtBits<8>">;

/// Simple arithmetic operations

def : PatGprGpr<add, ADD>;
def : PatGprSimm12<add, ADDI>;
def : PatGprGpr<sub, SUB>;
def : PatGprGpr<or, OR>;
def : PatGprSimm12<or, ORI>;
def : PatGprGpr<and, AND>;
def : PatGprSimm12<and, ANDI>;
def : PatGprGpr<xor, XOR>;
def : PatGprSimm12<xor, XORI>;
def : PatGprUimmLog2XLen<shl, SLLI>;
def : PatGprUimmLog2XLen<srl, SRLI>;
def : PatGprUimmLog2XLen<sra, SRAI>;

// Select 'or' as ADDI if the immediate bits are known to be 0 in $rs1. This
// can improve compressibility.
def or_is_add : PatFrag<(ops node:$lhs, node:$rhs), (or node:$lhs, node:$rhs),[{{
  KnownBits Known0 = CurDAG->computeKnownBits(N->getOperand(0), 0);
  KnownBits Known1 = CurDAG->computeKnownBits(N->getOperand(1), 0);
  return KnownBits::haveNoCommonBitsSet(Known0, Known1);
}}]>;
def : PatGprSimm12<or_is_add, ADDI>;

// Match both a plain shift and one where the shift amount is masked (this is
// typically introduced when the legalizer promotes the shift amount and
// zero-extends it). For Primate, the mask is unnecessary as shifts in the base
// ISA only read the least significant 5 bits (PR32I) or 6 bits (PR64I).
def shiftMaskXLen : ComplexPattern<XLenVT, 1, "selectShiftMaskXLen", [], [], 0>;
def shiftMask32   : ComplexPattern<i64, 1, "selectShiftMask32", [], [], 0>;

class shiftop<SDPatternOperator operator>
    : PatFrag<(ops node:$val, node:$count),
              (operator node:$val, (XLenVT (shiftMaskXLen node:$count)))>;
class shiftopw<SDPatternOperator operator>
    : PatFrag<(ops node:$val, node:$count),
              (operator node:$val, (i64 (shiftMask32 node:$count)))>;

def : PatGprGpr<shiftop<shl>, SLL>;
def : PatGprGpr<shiftop<srl>, SRL>;
def : PatGprGpr<shiftop<sra>, SRA>;

// This is a special case of the ADD instruction used to facilitate the use of a
// fourth operand to emit a relocation on a symbol relating to this instruction.
// The relocation does not affect any bits of the instruction itself but is used
// as a hint to the linker.
let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isCodeGenOnly = 0 in
def PseudoAddTPRel : Pseudo<(outs GPR:$rd),
                            (ins GPR:$rs1, GPR:$rs2, tprel_add_symbol:$src), [],
                            "add", "$rd, $rs1, $rs2, $src">;

/// FrameIndex calculations

def : Pat<(add (XLenVT AddrFI:$Rs), simm12:$imm12),
          (ADDI (XLenVT AddrFI:$Rs), simm12:$imm12)>;
def : Pat<(IsOrAdd (XLenVT AddrFI:$Rs), simm12:$imm12),
          (ADDI (XLenVT AddrFI:$Rs), simm12:$imm12)>;

/// HI and ADD_LO address nodes.

def : Pat<(primate_hi tglobaladdr:$in), (LUI tglobaladdr:$in)>;
def : Pat<(primate_hi tblockaddress:$in), (LUI tblockaddress:$in)>;
def : Pat<(primate_hi tjumptable:$in), (LUI tjumptable:$in)>;
def : Pat<(primate_hi tconstpool:$in), (LUI tconstpool:$in)>;

def : Pat<(primate_add_lo GPR:$hi, tglobaladdr:$lo),
          (ADDI GPR:$hi, tglobaladdr:$lo)>;
def : Pat<(primate_add_lo GPR:$hi, tblockaddress:$lo),
          (ADDI GPR:$hi, tblockaddress:$lo)>;
def : Pat<(primate_add_lo GPR:$hi, tjumptable:$lo),
          (ADDI GPR:$hi, tjumptable:$lo)>;
def : Pat<(primate_add_lo GPR:$hi, tconstpool:$lo),
          (ADDI GPR:$hi, tconstpool:$lo)>;

/// Setcc

def : PatGprGpr<setlt, SLT>;
def : PatGprSimm12<setlt, SLTI>;
def : PatGprGpr<setult, SLTU>;
def : PatGprSimm12<setult, SLTIU>;

// Define pattern expansions for setcc operations that aren't directly
// handled by a Primate instruction.
def : Pat<(XLenVT (seteq (XLenVT GPR:$rs1), 0)), (SLTIU GPR:$rs1, 1)>;
def : Pat<(XLenVT (seteq (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (SLTIU (XOR GPR:$rs1, GPR:$rs2), 1)>;
def : Pat<(XLenVT (seteq (XLenVT GPR:$rs1), simm12_plus1:$imm12)),
          (SLTIU (ADDI GPR:$rs1, (NegImm simm12_plus1:$imm12)), 1)>;
def : Pat<(XLenVT (setne (XLenVT GPR:$rs1), 0)), (SLTU (XLenVT X0), GPR:$rs1)>;
def : Pat<(XLenVT (setne (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (SLTU (XLenVT X0), (XOR GPR:$rs1, GPR:$rs2))>;
def : Pat<(XLenVT (setne (XLenVT GPR:$rs1), simm12_plus1:$imm12)),
          (SLTU (XLenVT X0), (ADDI GPR:$rs1, (NegImm simm12_plus1:$imm12)))>;
def : Pat<(XLenVT (setugt (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (SLTU GPR:$rs2, GPR:$rs1)>;
def : Pat<(XLenVT (setuge (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (XORI (XLenVT (SLTU GPR:$rs1, GPR:$rs2)), 1)>;
def : Pat<(XLenVT (setule (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (XORI (XLenVT (SLTU GPR:$rs2, GPR:$rs1)), 1)>;
def : Pat<(XLenVT (setgt  (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (SLT GPR:$rs2, GPR:$rs1)>;
def : Pat<(XLenVT (setge  (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (XORI (XLenVT (SLT GPR:$rs1, GPR:$rs2)), 1)>;
def : Pat<(XLenVT (setle  (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))), (XORI (XLenVT (SLT GPR:$rs2, GPR:$rs1)), 1)>;

def : Pat<(int_primate_output WIDEREG:$rs1, simm12:$imm), (OUTPUT_WRITE WIDEREG:$rs1, simm12:$imm)>;
def : Pat<(int_primate_output_done), (OUTPUT_DONE)>;

def : Pat<(int_primate_input simm12:$imm), (INPUT_READ (XLenVT X0), simm12:$imm)>;
def : Pat<(int_primate_input GPR:$rs1), (INPUT_READ (XLenVT GPR:$rs1), 0)>;
def : Pat<(int_primate_input_done), (INPUT_DONE)>;

{BFUInstPattern}

let usesCustomInserter = 1 in
class SelectCC_rrirr<RegisterClass valty, RegisterClass cmpty, ValueType vt = XLenVT>
    : Pseudo<(outs valty:$dst),
             (ins cmpty:$lhs, cmpty:$rhs, ixlenimm:$imm,
              valty:$truev, valty:$falsev),
             [(set (vt valty:$dst), (primate_selectcc (vt cmpty:$lhs), (vt cmpty:$rhs),
              (XLenVT timm:$imm), (vt valty:$truev), (vt valty:$falsev)))]>;

def Select_GPR_Using_CC_GPR : SelectCC_rrirr<GPR, GPR>;

/// Branches and jumps

// Match `primate_brcc` and lower to the appropriate Primate branch instruction.
class BccPat<CondCode Cond, PRInstB Inst, ValueType vt=XLenVT>
    : Pat<(primate_brcc (vt GPR:$rs1), (vt GPR:$rs2), Cond, bb:$imm12),
          (Inst GPR:$rs1, GPR:$rs2, simm13_lsb0:$imm12)>;

def : BccPat<SETEQ, BEQ>;
def : BccPat<SETNE, BNE>;
def : BccPat<SETLT, BLT>;
def : BccPat<SETGE, BGE>;
def : BccPat<SETULT, BLTU>;
def : BccPat<SETUGE, BGEU>;

let isBarrier = 1, isBranch = 1, isTerminator = 1 in
def PseudoBR : Pseudo<(outs), (ins simm21_lsb0_jal:$imm20), [(br bb:$imm20)]>,
               PseudoInstExpansion<(JAL X0, simm21_lsb0_jal:$imm20)>{{
  let Itinerary = ItinBranch;
}}

let Predicates = [HasFullI] in {{
let isBarrier = 1, isBranch = 1, isIndirectBranch = 1, isTerminator = 1 in
def PseudoBRIND : Pseudo<(outs), (ins GPRJALR:$rs1, simm12:$imm12), []>,
                  PseudoInstExpansion<(JALR X0, GPR:$rs1, simm12:$imm12)>{{
  let Itinerary = ItinBranch;
}}

def : Pat<(brind GPRJALR:$rs1), (PseudoBRIND GPRJALR:$rs1, 0)>;
def : Pat<(brind (add GPRJALR:$rs1, simm12:$imm12)),
          (PseudoBRIND GPRJALR:$rs1, simm12:$imm12)>;
}} // Predicates = [HasFullI]

def IntCCtoPrimateCC : SDNodeXForm<primate_selectcc, [{{
  ISD::CondCode CC = cast<CondCodeSDNode>(N->getOperand(2))->get();
  PrimateCC::CondCode BrCC = getPrimateCCForIntCC(CC);
  return CurDAG->getTargetConstant(BrCC, SDLoc(N), Subtarget->getXLenVT());
}}]>;

def primate_selectcc_frag : PatFrag<(ops node:$lhs, node:$rhs, node:$cc,
                                       node:$truev, node:$falsev),
                                  (primate_selectcc node:$lhs, node:$rhs,
                                                  node:$cc, node:$truev,
                                                  node:$falsev), [{{}}],
                                  IntCCtoPrimateCC>;


multiclass SelectCC_GPR_rrirr<DAGOperand valty, ValueType vt> {{
  let usesCustomInserter = 1 in
  def _Using_CC_GPR : Pseudo<(outs valty:$dst),
                             (ins GPR:$lhs, GPR:$rhs, ixlenimm:$cc,
                              valty:$truev, valty:$falsev),
                             [(set valty:$dst,
                               (primate_selectcc_frag:$cc (XLenVT GPR:$lhs), GPR:$rhs, cond,
                                                        (vt valty:$truev), valty:$falsev))]>;
  // Explicitly select 0 in the condition to X0. The register coalescer doesn't
  // always do it.
  def : Pat<(primate_selectcc_frag:$cc (XLenVT GPR:$lhs), 0, cond, (vt valty:$truev),
                                     valty:$falsev),
            (!cast<Instruction>(NAME#"_Using_CC_GPR") GPR:$lhs, (XLenVT X0),
             (IntCCtoPrimateCC $cc), valty:$truev, valty:$falsev)>;
}}


let Predicates = [HasFullI] in {{
// PseudoCALLReg is a generic pseudo instruction for calls which will eventually
// expand to auipc and jalr while encoding, with any given register used as the
// destination.
// Define AsmString to print "call" when compile with -S flag.
// Define isCodeGenOnly = 0 to support parsing assembly "call" instruction.
let isCall = 1, isBarrier = 1, isCodeGenOnly = 0, hasSideEffects = 0,
    mayStore = 0, mayLoad = 0 in
def PseudoCALLReg : Pseudo<(outs GPR:$rd), (ins call_symbol:$func), []> {{
  let AsmString = "call\t$rd, $func";
  let Itinerary = ItinBranch;
}}

// PseudoCALL is a pseudo instruction which will eventually expand to auipc
// and jalr while encoding. This is desirable, as an auipc+jalr pair with
// R_Primate_CALL and R_Primate_RELAX relocations can be be relaxed by the linker
// if the offset fits in a signed 21-bit immediate.
// Define AsmString to print "call" when compile with -S flag.
// Define isCodeGenOnly = 0 to support parsing assembly "call" instruction.
let isCall = 1, Defs = [X1], isCodeGenOnly = 0 in
def PseudoCALL : Pseudo<(outs), (ins call_symbol:$func), []> {{
  let AsmString = "call\t$func";
  let Itinerary = ItinBranch;
}}

def : Pat<(primate_call tglobaladdr:$func), (PseudoCALL tglobaladdr:$func)>;
def : Pat<(primate_call texternalsym:$func), (PseudoCALL texternalsym:$func)>;

def : Pat<(primate_uret_flag), (URET (XLenVT X0), (XLenVT X0))>;
def : Pat<(primate_sret_flag), (SRET (XLenVT X0), (XLenVT X0))>;
def : Pat<(primate_mret_flag), (MRET (XLenVT X0), (XLenVT X0))>;

let isCall = 1, Defs = [X1] in
def PseudoCALLIndirect : Pseudo<(outs), (ins GPRJALR:$rs1),
                                [(primate_call GPRJALR:$rs1)]>,
                         PseudoInstExpansion<(JALR X1, GPR:$rs1, 0)>{{
  let Itinerary = ItinBranch;
}}

let isBarrier = 1, isReturn = 1, isTerminator = 1 in
def PseudoRET : Pseudo<(outs), (ins), [(primate_ret_flag)]>,
                PseudoInstExpansion<(JAL X0, -2)> {{
  let Itinerary = ItinBranch;
}}

class PatWideWideImm<SDPatternOperator OpNode, PRInst Inst>
    : Pat<
          (insert_value 
            WIDEREG:$rs0, 
            (OpNode 
              (i32 (extract_value 
                WIDEREG:$rs1, 
                simm12:$imm1)),  
                simm12:$imm2), 
            simm12:$imm0), 
      (Inst WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, simm12:$imm2)>;

class PatWideWideWide<SDPatternOperator OpNode, PRInst Inst>
    : Pat<
          (insert_value 
            WIDEREG:$rs0, 
            (OpNode 
              (i32 (extract_value 
                WIDEREG:$rs1, 
                simm12:$imm1)),  
                (extract_value 
                 WIDEREG:$rs2, 
                 simm12:$imm2)), 
            simm12:$imm0), 
      (Inst WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2)>;
class PatScalarWideWide<SDPatternOperator OpNode, PRInst Inst>
    : Pat< 
            (OpNode 
              (i32 (extract_value 
                WIDEREG:$rs1, 
                simm12:$imm1)),  
                (extract_value 
                 WIDEREG:$rs2, 
                 simm12:$imm2)),
      (Inst WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2)>;
class PatScalarWideImm<SDPatternOperator OpNode, PRInst Inst>
    : Pat< 
            (OpNode 
              (i32 (extract_value 
                WIDEREG:$rs1, 
                simm12:$imm1)),   
              simm12:$imm2),
      (Inst WIDEREG:$rs1, simm12:$imm1, simm12:$imm2)>;
class PatWideScalarScalar<SDPatternOperator OpNode, PRInst Inst> 
  : Pat<
    (insert_value 
      WIDEREG:$rs0, 
      (OpNode 
      (XLenVT GPR:$rs1),  
      (XLenVT GPR:$rs2)),  
      simm12:$imm0),
    (Inst WIDEREG:$rs0, simm12:$imm0, (XLenVT GPR:$rs1), (XLenVT GPR:$rs2))>;
class PatWideScalarImm<SDPatternOperator OpNode, PRInst Inst> 
  : Pat<
    (insert_value 
      WIDEREG:$rs0, 
      (OpNode 
        (XLenVT GPR:$rs1),  
        simm12:$imm1), 
      simm12:$imm0),
    (Inst WIDEREG:$rs0, simm12:$imm0, (XLenVT GPR:$rs1), simm12:$imm1)>;
class PatWideWide<SDPatternOperator OpNode, PRInst Inst>
    : Pat<(OpNode (extract_value WIDEREG:$rs1, simm12:$imm1), (extract_value WIDEREG:$rs2, simm12:$imm2)), 
      (Inst WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2)>;



let mayLoad = 0, mayStore = 0, hasSideEffects= 0 in {{
// def PseudoMov : Pseudo<(outs WIDEREG:$rd), 
//       (ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1), 
//       [(set 
//           WIDEREG:$rd, 
//           (insert_value 
//             WIDEREG:$rs0, 
//             (i32 (extract_value 
//               WIDEREG:$rs1, 
//               simm12:$imm1)),
//             simm12:$imm0
//         )
//       )]>{{
//         let Constraints = "$rd = $rs0";
//         let Itinerary = ItinGreen;
//       }}

def PseudoADDsww : Pseudo<(outs GPR:$rd), 
      (ins WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2), 
      []>{{
        let Itinerary = ItinGreen;
      }}

def PseudoADDwww : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2), 
      []>{{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

def PseudoADDwss : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, GPR:$rs2), 
      []>{{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

let Itinerary = ItinGreen in
def PseudoADDIswi : Pseudo<(outs GPR:$rd), 
      (ins WIDEREG:$rs1, simm12:$imm1, simm12:$imm2), 
      []>;

def PseudoADDIwwi : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, simm12:$imm2), 
      []> {{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

def PseudoADDIwsi : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, simm12:$imm1), 
      []> {{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

def PseudoANDIswi : Pseudo<(outs GPR:$rd), 
      (ins WIDEREG:$rs1, simm12:$imm1, simm12:$imm2), 
      []>;

def PseudoANDIwwi : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, simm12:$imm2), 
      []> {{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

def PseudoANDIwsi : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, simm12:$imm1), 
      []> {{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

def PseudoANDsww : Pseudo<(outs GPR:$rd), 
      (ins WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2), 
      []>{{
        let Itinerary = ItinGreen;
      }}

def PseudoANDwww : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, WIDEREG:$rs1, simm12:$imm1, WIDEREG:$rs2, simm12:$imm2), 
      []>{{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}

def PseudoANDwss : Pseudo<(outs WIDEREG:$rd), 
      (ins WIDEREG:$rs0, simm12:$imm0, GPR:$rs1, GPR:$rs2), 
      []>{{
        let Itinerary = ItinGreen;
        let Constraints = "$rd = $rs0";
      }}
}}

def : PatWideScalarImm<add, PseudoADDIwsi>;
def : PatScalarWideImm<add, PseudoADDIswi>;
def : PatWideWideImm<add, PseudoADDIwwi>;

def : PatWideScalarScalar<add, PseudoADDwss>;
def : PatScalarWideWide<add, PseudoADDsww>;
def : PatWideWideWide<add, PseudoADDwww>;

def : PatWideScalarImm<and, PseudoANDIwsi>;
def : PatScalarWideImm<and, PseudoANDIswi>;
def : PatWideWideImm<and, PseudoANDIwwi>;

def : PatWideScalarScalar<and, PseudoANDwss>;
def : PatScalarWideWide<and, PseudoANDsww>;
def : PatWideWideWide<and, PseudoANDwww>;

// missed op patterns
def : Pat<(extract_value WIDEREG:$rs1, simm12:$imm1), 
           (XLenVT (EXTRACT WIDEREG:$rs1, simm12:$imm1))>;
def : Pat<(insert_value WIDEREG:$rs1, (XLenVT GPR:$rs2), simm12:$imm1), 
           (PrimateAGGVT (INSERT WIDEREG:$rs1, GPR:$rs2, simm12:$imm1))>;


// PseudoTAIL is a pseudo instruction similar to PseudoCALL and will eventually// expand to auipc and jalr while encoding.
// Define AsmString to print "tail" when compile with -S flag.
let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1, Uses = [X2],
    isCodeGenOnly = 0 in
def PseudoTAIL : Pseudo<(outs), (ins call_symbol:$dst), []> {{
  let AsmString = "tail\t$dst";
}}

let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1, Uses = [X2] in
def PseudoTAILIndirect : Pseudo<(outs), (ins GPRTC:$rs1),
                                [(primate_tail GPRTC:$rs1)]>,
                         PseudoInstExpansion<(JALR X0, GPR:$rs1, 0)>;

def : Pat<(primate_tail (iPTR tglobaladdr:$dst)),
          (PseudoTAIL texternalsym:$dst)>;
def : Pat<(primate_tail (iPTR texternalsym:$dst)),
          (PseudoTAIL texternalsym:$dst)>;
}} // Predicates = [HasFullI]

let isCall = 0, isBarrier = 1, isBranch = 1, isTerminator = 1,
    isCodeGenOnly = 0, hasSideEffects = 0, mayStore = 0, mayLoad = 0 in
def PseudoJump : Pseudo<(outs GPR:$rd), (ins pseudo_jump_symbol:$target), []> {{
  let AsmString = "jump\t$target, $rd";
}}

let Predicates = [HasFullI] in {{
let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isCodeGenOnly = 0,
    isAsmParserOnly = 1 in
def PseudoLLA : Pseudo<(outs GPR:$dst), (ins bare_symbol:$src), [],
                       "lla", "$dst, $src">;

let hasSideEffects = 0, mayLoad = 1, mayStore = 0, Size = 8, isCodeGenOnly = 0,
    isAsmParserOnly = 1 in
def PseudoLGA : Pseudo<(outs GPR:$dst), (ins bare_symbol:$src), [],
                       "lga", "$dst, $src">;

let hasSideEffects = 0, mayLoad = 1, mayStore = 0, isCodeGenOnly = 0,
    isAsmParserOnly = 1 in
def PseudoLA : Pseudo<(outs GPR:$dst), (ins bare_symbol:$src), [],
                      "la", "$dst, $src">;

let hasSideEffects = 0, mayLoad = 1, mayStore = 0, isCodeGenOnly = 0,
    isAsmParserOnly = 1 in
def PseudoLA_TLS_IE : Pseudo<(outs GPR:$dst), (ins bare_symbol:$src), [],
                             "la.tls.ie", "$dst, $src">;

let hasSideEffects = 0, mayLoad = 1, mayStore = 0, isCodeGenOnly = 0,
    isAsmParserOnly = 1 in
def PseudoLA_TLS_GD : Pseudo<(outs GPR:$dst), (ins bare_symbol:$src), [],
                             "la.tls.gd", "$dst, $src">;
}} // Predicates = [HasFullI]


/// Sign/Zero Extends

// There are single-instruction versions of these in Zbb, so disable these
// Pseudos if that extension is present.
let hasSideEffects = 0, mayLoad = 0,
    mayStore = 0, isCodeGenOnly = 0, isAsmParserOnly = 1 in {{
def PseudoSEXT_B : Pseudo<(outs GPR:$rd), (ins GPR:$rs), [], "sext.b", "$rd, $rs">;
def PseudoSEXT_H : Pseudo<(outs GPR:$rd), (ins GPR:$rs), [], "sext.h", "$rd, $rs">;
// pr64's sext.w is defined above, using InstAlias<"sext.w ...
// zext.b is defined above, using InstAlias<"zext.b ...
def PseudoZEXT_H : Pseudo<(outs GPR:$rd), (ins GPR:$rs), [], "zext.h", "$rd, $rs">;
}} // hasSideEffects = 0, ...

let Predicates = [IsPR64], hasSideEffects = 0, mayLoad = 0, mayStore = 0,
  isCodeGenOnly = 0, isAsmParserOnly = 1 in {{
def PseudoZEXT_W : Pseudo<(outs GPR:$rd), (ins GPR:$rs), [], "zext.w", "$rd, $rs">;
}} // Predicates = [IsPR64], ...

/// Loads

class LdPat<PatFrag LoadOp, PRInst Inst, ValueType vt = XLenVT>
    : Pat<(vt (LoadOp (AddrRegImm (XLenVT GPR:$rs1), simm12:$imm12))),
          (Inst GPR:$rs1, simm12:$imm12)>;

let Predicates = [HasFullI] in {{
multiclass LdPat<PatFrag LoadOp, PRInst Inst, ValueType vt = XLenVT> {{
  def : Pat<(vt (LoadOp BaseAddr:$rs1)), (Inst BaseAddr:$rs1, 0)>;
  def : Pat<(vt (LoadOp (add BaseAddr:$rs1, simm12:$imm12))),
            (Inst BaseAddr:$rs1, simm12:$imm12)>;
  def : Pat<(vt (LoadOp (IsOrAdd AddrFI:$rs1, simm12:$imm12))),
            (Inst AddrFI:$rs1, simm12:$imm12)>;
}}

defm : LdPat<sextloadi8, LB>;
defm : LdPat<extloadi8, LB>;
defm : LdPat<sextloadi16, LH>;
defm : LdPat<extloadi16, LH>;
defm : LdPat<load, LW, i32>, Requires<[IsPR32]>;
defm : LdPat<zextloadi8, LBU>;
defm : LdPat<zextloadi16, LHU>;
}} // Predicates = [HasFullI]

/// Stores

class StPat<PatFrag StoreOp, PRInst Inst, RegisterClass StTy,
            ValueType vt>
    : Pat<(StoreOp (vt StTy:$rs2), (AddrRegImm (XLenVT GPR:$rs1),
                   simm12:$imm12)),
          (Inst StTy:$rs2, GPR:$rs1, simm12:$imm12)>;

let Predicates = [HasFullI] in {{
multiclass StPat<PatFrag StoreOp, PRInst Inst, RegisterClass StTy,
                 ValueType vt> {{
  def : Pat<(StoreOp (vt StTy:$rs2), BaseAddr:$rs1),
            (Inst StTy:$rs2, BaseAddr:$rs1, 0)>;
  def : Pat<(StoreOp (vt StTy:$rs2), (add BaseAddr:$rs1, simm12:$imm12)),
            (Inst StTy:$rs2, BaseAddr:$rs1, simm12:$imm12)>;
  def : Pat<(StoreOp (vt StTy:$rs2), (IsOrAdd AddrFI:$rs1, simm12:$imm12)),
            (Inst StTy:$rs2, AddrFI:$rs1, simm12:$imm12)>;
}}

defm : StPat<truncstorei8, SB, GPR, XLenVT>;
defm : StPat<truncstorei16, SH, GPR, XLenVT>;
defm : StPat<store, SW, GPR, i32>, Requires<[IsPR32]>;
}} // Predicates = [HasFullI]

/// Fences

// Refer to Table A.6 in the version 2.3 draft of the Primate Instruction Set
// Manual: Volume I.

let Predicates = [HasFullI] in {{
// fence acquire -> fence r, rw
def : Pat<(atomic_fence (XLenVT 4), (timm)), (FENCE 0b10, 0b11)>;
// fence release -> fence rw, w
def : Pat<(atomic_fence (XLenVT 5), (timm)), (FENCE 0b11, 0b1)>;
// fence acq_rel -> fence.tso
def : Pat<(atomic_fence (XLenVT 6), (timm)), (FENCE_TSO)>;
// fence seq_cst -> fence rw, rw
def : Pat<(atomic_fence (XLenVT 7), (timm)), (FENCE 0b11, 0b11)>;
}} // Predicates = [HasFullI]

// Lowering for atomic load and store is defined in PrimateInstrInfoA.td.
// Although these are lowered to fence+load/store instructions defined in the
// base PR32I/PR64I ISA, this lowering is only used when the A extension is
// present. This is necessary as it isn't valid to mix __atomic_* libcalls
// with inline atomic operations for the same object.

/// Access to system registers

// Helpers for defining specific operations. They are defined for each system
// register separately. Side effect is not used because dependencies are
// expressed via use-def properties.

let Predicates = [HasFullI] in {{
class ReadSysReg<SysReg SR, list<Register> Regs>
  : Pseudo<(outs GPR:$rd), (ins),
           [(set (XLenVT GPR:$rd), (primate_read_csr (XLenVT SR.Encoding)))]>,
    PseudoInstExpansion<(CSRRS GPR:$rd, SR.Encoding, X0)> {{
  let hasSideEffects = 0;
  let Uses = Regs;
}}

class WriteSysReg<SysReg SR, list<Register> Regs>
  : Pseudo<(outs), (ins GPR:$val),
           [(primate_write_csr (XLenVT SR.Encoding), (XLenVT GPR:$val))]>,
    PseudoInstExpansion<(CSRRW (XLenVT X0), SR.Encoding, GPR:$val)> {{
  let hasSideEffects = 0;
  let Defs = Regs;
}}

class WriteSysRegImm<SysReg SR, list<Register> Regs>
  : Pseudo<(outs), (ins uimm5:$val),
           [(primate_write_csr (XLenVT SR.Encoding), uimm5:$val)]>,
    PseudoInstExpansion<(CSRRWI (XLenVT X0), SR.Encoding, uimm5:$val)> {{
  let hasSideEffects = 0;
  let Defs = Regs;
}}

class SwapSysReg<SysReg SR, list<Register> Regs>
  : Pseudo<(outs GPR:$rd), (ins GPR:$val),
           [(set (XLenVT GPR:$rd), (primate_swap_csr (XLenVT SR.Encoding), (XLenVT GPR:$val)))]>,
    PseudoInstExpansion<(CSRRW GPR:$rd, SR.Encoding, GPR:$val)> {{
  let hasSideEffects = 0;
  let Uses = Regs;
  let Defs = Regs;
}}

class SwapSysRegImm<SysReg SR, list<Register> Regs>
  : Pseudo<(outs GPR:$rd), (ins uimm5:$val),
           [(set (XLenVT GPR:$rd), (primate_swap_csr (XLenVT SR.Encoding), uimm5:$val))]>,
    PseudoInstExpansion<(CSRRWI GPR:$rd, SR.Encoding, uimm5:$val)> {{
  let hasSideEffects = 0;
  let Uses = Regs;
  let Defs = Regs;
}}

def ReadFRM : ReadSysReg<SysRegFRM, [FRM]>;
def WriteFRM : WriteSysReg<SysRegFRM, [FRM]>;
def WriteFRMImm : WriteSysRegImm<SysRegFRM, [FRM]>;
}} // Predicates = [HasFullI]

/// Other pseudo-instructions

// Pessimistically assume the stack pointer will be clobbered
let Defs = [X2], Uses = [X2] in {{
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                              [(callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                              [(callseq_end timm:$amt1, timm:$amt2)]>;
}} // Defs = [X2], Uses = [X2]

/// PR64 patterns

let Predicates = [IsPR64, NotHasStdExtZba] in {{
def : Pat<(i64 (and GPR:$rs1, 0xffffffff)), (SRLI (SLLI GPR:$rs1, 32), 32)>;

// If we're shifting a 32-bit zero extended value left by 0-31 bits, use 2
// shifts instead of 3. This can occur when unsigned is used to index an array.
def : Pat<(i64 (shl (and GPR:$rs1, 0xffffffff), uimm5:$shamt)),
          (SRLI (SLLI GPR:$rs1, 32), (ImmSubFrom32 uimm5:$shamt))>;
}}

let Predicates = [IsPR64] in {{

/// sext and zext

def : Pat<(sext_inreg GPR:$rs1, i32), (ADDIW GPR:$rs1, 0)>;

/// ALU operations

def : Pat<(sext_inreg (add GPR:$rs1, GPR:$rs2), i32),
          (ADDW GPR:$rs1, GPR:$rs2)>;
def : Pat<(sext_inreg (add GPR:$rs1, simm12:$imm12), i32),
          (ADDIW GPR:$rs1, simm12:$imm12)>;
def : Pat<(sext_inreg (sub GPR:$rs1, GPR:$rs2), i32),
          (SUBW GPR:$rs1, GPR:$rs2)>;
def : Pat<(sext_inreg (shl GPR:$rs1, uimm5:$shamt), i32),
          (SLLIW GPR:$rs1, uimm5:$shamt)>;
def : Pat<(i64 (srl (and GPR:$rs1, 0xffffffff), uimm5:$shamt)),
          (SRLIW GPR:$rs1, uimm5:$shamt)>;
def : Pat<(i64 (srl (shl GPR:$rs1, (i64 32)), uimm6gt32:$shamt)),
          (SRLIW GPR:$rs1, (ImmSub32 uimm6gt32:$shamt))>;
def : Pat<(sra (sext_inreg GPR:$rs1, i32), uimm5:$shamt),
          (SRAIW GPR:$rs1, uimm5:$shamt)>;
def : Pat<(i64 (sra (shl GPR:$rs1, (i64 32)), uimm6gt32:$shamt)),
          (SRAIW GPR:$rs1, (ImmSub32 uimm6gt32:$shamt))>;

def : PatGprGpr<shiftopw<primate_sllw>, SLLW>;
def : PatGprGpr<shiftopw<primate_srlw>, SRLW>;
def : PatGprGpr<shiftopw<primate_sraw>, SRAW>;

/// Loads

let Predicates = [HasFullI] in {{
defm : LdPat<sextloadi32, LW, i64>;
defm : LdPat<extloadi32, LW, i64>;
defm : LdPat<zextloadi32, LWU, i64>;
defm : LdPat<load, LD, i64>;
}} // Predicates = [HasFullI]

/// Stores

let Predicates = [HasFullI] in {{
defm : StPat<truncstorei32, SW, GPR, i64>;
defm : StPat<store, SD, GPR, i64>;
}} // Predicates = [HasFullI]
}} // Predicates = [IsPR64]

let Predicates = [HasFullI] in {{
/// readcyclecounter
// On PR64, we can directly read the 64-bit "cycle" CSR.
let Predicates = [IsPR64] in
def : Pat<(i64 (readcyclecounter)), (CSRRS CYCLE.Encoding, (XLenVT X0))>;
// On PR32, ReadCycleWide will be expanded to the suggested loop reading both
// halves of the 64-bit "cycle" CSR.
let Predicates = [IsPR32], usesCustomInserter = 1, hasNoSchedulingInfo = 1 in
def ReadCycleWide : Pseudo<(outs GPR:$lo, GPR:$hi), (ins),
                           [(set GPR:$lo, GPR:$hi, (primate_read_cycle_wide))],
                           "", "">;
}} // Predicates = [HasFullI]

/// traps

// We lower `trap` to `unimp`, as this causes a hard exception on nearly all
// systems.
def : Pat<(trap), (UNIMP)>;

// We lower `debugtrap` to `ebreak`, as this will get the attention of the
// debugger if possible.
let Predicates = [HasFullI] in
def : Pat<(debugtrap), (EBREAK)>;

/// Simple optimization
def : Pat<(XLenVT (add (XLenVT GPR:$rs1), (XLenVT AddiPair:$rs2))),
          (ADDI (ADDI GPR:$rs1, (AddiPairImmB AddiPair:$rs2)),
                (AddiPairImmA GPR:$rs2))>;

let Predicates = [IsPR64] in {{
def : Pat<(sext_inreg (add_oneuse GPR:$rs1, (AddiPair:$rs2)), i32),
          (ADDIW (ADDIW GPR:$rs1, (AddiPairImmB AddiPair:$rs2)),
                 (AddiPairImmA AddiPair:$rs2))>;
}}

//===----------------------------------------------------------------------===//
// Experimental PR64 i32 legalization patterns.
//===----------------------------------------------------------------------===//

def simm12i32 : ImmLeaf<i32, [{{return isInt<12>(Imm);}}]>;

// Convert from i32 immediate to i64 target immediate to make SelectionDAG type
// checking happy so we can use ADDIW which expects an XLen immediate.
def as_i64imm : SDNodeXForm<imm, [{{
  return CurDAG->getTargetConstant(N->getSExtValue(), SDLoc(N), MVT::i64);
}}]>;

def zext_is_sext : PatFrag<(ops node:$src), (zext node:$src), [{{
  KnownBits Known = CurDAG->computeKnownBits(N->getOperand(0), 0);
  return Known.isNonNegative();
}}]>;

let Predicates = [IsPR64] in {{
def : LdPat<sextloadi8, LB, i32>;
def : LdPat<extloadi8, LBU, i32>; // Prefer unsigned due to no c.lb in Zcb.
def : LdPat<sextloadi16, LH, i32>;
def : LdPat<extloadi16, LH, i32>;
def : LdPat<zextloadi8, LBU, i32>;
def : LdPat<zextloadi16, LHU, i32>;

def : StPat<truncstorei8, SB, GPR, i32>;
def : StPat<truncstorei16, SH, GPR, i32>;

def : Pat<(i64 (anyext (i32 GPR:$src))), (COPY GPR:$src)>;
def : Pat<(i64 (sext (i32 GPR:$src))), (ADDIW GPR:$src, 0)>;
def : Pat<(i32 (trunc (i64 GPR:$src))), (COPY GPR:$src)>;

def : PatGprGpr<add, ADDW, i32, i32>;
def : PatGprGpr<sub, SUBW, i32, i32>;
def : PatGprGpr<and, AND, i32, i32>;
def : PatGprGpr<or, OR, i32, i32>;
def : PatGprGpr<xor, XOR, i32, i32>;
def : PatGprGpr<shiftopw<shl>, SLLW, i32, i64>;
def : PatGprGpr<shiftopw<srl>, SRLW, i32, i64>;
def : PatGprGpr<shiftopw<sra>, SRAW, i32, i64>;

def : Pat<(i32 (add GPR:$rs1, simm12i32:$imm)),
          (ADDIW GPR:$rs1, (i64 (as_i64imm $imm)))>;
def : Pat<(i32 (and GPR:$rs1, simm12i32:$imm)),
          (ANDI GPR:$rs1, (i64 (as_i64imm $imm)))>;
def : Pat<(i32 (or GPR:$rs1, simm12i32:$imm)),
          (ORI GPR:$rs1, (i64 (as_i64imm $imm)))>;
def : Pat<(i32 (xor GPR:$rs1, simm12i32:$imm)),
          (XORI GPR:$rs1, (i64 (as_i64imm $imm)))>;

def : PatGprImm<shl, SLLIW, uimm5, i32>;
def : PatGprImm<srl, SRLIW, uimm5, i32>;
def : PatGprImm<sra, SRAIW, uimm5, i32>;

def : Pat<(i32 (and GPR:$rs, TrailingOnesMask:$mask)),
          (SRLI (SLLI $rs, (i64 (XLenSubTrailingOnes $mask))),
                (i64 (XLenSubTrailingOnes $mask)))>;

// Use sext if the sign bit of the input is 0.
def : Pat<(zext_is_sext GPR:$src), (ADDIW GPR:$src, 0)>;
}}

let Predicates = [IsPR64, NotHasStdExtZba] in {{
def : Pat<(zext GPR:$src), (SRLI (SLLI GPR:$src, 32), 32)>;

// If we're shifting a 32-bit zero extended value left by 0-31 bits, use 2
// shifts instead of 3. This can occur when unsigned is used to index an array.
def : Pat<(shl (zext GPR:$rs), uimm5:$shamt),
          (SRLI (SLLI GPR:$rs, 32), (ImmSubFrom32 uimm5:$shamt))>;
}}

//===----------------------------------------------------------------------===//
// Standard extensions
//===----------------------------------------------------------------------===//

include "PrimateInstrInfoM.td"
include "PrimateInstrInfoA.td"
include "PrimateInstrInfoF.td"
include "PrimateInstrInfoD.td"
include "PrimateInstrInfoC.td"
include "PrimateInstrInfoB.td"
include "PrimateInstrInfoZfh.td"
"""

with open(os.path.join(gen_file_dir, "./PrimateInstrInfo.td"), "w") as f:
    print(PrimateInstrInfo, file=f)
    
    
PrimateIntrinsDefTemplate = """def int_primate_BFU_{0} :  Intrinsic<[llvm_any_ty], // return val
                  [llvm_any_ty], // Params: gpr w/ struct
		              [IntrHasSideEffects]>; // properties;
                """
PrimateIntrinsDef = combStr(PrimateIntrinsDefTemplate, max(numBFUs-2, 1))
    
IntrinsicsPrimate = f"""
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

let TargetPrefix = "primate" in {{

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
  multiclass PrimateMaskedAtomicRMWFourArgIntrinsics {{
    // i32 @llvm.<name>.i32.<p>(any*, i32, i32, i32 imm);
    def _i32 : PrimateMaskedAtomicRMWFourArg<llvm_i32_ty>;
    // i64 @llvm.<name>.i32.<p>(any*, i64, i64, i64 imm);
    def _i64 : PrimateMaskedAtomicRMWFourArg<llvm_i64_ty>;
  }}

  multiclass PrimateMaskedAtomicRMWFiveArgIntrinsics {{
    // i32 @llvm.<name>.i32.<p>(any*, i32, i32, i32, i32 imm);
    def _i32 : PrimateMaskedAtomicRMWFiveArg<llvm_i32_ty>;
    // i64 @llvm.<name>.i64.<p>(any*, i64, i64, i64, i64 imm);
    def _i64 : PrimateMaskedAtomicRMWFiveArg<llvm_i64_ty>;
  }}

  // @llvm.primate.masked.atomicrmw.*.{{i32,i64}}.<p>(...)
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

  // @llvm.primate.masked.cmpxchg.{{i32,i64}}.<p>(...)
  defm int_primate_masked_cmpxchg : PrimateMaskedAtomicRMWFiveArgIntrinsics;

}} // TargetPrefix = "primate"

//===----------------------------------------------------------------------===//
// Bitmanip (Bit Manipulation) Extension

let TargetPrefix = "primate" in {{

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
  def int_primate_input :  Intrinsic<[llvm_any_ty], // return val
                  [llvm_i32_ty], // Params: imm12
		              [IntrNoMem, IntrHasSideEffects]>; // properties;
                
  def int_primate_input_done :  Intrinsic<[], // return val
                  [], // Params: imm12
		              [IntrNoMem, IntrHasSideEffects]>; // properties;
    
  def int_primate_output :  Intrinsic<[], // return val
                  [llvm_any_ty, llvm_i32_ty], // Params: imm12
		              [IntrNoMem, IntrHasSideEffects]>; // properties;
                
  def int_primate_output_done :  Intrinsic<[], // return val
                  [], // Params: imm12
		              [IntrNoMem, IntrHasSideEffects]>; // properties;
  
  {PrimateIntrinsDef}

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

}} // TargetPrefix = "primate"

//===----------------------------------------------------------------------===//
// Vectors

class PrimateVIntrinsic {{
  // These intrinsics may accept illegal integer values in their llvm_any_ty
  // operand, so they have to be extended. If set to zero then the intrinsic
  // does not have any operand that must be extended.
  Intrinsic IntrinsicID = !cast<Intrinsic>(NAME);
  bits<4> SplatOperand = 0;
}}

let TargetPrefix = "primate" in {{
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
                    [llvm_ptr_ty,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride fault-only-first load
  // Input: (pointer, vl)
  // Output: (data, vl)
  // NOTE: We model this with default memory properties since we model writing
  // VL as a side effect. IntrReadMem, IntrHasSideEffects does not work.
  class PrimateUSLoadFF
        : Intrinsic<[llvm_anyvector_ty, llvm_anyint_ty],
                    [llvm_ptr_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>]>,
                    PrimateVIntrinsic;
  // For unit stride load with mask
  // Input: (maskedoff, pointer, mask, vl)
  class PrimateUSLoadMask
        : Intrinsic<[llvm_anyvector_ty ],
                    [LLVMMatchType<0>,
                     llvm_ptr_ty,
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
                     llvm_ptr_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>]>, PrimateVIntrinsic;
  // For strided load
  // Input: (pointer, stride, vl)
  class PrimateSLoad
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_ptr_ty,
                     llvm_anyint_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For strided load with mask
  // Input: (maskedoff, pointer, stride, mask, vl)
  class PrimateSLoadMask
        : Intrinsic<[llvm_anyvector_ty ],
                    [LLVMMatchType<0>,
                     llvm_ptr_ty, llvm_anyint_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>, IntrReadMem]>, PrimateVIntrinsic;
  // For indexed load
  // Input: (pointer, index, vl)
  class PrimateILoad
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_ptr_ty,
                     llvm_anyvector_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For indexed load with mask
  // Input: (maskedoff, pointer, index, mask, vl)
  class PrimateILoadMask
        : Intrinsic<[llvm_anyvector_ty ],
                    [LLVMMatchType<0>,
                     llvm_ptr_ty, llvm_anyvector_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride store
  // Input: (vector_in, pointer, vl)
  class PrimateUSStore
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     llvm_ptr_ty,
                     LLVMMatchType<0>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For unit stride store with mask
  // Input: (vector_in, pointer, mask, vl)
  class PrimateUSStoreMask
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     llvm_ptr_ty,
                     LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For strided store
  // Input: (vector_in, pointer, stride, vl)
  class PrimateSStore
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     llvm_ptr_ty,
                     LLVMMatchType<0>,
                     llvm_anyint_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For stride store with mask
  // Input: (vector_in, pointer, stirde, mask, vl)
  class PrimateSStoreMask
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     llvm_ptr_ty, LLVMMatchType<0>, llvm_anyint_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For indexed store
  // Input: (vector_in, pointer, index, vl)
  class PrimateIStore
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     llvm_ptr_ty,
                     LLVMMatchType<0>,
                     llvm_anyint_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<1>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For indexed store with mask
  // Input: (vector_in, pointer, index, mask, vl)
  class PrimateIStoreMask
        : Intrinsic<[],
                    [llvm_anyvector_ty,
                     llvm_ptr_ty, LLVMMatchType<0>, llvm_anyvector_ty,
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
                    [IntrNoMem]>, PrimateVIntrinsic {{
  }}
  // For destination vector type is the same as first source vector (with mask).
  // Second operand is XLen.
  // Input: (maskedoff, vector_in, xlen_in, mask, vl)
  class PrimateGatherVXMask
       : Intrinsic<[llvm_anyvector_ty],
                   [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyint_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, LLVMMatchType<1>],
                   [IntrNoMem]>, PrimateVIntrinsic {{
  }}
  // For destination vector type is the same as first source vector.
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryAAXNoMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For destination vector type is the same as first source vector (with mask).
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateBinaryAAXMask
       : Intrinsic<[llvm_anyvector_ty],
                   [LLVMMatchType<0>, LLVMMatchType<0>, llvm_any_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                   [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 3;
  }}
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
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For destination vector type is NOT the same as first source vector (with mask).
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateBinaryABXMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_anyvector_ty, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 3;
  }}
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
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For binary operations with mask type output and V0 as input.
  // Output: (mask type output)
  // Input: (vector_in, vector_in/scalar_in, V0, vl)
  class PrimateBinaryMOutWithV0
        :Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                   [llvm_anyvector_ty, llvm_any_ty,
                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                    llvm_anyint_ty],
                   [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For binary operations with mask type output.
  // Output: (mask type output)
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateBinaryMOut
        : Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For binary operations with mask type output without mask.
  // Output: (mask type output)
  // Input: (vector_in, vector_in/scalar_in, vl)
  class PrimateCompareNoMask
        : Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                    [llvm_anyvector_ty, llvm_any_ty, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For binary operations with mask type output with mask.
  // Output: (mask type output)
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateCompareMask
        : Intrinsic<[LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
                    [LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                     llvm_anyvector_ty, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 3;
  }}
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
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  // For Saturating binary operations with mask.
  // The destination vector type is the same as first source vector.
  // Input: (maskedoff, vector_in, vector_in/scalar_in, mask, vl)
  class PrimateSaturatingBinaryAAXMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, LLVMMatchType<0>, llvm_any_ty,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem, IntrHasSideEffects]>, PrimateVIntrinsic {{
    let SplatOperand = 3;
  }}
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
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  class PrimateTernaryAAXAMask
        : Intrinsic<[llvm_anyvector_ty],
                    [LLVMMatchType<0>, llvm_any_ty, LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  class PrimateTernaryWideNoMask
        : Intrinsic< [llvm_anyvector_ty],
                     [LLVMMatchType<0>, llvm_any_ty, llvm_anyvector_ty,
                      llvm_anyint_ty],
                     [IntrNoMem] >, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
  class PrimateTernaryWideMask
        : Intrinsic< [llvm_anyvector_ty],
                     [LLVMMatchType<0>, llvm_any_ty, llvm_anyvector_ty,
                      LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                     [IntrNoMem]>, PrimateVIntrinsic {{
    let SplatOperand = 2;
  }}
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
                    [llvm_ptr_ty, LLVMMatchType<0>, llvm_anyvector_ty, LLVMMatchType<0>,
                     llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>]>, PrimateVIntrinsic;
  // For atomic operations with mask
  // Input: (base, index, value, mask, vl)
  class PrimateAMOMask
        : Intrinsic<[llvm_anyvector_ty],
                    [llvm_ptr_ty, LLVMMatchType<0>, llvm_anyvector_ty, LLVMMatchType<0>,
                     LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>]>, PrimateVIntrinsic;

  // For unit stride segment load
  // Input: (pointer, vl)
  class PrimateUSSegLoad<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    [llvm_ptr_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For unit stride segment load with mask
  // Input: (maskedoff, pointer, mask, vl)
  class PrimateUSSegLoadMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                                [llvm_ptr_ty,
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
                    [llvm_ptr_ty, LLVMMatchType<1>],
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
                     [llvm_ptr_ty,
                      LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                      LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>]>, PrimateVIntrinsic;

  // For stride segment load
  // Input: (pointer, offset, vl)
  class PrimateSSegLoad<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    [llvm_ptr_ty, llvm_anyint_ty, LLVMMatchType<1>],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For stride segment load with mask
  // Input: (maskedoff, pointer, offset, mask, vl)
  class PrimateSSegLoadMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                                [llvm_ptr_ty,
                                 llvm_anyint_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>, IntrReadMem]>, PrimateVIntrinsic;

  // For indexed segment load
  // Input: (pointer, index, vl)
  class PrimateISegLoad<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    [llvm_ptr_ty, llvm_anyvector_ty, llvm_anyint_ty],
                    [NoCapture<ArgIndex<0>>, IntrReadMem]>, PrimateVIntrinsic;
  // For indexed segment load with mask
  // Input: (maskedoff, pointer, index, mask, vl)
  class PrimateISegLoadMask<int nf>
        : Intrinsic<!listconcat([llvm_anyvector_ty], !listsplat(LLVMMatchType<0>,
                                !add(nf, -1))),
                    !listconcat(!listsplat(LLVMMatchType<0>, nf),
                                [llvm_ptr_ty,
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
                                [llvm_ptr_ty, llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For unit stride segment store with mask
  // Input: (value, pointer, mask, vl)
  class PrimateUSSegStoreMask<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [llvm_ptr_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;

  // For stride segment store
  // Input: (value, pointer, offset, vl)
  class PrimateSSegStore<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [llvm_ptr_ty, llvm_anyint_ty,
                                 LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For stride segment store with mask
  // Input: (value, pointer, offset, mask, vl)
  class PrimateSSegStoreMask<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [llvm_ptr_ty, llvm_anyint_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 LLVMMatchType<1>]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;

  // For indexed segment store
  // Input: (value, pointer, offset, vl)
  class PrimateISegStore<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [llvm_ptr_ty, llvm_anyvector_ty,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;
  // For indexed segment store with mask
  // Input: (value, pointer, offset, mask, vl)
  class PrimateISegStoreMask<int nf>
        : Intrinsic<[],
                    !listconcat([llvm_anyvector_ty],
                                !listsplat(LLVMMatchType<0>, !add(nf, -1)),
                                [llvm_ptr_ty, llvm_anyvector_ty,
                                 LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                 llvm_anyint_ty]),
                    [NoCapture<ArgIndex<nf>>, IntrWriteMem]>, PrimateVIntrinsic;

  multiclass PrimateUSLoad {{
    def "int_primate_" # NAME : PrimateUSLoad;
    def "int_primate_" # NAME # "_mask" : PrimateUSLoadMask;
  }}
  multiclass PrimateUSLoadFF {{
    def "int_primate_" # NAME : PrimateUSLoadFF;
    def "int_primate_" # NAME # "_mask" : PrimateUSLoadFFMask;
  }}
  multiclass PrimateSLoad {{
    def "int_primate_" # NAME : PrimateSLoad;
    def "int_primate_" # NAME # "_mask" : PrimateSLoadMask;
  }}
  multiclass PrimateILoad {{
    def "int_primate_" # NAME : PrimateILoad;
    def "int_primate_" # NAME # "_mask" : PrimateILoadMask;
  }}
  multiclass PrimateUSStore {{
    def "int_primate_" # NAME : PrimateUSStore;
    def "int_primate_" # NAME # "_mask" : PrimateUSStoreMask;
  }}
  multiclass PrimateSStore {{
    def "int_primate_" # NAME : PrimateSStore;
    def "int_primate_" # NAME # "_mask" : PrimateSStoreMask;
  }}

  multiclass PrimateIStore {{
    def "int_primate_" # NAME : PrimateIStore;
    def "int_primate_" # NAME # "_mask" : PrimateIStoreMask;
  }}
  multiclass PrimateUnaryAA {{
    def "int_primate_" # NAME : PrimateUnaryAANoMask;
    def "int_primate_" # NAME # "_mask" : PrimateUnaryAAMask;
  }}
  multiclass PrimateUnaryAB {{
    def "int_primate_" # NAME : PrimateUnaryABNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateUnaryABMask;
  }}
  // AAX means the destination type(A) is the same as the first source
  // type(A). X means any type for the second source operand.
  multiclass PrimateBinaryAAX {{
    def "int_primate_" # NAME : PrimateBinaryAAXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryAAXMask;
  }}
  // Like PrimateBinaryAAX, but the second operand is used a shift amount so it
  // must be a vector or an XLen scalar.
  multiclass PrimateBinaryAAShift {{
    def "int_primate_" # NAME : PrimateBinaryAAShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryAAShiftMask;
  }}
  multiclass PrimateRGatherVV {{
    def "int_primate_" # NAME : PrimateRGatherVVNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateRGatherVVMask;
  }}
  multiclass PrimateRGatherVX {{
    def "int_primate_" # NAME : PrimateGatherVXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateGatherVXMask;
  }}
  multiclass PrimateRGatherEI16VV {{
    def "int_primate_" # NAME : PrimateRGatherEI16VVNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateRGatherEI16VVMask;
  }}
  // ABX means the destination type(A) is different from the first source
  // type(B). X means any type for the second source operand.
  multiclass PrimateBinaryABX {{
    def "int_primate_" # NAME : PrimateBinaryABXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryABXMask;
  }}
  // Like PrimateBinaryABX, but the second operand is used a shift amount so it
  // must be a vector or an XLen scalar.
  multiclass PrimateBinaryABShift {{
    def "int_primate_" # NAME : PrimateBinaryABShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateBinaryABShiftMask;
  }}
  multiclass PrimateBinaryWithV0 {{
    def "int_primate_" # NAME : PrimateBinaryWithV0;
  }}
  multiclass PrimateBinaryMaskOutWithV0 {{
    def "int_primate_" # NAME : PrimateBinaryMOutWithV0;
  }}
  multiclass PrimateBinaryMaskOut {{
    def "int_primate_" # NAME : PrimateBinaryMOut;
  }}
  multiclass PrimateSaturatingBinaryAAX {{
    def "int_primate_" # NAME : PrimateSaturatingBinaryAAXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateSaturatingBinaryAAXMask;
  }}
  multiclass PrimateSaturatingBinaryAAShift {{
    def "int_primate_" # NAME : PrimateSaturatingBinaryAAShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateSaturatingBinaryAAShiftMask;
  }}
  multiclass PrimateSaturatingBinaryABShift {{
    def "int_primate_" # NAME : PrimateSaturatingBinaryABShiftNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateSaturatingBinaryABShiftMask;
  }}
  multiclass PrimateTernaryAAAX {{
    def "int_primate_" # NAME : PrimateTernaryAAAXNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateTernaryAAAXMask;
  }}
  multiclass PrimateTernaryAAXA {{
    def "int_primate_" # NAME : PrimateTernaryAAXANoMask;
    def "int_primate_" # NAME # "_mask" : PrimateTernaryAAXAMask;
  }}
  multiclass PrimateCompare {{
    def "int_primate_" # NAME : PrimateCompareNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateCompareMask;
  }}
  multiclass PrimateClassify {{
    def "int_primate_" # NAME : PrimateClassifyNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateClassifyMask;
  }}
  multiclass PrimateTernaryWide {{
    def "int_primate_" # NAME : PrimateTernaryWideNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateTernaryWideMask;
  }}
  multiclass PrimateReduction {{
    def "int_primate_" # NAME : PrimateReductionNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateReductionMask;
  }}
  multiclass PrimateMaskUnarySOut {{
    def "int_primate_" # NAME : PrimateMaskUnarySOutNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateMaskUnarySOutMask;
  }}
  multiclass PrimateMaskUnaryMOut {{
    def "int_primate_" # NAME : PrimateUnaryNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateMaskUnaryMOutMask;
  }}
  multiclass PrimateConversion {{
    def "int_primate_" #NAME :PrimateConversionNoMask;
    def "int_primate_" # NAME # "_mask" : PrimateConversionMask;
  }}
  multiclass PrimateAMO {{
    def "int_primate_" # NAME : PrimateAMONoMask;
    def "int_primate_" # NAME # "_mask" : PrimateAMOMask;
  }}
  multiclass PrimateUSSegLoad<int nf> {{
    def "int_primate_" # NAME : PrimateUSSegLoad<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateUSSegLoadMask<nf>;
  }}
  multiclass PrimateUSSegLoadFF<int nf> {{
    def "int_primate_" # NAME : PrimateUSSegLoadFF<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateUSSegLoadFFMask<nf>;
  }}
  multiclass PrimateSSegLoad<int nf> {{
    def "int_primate_" # NAME : PrimateSSegLoad<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateSSegLoadMask<nf>;
  }}
  multiclass PrimateISegLoad<int nf> {{
    def "int_primate_" # NAME : PrimateISegLoad<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateISegLoadMask<nf>;
  }}
  multiclass PrimateUSSegStore<int nf> {{
    def "int_primate_" # NAME : PrimateUSSegStore<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateUSSegStoreMask<nf>;
  }}
  multiclass PrimateSSegStore<int nf> {{
    def "int_primate_" # NAME : PrimateSSegStore<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateSSegStoreMask<nf>;
  }}
  multiclass PrimateISegStore<int nf> {{
    def "int_primate_" # NAME : PrimateISegStore<nf>;
    def "int_primate_" # NAME # "_mask" : PrimateISegStoreMask<nf>;
  }}

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

  foreach nf = [2, 3, 4, 5, 6, 7, 8] in {{
    defm vlseg # nf : PrimateUSSegLoad<nf>;
    defm vlseg # nf # ff : PrimateUSSegLoadFF<nf>;
    defm vlsseg # nf : PrimateSSegLoad<nf>;
    defm vloxseg # nf : PrimateISegLoad<nf>;
    defm vluxseg # nf : PrimateISegLoad<nf>;
    defm vsseg # nf : PrimateUSSegStore<nf>;
    defm vssseg # nf : PrimateSSegStore<nf>;
    defm vsoxseg # nf : PrimateISegStore<nf>;
    defm vsuxseg # nf : PrimateISegStore<nf>;
  }}

}} // TargetPrefix = "primate"
"""

with open(os.path.join(gen_file_dir, "./IntrinsicsPrimate.td"), "w") as f:
    print(IntrinsicsPrimate, file=f)
