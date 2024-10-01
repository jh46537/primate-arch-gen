//===--- Primate.h - Declare Primate target feature support --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Primate TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_PRIMATE_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_PRIMATE_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/PrimateISAInfo.h"
#include "llvm/TargetParser/Triple.h"
#include <optional>

namespace clang {
namespace targets {

// Primate Target
class PrimateTargetInfo : public TargetInfo {
protected:
  std::string ABI, CPU;
  std::unique_ptr<llvm::PrimateISAInfo> ISAInfo;

private:
  bool FastUnalignedAccess;
  bool HasExperimental = false;

public:
  PrimateTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    BFloat16Width = 16;
    BFloat16Align = 16;
    BFloat16Format = &llvm::APFloat::BFloat();
    LongDoubleWidth = 128;
    LongDoubleAlign = 128;
    LongDoubleFormat = &llvm::APFloat::IEEEquad();
    SuitableAlign = 128;
    WCharType = SignedInt;
    WIntType = UnsignedInt;
    //HasPrimateVTypes = true;
    MCountName = "_mcount";
    HasFloat16 = true;
    HasStrictFP = true;
  }

  bool setCPU(const std::string &Name) override {
    if (!isValidCPUName(Name))
      return false;
    CPU = Name;
    return true;
  }

  StringRef getABI() const override { return ABI; }
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<Builtin::Info> getTargetBuiltins() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  std::string_view getClobbers() const override { return ""; }

  StringRef getConstraintRegister(StringRef Constraint,
                                  StringRef Expression) const override {
    return Expression;
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  int getEHDataRegisterNumber(unsigned RegNo) const override {
    if (RegNo == 0)
      return 10;
    else if (RegNo == 1)
      return 11;
    else
      return -1;
  }

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;

  std::string convertConstraint(const char *&Constraint) const override;

  bool
  initFeatureMap(llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags,
                 StringRef CPU,
                 const std::vector<std::string> &FeaturesVec) const override;

  std::optional<std::pair<unsigned, unsigned>>
  getVScaleRange(const LangOptions &LangOpts) const override;

  bool hasFeature(StringRef Feature) const override;

  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override;

  bool hasBitIntType() const override { return true; }

  bool hasBFloat16Type() const override { return true; }

  bool useFP16ConversionIntrinsics() const override {
    return false;
  }

  bool isValidCPUName(StringRef Name) const override;
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override;
  bool isValidTuneCPUName(StringRef Name) const override;
  void fillValidTuneCPUList(SmallVectorImpl<StringRef> &Values) const override;
  bool supportsTargetAttributeTune() const override { return true; }
  ParsedTargetAttr parseTargetAttr(StringRef Str) const override;
};
class LLVM_LIBRARY_VISIBILITY Primate32TargetInfo : public PrimateTargetInfo {
public:
  Primate32TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : PrimateTargetInfo(Triple, Opts) {
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    SizeType = UnsignedInt;
    resetDataLayout("e-G1-m:e-p:32:32-i64:64-n32-S128");
  }

  bool setABI(const std::string &Name) override {
    if (Name == "ilp32e") {
      ABI = Name;
      resetDataLayout("e-G1-m:e-p:32:32-i64:64-n32-S128");
      return true;
    }

    if (Name == "ilp32" || Name == "ilp32f" || Name == "ilp32d") {
      ABI = Name;
      return true;
    }
    return false;
  }

  void setMaxAtomicWidth() override {
    MaxAtomicPromoteWidth = 128;

    if (ISAInfo->hasExtension("a"))
      MaxAtomicInlineWidth = 32;
  }
};
class LLVM_LIBRARY_VISIBILITY Primate64TargetInfo : public PrimateTargetInfo {
public:
  Primate64TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : PrimateTargetInfo(Triple, Opts) {
    LongWidth = LongAlign = PointerWidth = PointerAlign = 64;
    IntMaxType = Int64Type = SignedLong;
    resetDataLayout("e-G1-m:e-p:64:64-i64:64-i128:128-n32:64-S128");
  }

  bool setABI(const std::string &Name) override {
    if (Name == "lp64e") {
      ABI = Name;
      resetDataLayout("e-G1-m:e-p:64:64-i64:64-i128:128-n32:64-S64");
      return true;
    }

    if (Name == "lp64" || Name == "lp64f" || Name == "lp64d") {
      ABI = Name;
      return true;
    }
    return false;
  }

  void setMaxAtomicWidth() override {
    MaxAtomicPromoteWidth = 128;

    if (ISAInfo->hasExtension("a"))
      MaxAtomicInlineWidth = 64;
  }
};
} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_PRIMATE_H
