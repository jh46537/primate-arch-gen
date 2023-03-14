//===-- PrimateSubtarget.cpp - Primate Subtarget Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Primate specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "PrimateSubtarget.h"
#include "Primate.h"
#include "PrimateCallLowering.h"
#include "PrimateFrameLowering.h"
#include "PrimateLegalizerInfo.h"
#include "PrimateRegisterBankInfo.h"
#include "PrimateTargetMachine.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "primate-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "PrimateGenSubtargetInfo.inc"

static cl::opt<unsigned> PRVVectorBitsMax(
    "primate-v-vector-bits-max",
    cl::desc("Assume V extension vector registers are at most this big, "
             "with zero meaning no maximum size is assumed."),
    cl::init(0), cl::Hidden);

static cl::opt<unsigned> PRVVectorBitsMin(
    "primate-v-vector-bits-min",
    cl::desc("Assume V extension vector registers are at least this big, "
             "with zero meaning no minimum size is assumed."),
    cl::init(0), cl::Hidden);

static cl::opt<unsigned> PRVVectorLMULMax(
    "primate-v-fixed-length-vector-lmul-max",
    cl::desc("The maximum LMUL value to use for fixed length vectors. "
             "Fractional LMUL values are not supported."),
    cl::init(8), cl::Hidden);

void PrimateSubtarget::anchor() {}

PrimateSubtarget &
PrimateSubtarget::initializeSubtargetDependencies(const Triple &TT, StringRef CPU,
                                                StringRef TuneCPU, StringRef FS,
                                                StringRef ABIName) {
  // Determine default and user-specified characteristics
  bool Is64Bit = TT.isArch64Bit();
  if (CPU.empty())
    CPU = Is64Bit ? "generic-pr64" : "generic-pr32";
  if (CPU == "generic")
    report_fatal_error(Twine("CPU 'generic' is not supported. Use ") +
                       (Is64Bit ? "generic-pr64" : "generic-pr32"));

  if (TuneCPU.empty())
    TuneCPU = CPU;

  ParseSubtargetFeatures(CPU, TuneCPU, FS);
  //FIXME(ahsu)
  if (Is64Bit) {
    XLenVT = MVT::i64;
    XLen = 64;
  }
  //XLenVT = MVT::i128;
  //XLen = 128;

  TargetABI = PrimateABI::computeTargetABI(TT, getFeatureBits(), ABIName);
  PrimateFeatures::validate(TT, getFeatureBits());
  return *this;
}

PrimateSubtarget::PrimateSubtarget(const Triple &TT, StringRef CPU,
                               StringRef TuneCPU, StringRef FS,
                               StringRef ABIName, const TargetMachine &TM)
    : PrimateGenSubtargetInfo(TT, CPU, TuneCPU, FS),
      UserReservedRegister(Primate::NUM_TARGET_REGS),
      FrameLowering(initializeSubtargetDependencies(TT, CPU, TuneCPU, FS, ABIName)),
      InstrInfo(*this), RegInfo(getHwMode()), TLInfo(TM, *this),
      InstrItins(getInstrItineraryForCPU(selectPrimateCPU(CPU, TT.isArch64Bit()))) {
  CallLoweringInfo.reset(new PrimateCallLowering(*getTargetLowering()));
  Legalizer.reset(new PrimateLegalizerInfo(*this));

  auto *RBI = new PrimateRegisterBankInfo(*getRegisterInfo());
  RegBankInfo.reset(RBI);
  InstSelector.reset(createPrimateInstructionSelector(
      *static_cast<const PrimateTargetMachine *>(&TM), *this, *RBI));
}

const CallLowering *PrimateSubtarget::getCallLowering() const {
  return CallLoweringInfo.get();
}

InstructionSelector *PrimateSubtarget::getInstructionSelector() const {
  return InstSelector.get();
}

const LegalizerInfo *PrimateSubtarget::getLegalizerInfo() const {
  return Legalizer.get();
}

const RegisterBankInfo *PrimateSubtarget::getRegBankInfo() const {
  return RegBankInfo.get();
}

unsigned PrimateSubtarget::getMaxPRVVectorSizeInBits() const {
  assert(hasStdExtV() && "Tried to get vector length without V support!");
  if (PRVVectorBitsMax == 0)
    return 0;
  assert(PRVVectorBitsMax >= 128 && PRVVectorBitsMax <= 65536 &&
         isPowerOf2_32(PRVVectorBitsMax) &&
         "V extension requires vector length to be in the range of 128 to "
         "65536 and a power of 2!");
  assert(PRVVectorBitsMax >= PRVVectorBitsMin &&
         "Minimum V extension vector length should not be larger than its "
         "maximum!");
  unsigned Max = std::max(PRVVectorBitsMin, PRVVectorBitsMax);
  return PowerOf2Floor((Max < 128 || Max > 65536) ? 0 : Max);
}

unsigned PrimateSubtarget::getMinPRVVectorSizeInBits() const {
  assert(hasStdExtV() &&
         "Tried to get vector length without V extension support!");
  assert((PRVVectorBitsMin == 0 ||
          (PRVVectorBitsMin >= 128 && PRVVectorBitsMax <= 65536 &&
           isPowerOf2_32(PRVVectorBitsMin))) &&
         "V extension requires vector length to be in the range of 128 to "
         "65536 and a power of 2!");
  assert((PRVVectorBitsMax >= PRVVectorBitsMin || PRVVectorBitsMax == 0) &&
         "Minimum V extension vector length should not be larger than its "
         "maximum!");
  unsigned Min = PRVVectorBitsMin;
  if (PRVVectorBitsMax != 0)
    Min = std::min(PRVVectorBitsMin, PRVVectorBitsMax);
  return PowerOf2Floor((Min < 128 || Min > 65536) ? 0 : Min);
}

unsigned PrimateSubtarget::getMaxLMULForFixedLengthVectors() const {
  assert(hasStdExtV() &&
         "Tried to get maximum LMUL without V extension support!");
  assert(PRVVectorLMULMax <= 8 && isPowerOf2_32(PRVVectorLMULMax) &&
         "V extension requires a LMUL to be at most 8 and a power of 2!");
  return PowerOf2Floor(std::max<unsigned>(PRVVectorLMULMax, 1));
}

bool PrimateSubtarget::usePRVForFixedLengthVectors() const {
  return hasStdExtV() && getMinPRVVectorSizeInBits() != 0;
}
