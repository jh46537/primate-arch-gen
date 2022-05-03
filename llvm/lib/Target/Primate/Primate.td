//===-- Primate.td - Describe the Primate Target Machine ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Primate subtarget features and instruction predicates.
//===----------------------------------------------------------------------===//

def FeatureStdExtM
    : SubtargetFeature<"m", "HasStdExtM", "true",
                       "'M' (Integer Multiplication and Division)">;
def HasStdExtM : Predicate<"Subtarget->hasStdExtM()">,
                           AssemblerPredicate<(all_of FeatureStdExtM),
                           "'M' (Integer Multiplication and Division)">;

def FeatureStdExtA
    : SubtargetFeature<"a", "HasStdExtA", "true",
                       "'A' (Atomic Instructions)">;
def HasStdExtA : Predicate<"Subtarget->hasStdExtA()">,
                           AssemblerPredicate<(all_of FeatureStdExtA),
                           "'A' (Atomic Instructions)">;

def FeatureStdExtF
    : SubtargetFeature<"f", "HasStdExtF", "true",
                       "'F' (Single-Precision Floating-Point)">;
def HasStdExtF : Predicate<"Subtarget->hasStdExtF()">,
                           AssemblerPredicate<(all_of FeatureStdExtF),
                           "'F' (Single-Precision Floating-Point)">;

def FeatureStdExtD
    : SubtargetFeature<"d", "HasStdExtD", "true",
                       "'D' (Double-Precision Floating-Point)",
                       [FeatureStdExtF]>;
def HasStdExtD : Predicate<"Subtarget->hasStdExtD()">,
                           AssemblerPredicate<(all_of FeatureStdExtD),
                           "'D' (Double-Precision Floating-Point)">;

def FeatureExtZfh
    : SubtargetFeature<"experimental-zfh", "HasStdExtZfh", "true",
                       "'Zfh' (Half-Precision Floating-Point)",
                       [FeatureStdExtF]>;
def HasStdExtZfh : Predicate<"Subtarget->hasStdExtZfh()">,
                             AssemblerPredicate<(all_of FeatureExtZfh),
                             "'Zfh' (Half-Precision Floating-Point)">;

def FeatureStdExtC
    : SubtargetFeature<"c", "HasStdExtC", "true",
                       "'C' (Compressed Instructions)">;
def HasStdExtC : Predicate<"Subtarget->hasStdExtC()">,
                           AssemblerPredicate<(all_of FeatureStdExtC),
                           "'C' (Compressed Instructions)">;

def FeatureExtZba
    : SubtargetFeature<"experimental-zba", "HasStdExtZba", "true",
                       "'Zba' (Address calculation 'B' Instructions)">;
def HasStdExtZba : Predicate<"Subtarget->hasStdExtZba()">,
                             AssemblerPredicate<(all_of FeatureExtZba),
                             "'Zba' (Address calculation 'B' Instructions)">;
def NotHasStdExtZba : Predicate<"!Subtarget->hasStdExtZba()">;

def FeatureExtZbb
    : SubtargetFeature<"experimental-zbb", "HasStdExtZbb", "true",
                       "'Zbb' (Base 'B' Instructions)">;
def HasStdExtZbb : Predicate<"Subtarget->hasStdExtZbb()">,
                             AssemblerPredicate<(all_of FeatureExtZbb),
                             "'Zbb' (Base 'B' Instructions)">;

def FeatureExtZbc
    : SubtargetFeature<"experimental-zbc", "HasStdExtZbc", "true",
                       "'Zbc' (Carry-Less 'B' Instructions)">;
def HasStdExtZbc : Predicate<"Subtarget->hasStdExtZbc()">,
                             AssemblerPredicate<(all_of FeatureExtZbc),
                             "'Zbc' (Carry-Less 'B' Instructions)">;

def FeatureExtZbe
    : SubtargetFeature<"experimental-zbe", "HasStdExtZbe", "true",
                       "'Zbe' (Extract-Deposit 'B' Instructions)">;
def HasStdExtZbe : Predicate<"Subtarget->hasStdExtZbe()">,
                             AssemblerPredicate<(all_of FeatureExtZbe),
                             "'Zbe' (Extract-Deposit 'B' Instructions)">;

def FeatureExtZbf
    : SubtargetFeature<"experimental-zbf", "HasStdExtZbf", "true",
                       "'Zbf' (Bit-Field 'B' Instructions)">;
def HasStdExtZbf : Predicate<"Subtarget->hasStdExtZbf()">,
                             AssemblerPredicate<(all_of FeatureExtZbf),
                             "'Zbf' (Bit-Field 'B' Instructions)">;

def FeatureExtZbm
    : SubtargetFeature<"experimental-zbm", "HasStdExtZbm", "true",
                       "'Zbm' (Matrix 'B' Instructions)">;
def HasStdExtZbm : Predicate<"Subtarget->hasStdExtZbm()">,
                             AssemblerPredicate<(all_of FeatureExtZbm),
                             "'Zbm' (Matrix 'B' Instructions)">;

def FeatureExtZbp
    : SubtargetFeature<"experimental-zbp", "HasStdExtZbp", "true",
                       "'Zbp' (Permutation 'B' Instructions)">;
def HasStdExtZbp : Predicate<"Subtarget->hasStdExtZbp()">,
                             AssemblerPredicate<(all_of FeatureExtZbp),
                             "'Zbp' (Permutation 'B' Instructions)">;

def FeatureExtZbr
    : SubtargetFeature<"experimental-zbr", "HasStdExtZbr", "true",
                       "'Zbr' (Polynomial Reduction 'B' Instructions)">;
def HasStdExtZbr : Predicate<"Subtarget->hasStdExtZbr()">,
                             AssemblerPredicate<(all_of FeatureExtZbr),
                             "'Zbr' (Polynomial Reduction 'B' Instructions)">;

def FeatureExtZbs
    : SubtargetFeature<"experimental-zbs", "HasStdExtZbs", "true",
                       "'Zbs' (Single-Bit 'B' Instructions)">;
def HasStdExtZbs : Predicate<"Subtarget->hasStdExtZbs()">,
                             AssemblerPredicate<(all_of FeatureExtZbs),
                             "'Zbs' (Single-Bit 'B' Instructions)">;

def FeatureExtZbt
    : SubtargetFeature<"experimental-zbt", "HasStdExtZbt", "true",
                       "'Zbt' (Ternary 'B' Instructions)">;
def HasStdExtZbt : Predicate<"Subtarget->hasStdExtZbt()">,
                             AssemblerPredicate<(all_of FeatureExtZbt),
                             "'Zbt' (Ternary 'B' Instructions)">;

// Some instructions belong to both the basic and the permutation
// subextensions. They should be enabled if either has been specified.
def HasStdExtZbbOrZbp
    : Predicate<"Subtarget->hasStdExtZbb() || Subtarget->hasStdExtZbp()">,
                AssemblerPredicate<(any_of FeatureExtZbb, FeatureExtZbp),
                                   "'Zbb' (Base 'B' Instructions) or "
                                   "'Zbp' (Permutation 'B' Instructions)">;

def FeatureExtZbproposedc
    : SubtargetFeature<"experimental-zbproposedc", "HasStdExtZbproposedc", "true",
                       "'Zbproposedc' (Proposed Compressed 'B' Instructions)">;
def HasStdExtZbproposedc : Predicate<"Subtarget->hasStdExtZbproposedc()">,
                           AssemblerPredicate<(all_of FeatureExtZbproposedc),
                           "'Zbproposedc' (Proposed Compressed 'B' Instructions)">;

def FeatureStdExtB
    : SubtargetFeature<"experimental-b", "HasStdExtB", "true",
                       "'B' (Bit Manipulation Instructions)",
                       [FeatureExtZba,
                        FeatureExtZbb,
                        FeatureExtZbc,
                        FeatureExtZbe,
                        FeatureExtZbf,
                        FeatureExtZbm,
                        FeatureExtZbp,
                        FeatureExtZbr,
                        FeatureExtZbs,
                        FeatureExtZbt]>;
def HasStdExtB : Predicate<"Subtarget->hasStdExtB()">,
                           AssemblerPredicate<(all_of FeatureStdExtB),
                           "'B' (Bit Manipulation Instructions)">;

def FeatureNoPRCHints
    : SubtargetFeature<"no-rvc-hints", "EnablePRCHintInstrs", "false",
                       "Disable PRC Hint Instructions.">;
def HasPRCHints : Predicate<"Subtarget->enablePRCHintInstrs()">,
                  AssemblerPredicate<(all_of(not FeatureNoPRCHints)),
                                      "PRC Hint Instructions">;

def FeatureStdExtV
    : SubtargetFeature<"experimental-v", "HasStdExtV", "true",
                       "'V' (Vector Instructions)">;
def HasStdExtV : Predicate<"Subtarget->hasStdExtV()">,
                           AssemblerPredicate<(all_of FeatureStdExtV),
                           "'V' (Vector Instructions)">;

def FeatureStdExtZvlsseg
    : SubtargetFeature<"experimental-zvlsseg", "HasStdExtZvlsseg", "true",
                       "'Zvlsseg' (Vector segment load/store instructions)",
                       [FeatureStdExtV]>;
def HasStdExtZvlsseg : Predicate<"Subtarget->hasStdExtZvlsseg()">,
                                 AssemblerPredicate<(all_of FeatureStdExtZvlsseg),
                                 "'Zvlsseg' (Vector segment load/store instructions)">;

def FeatureExtZvamo
    : SubtargetFeature<"experimental-zvamo", "HasStdExtZvamo", "true",
                       "'Zvamo' (Vector AMO Operations)",
                       [FeatureStdExtV]>;
def HasStdExtZvamo : Predicate<"Subtarget->hasStdExtZvamo()">,
                               AssemblerPredicate<(all_of FeatureExtZvamo),
                               "'Zvamo' (Vector AMO Operations)">;

def Feature64Bit
    : SubtargetFeature<"64bit", "HasPR64", "true", "Implements PR64">;
def IsPR64 : Predicate<"Subtarget->is64Bit()">,
                       AssemblerPredicate<(all_of Feature64Bit),
                       "PR64I Base Instruction Set">;
def IsPR32 : Predicate<"!Subtarget->is64Bit()">,
                       AssemblerPredicate<(all_of (not Feature64Bit)),
                       "PR32I Base Instruction Set">;

defvar PR32 = DefaultMode;
def PR64           : HwMode<"+64bit">;

def FeaturePR32E
    : SubtargetFeature<"e", "IsPR32E", "true",
                       "Implements PR32E (provides 16 rather than 32 GPRs)">;
def IsPR32E : Predicate<"Subtarget->isPR32E()">,
                        AssemblerPredicate<(all_of FeaturePR32E)>;

def FeatureRelax
    : SubtargetFeature<"relax", "EnableLinkerRelax", "true",
                       "Enable Linker relaxation.">;

foreach i = {1-31} in
    def FeatureReserveX#i :
        SubtargetFeature<"reserve-x"#i, "UserReservedRegister[Primate::X"#i#"]",
                         "true", "Reserve X"#i>;

def FeatureSaveRestore : SubtargetFeature<"save-restore", "EnableSaveRestore",
                                          "true", "Enable save/restore.">;

//===----------------------------------------------------------------------===//
// Named operands for CSR instructions.
//===----------------------------------------------------------------------===//

include "PrimateSystemOperands.td"

//===----------------------------------------------------------------------===//
// Registers, calling conventions, instruction descriptions.
//===----------------------------------------------------------------------===//

include "PrimateSchedule.td"
include "PrimateRegisterInfo.td"
include "PrimateCallingConv.td"
include "PrimateInstrInfo.td"
include "PrimateRegisterBanks.td"
include "PrimateSchedRocket.td"
include "PrimateSchedSiFive7.td"

//===----------------------------------------------------------------------===//
// Primate processors supported.
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic-pr32", NoSchedModel, []>;
def : ProcessorModel<"generic-pr64", NoSchedModel, [Feature64Bit]>;

def : ProcessorModel<"rocket-pr32", RocketModel, []>;
def : ProcessorModel<"rocket-pr64", RocketModel, [Feature64Bit]>;

def : ProcessorModel<"sifive-7-pr32", SiFive7Model, []>;
def : ProcessorModel<"sifive-7-pr64", SiFive7Model, [Feature64Bit]>;

def : ProcessorModel<"sifive-e31", RocketModel, [FeatureStdExtM,
                                                 FeatureStdExtA,
                                                 FeatureStdExtC]>;

def : ProcessorModel<"sifive-u54", RocketModel, [Feature64Bit,
                                                 FeatureStdExtM,
                                                 FeatureStdExtA,
                                                 FeatureStdExtF,
                                                 FeatureStdExtD,
                                                 FeatureStdExtC]>;

def : ProcessorModel<"sifive-e76", SiFive7Model, [FeatureStdExtM,
                                                  FeatureStdExtA,
                                                  FeatureStdExtF,
                                                  FeatureStdExtC]>;

def : ProcessorModel<"sifive-u74", SiFive7Model, [Feature64Bit,
                                                  FeatureStdExtM,
                                                  FeatureStdExtA,
                                                  FeatureStdExtF,
                                                  FeatureStdExtD,
                                                  FeatureStdExtC]>;

//===----------------------------------------------------------------------===//
// Define the Primate target.
//===----------------------------------------------------------------------===//

def PrimateInstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def PrimateAsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = 1;
  let AllowDuplicateRegisterNames = 1;
}

def PrimateAsmWriter : AsmWriter {
  int PassSubtarget = 1;
}

def Primate : Target {
  let InstructionSet = PrimateInstrInfo;
  let AssemblyParsers = [PrimateAsmParser];
  let AssemblyWriters = [PrimateAsmWriter];
  let AllowRegisterRenaming = 1;
}
