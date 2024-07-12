//===-- PrimateTargetStreamer.cpp - Primate Target Streamer Methods -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Primate specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "PrimateTargetStreamer.h"
#include "PrimateMCTargetDesc.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/PrimateAttributes.h"

using namespace llvm;

PrimateTargetStreamer::PrimateTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

void PrimateTargetStreamer::finish() { finishAttributeSection(); }

void PrimateTargetStreamer::emitDirectiveOptionPush() {}
void PrimateTargetStreamer::emitDirectiveOptionPop() {}
void PrimateTargetStreamer::emitDirectiveOptionPIC() {}
void PrimateTargetStreamer::emitDirectiveOptionNoPIC() {}
void PrimateTargetStreamer::emitDirectiveOptionPRC() {}
void PrimateTargetStreamer::emitDirectiveOptionNoPRC() {}
void PrimateTargetStreamer::emitDirectiveOptionRelax() {}
void PrimateTargetStreamer::emitDirectiveOptionNoRelax() {}
void PrimateTargetStreamer::emitAttribute(unsigned Attribute, unsigned Value) {}
void PrimateTargetStreamer::finishAttributeSection() {}
void PrimateTargetStreamer::emitTextAttribute(unsigned Attribute,
                                            StringRef String) {}
void PrimateTargetStreamer::emitIntTextAttribute(unsigned Attribute,
                                               unsigned IntValue,
                                               StringRef StringValue) {}

void PrimateTargetStreamer::emitTargetAttributes(const MCSubtargetInfo &STI) {
  if (STI.hasFeature(Primate::FeaturePRE))
    emitAttribute(PrimateAttrs::STACK_ALIGN, PrimateAttrs::ALIGN_4);
  else
    emitAttribute(PrimateAttrs::STACK_ALIGN, PrimateAttrs::ALIGN_16);

  std::string Arch = "pr32";
  if (STI.hasFeature(Primate::Feature64Bit))
    Arch = "pr64";
  if (STI.hasFeature(Primate::FeaturePRE))
    Arch += "e1p9";
  else
    Arch += "i2p0";
  if (STI.hasFeature(Primate::FeatureStdExtM))
    Arch += "_m2p0";
  if (STI.hasFeature(Primate::FeatureStdExtA))
    Arch += "_a2p0";
  if (STI.hasFeature(Primate::FeatureStdExtF))
    Arch += "_f2p0";
  if (STI.hasFeature(Primate::FeatureStdExtD))
    Arch += "_d2p0";
  if (STI.hasFeature(Primate::FeatureStdExtC))
    Arch += "_c2p0";
  if (STI.hasFeature(Primate::FeatureStdExtZbb))
    Arch += "_b0p93";
  if (STI.hasFeature(Primate::FeatureStdExtV))
    Arch += "_v0p10";
  if (STI.hasFeature(Primate::FeatureStdExtZfh))
    Arch += "_zfh0p1";
  if (STI.hasFeature(Primate::FeatureStdExtZba))
    Arch += "_zba0p93";
  if (STI.hasFeature(Primate::FeatureStdExtZbb))
    Arch += "_zbb0p93";
  if (STI.hasFeature(Primate::FeatureStdExtZbc))
    Arch += "_zbc0p93";
  if (STI.hasFeature(Primate::FeatureStdExtZbs))
    Arch += "_zbs0p93";

  emitTextAttribute(PrimateAttrs::ARCH, Arch);
}

// This part is for ascii assembly output
PrimateTargetAsmStreamer::PrimateTargetAsmStreamer(MCStreamer &S,
                                               formatted_raw_ostream &OS)
    : PrimateTargetStreamer(S), OS(OS) {}

void PrimateTargetAsmStreamer::emitDirectiveOptionPush() {
  OS << "\t.option\tpush\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionPop() {
  OS << "\t.option\tpop\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionPIC() {
  OS << "\t.option\tpic\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionNoPIC() {
  OS << "\t.option\tnopic\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionPRC() {
  OS << "\t.option\tprc\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionNoPRC() {
  OS << "\t.option\tnoprc\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionRelax() {
  OS << "\t.option\trelax\n";
}

void PrimateTargetAsmStreamer::emitDirectiveOptionNoRelax() {
  OS << "\t.option\tnorelax\n";
}

void PrimateTargetAsmStreamer::emitAttribute(unsigned Attribute, unsigned Value) {
  OS << "\t.attribute\t" << Attribute << ", " << Twine(Value) << "\n";
}

void PrimateTargetAsmStreamer::emitTextAttribute(unsigned Attribute,
                                               StringRef String) {
  OS << "\t.attribute\t" << Attribute << ", \"" << String << "\"\n";
}

void PrimateTargetAsmStreamer::emitIntTextAttribute(unsigned Attribute,
                                                  unsigned IntValue,
                                                  StringRef StringValue) {}

void PrimateTargetAsmStreamer::finishAttributeSection() {}
