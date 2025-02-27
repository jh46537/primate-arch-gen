//===--- Primate.cpp - Implement Primate target feature support --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Primate TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Primate.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/PrimateTargetParser.h"
#include <optional>

using namespace clang;
using namespace clang::targets;

ArrayRef<const char *> PrimateTargetInfo::getGCCRegNames() const {
  // clang-format off
  static const char *const GCCRegNames[] = {
      // Integer registers
      "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
      "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
      "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
      "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31",

      // Floating point registers
      "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
      "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
      "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
      "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",

      // Vector registers
      "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
      "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
      "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
      "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",

      // CSRs
      "fflags", "frm", "vtype", "vl", "vxsat", "vxrm"
    };
  // clang-format on
  return llvm::ArrayRef(GCCRegNames);
}

ArrayRef<TargetInfo::GCCRegAlias> PrimateTargetInfo::getGCCRegAliases() const {
  static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
      {{"zero"}, "x0"}, {{"ra"}, "x1"},   {{"sp"}, "x2"},    {{"gp"}, "x3"},
      {{"tp"}, "x4"},   {{"t0"}, "x5"},   {{"t1"}, "x6"},    {{"t2"}, "x7"},
      {{"s0"}, "x8"},   {{"s1"}, "x9"},   {{"a0"}, "x10"},   {{"a1"}, "x11"},
      {{"a2"}, "x12"},  {{"a3"}, "x13"},  {{"a4"}, "x14"},   {{"a5"}, "x15"},
      {{"a6"}, "x16"},  {{"a7"}, "x17"},  {{"s2"}, "x18"},   {{"s3"}, "x19"},
      {{"s4"}, "x20"},  {{"s5"}, "x21"},  {{"s6"}, "x22"},   {{"s7"}, "x23"},
      {{"s8"}, "x24"},  {{"s9"}, "x25"},  {{"s10"}, "x26"},  {{"s11"}, "x27"},
      {{"t3"}, "x28"},  {{"t4"}, "x29"},  {{"t5"}, "x30"},   {{"t6"}, "x31"},
      {{"ft0"}, "f0"},  {{"ft1"}, "f1"},  {{"ft2"}, "f2"},   {{"ft3"}, "f3"},
      {{"ft4"}, "f4"},  {{"ft5"}, "f5"},  {{"ft6"}, "f6"},   {{"ft7"}, "f7"},
      {{"fs0"}, "f8"},  {{"fs1"}, "f9"},  {{"fa0"}, "f10"},  {{"fa1"}, "f11"},
      {{"fa2"}, "f12"}, {{"fa3"}, "f13"}, {{"fa4"}, "f14"},  {{"fa5"}, "f15"},
      {{"fa6"}, "f16"}, {{"fa7"}, "f17"}, {{"fs2"}, "f18"},  {{"fs3"}, "f19"},
      {{"fs4"}, "f20"}, {{"fs5"}, "f21"}, {{"fs6"}, "f22"},  {{"fs7"}, "f23"},
      {{"fs8"}, "f24"}, {{"fs9"}, "f25"}, {{"fs10"}, "f26"}, {{"fs11"}, "f27"},
      {{"ft8"}, "f28"}, {{"ft9"}, "f29"}, {{"ft10"}, "f30"}, {{"ft11"}, "f31"}};
  return llvm::ArrayRef(GCCRegAliases);
}

bool PrimateTargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  default:
    return false;
  case 'I':
    // A 12-bit signed immediate.
    Info.setRequiresImmediate(-2048, 2047);
    return true;
  case 'J':
    // Integer zero.
    Info.setRequiresImmediate(0);
    return true;
  case 'K':
    // A 5-bit unsigned immediate for CSR access instructions.
    Info.setRequiresImmediate(0, 31);
    return true;
  case 'f':
    // A floating-point register.
    Info.setAllowsRegister();
    return true;
  case 'A':
    // An address that is held in a general-purpose register.
    Info.setAllowsMemory();
    return true;
  case 'S': // A symbolic address
    Info.setAllowsRegister();
    return true;
  case 'v':
    // A vector register.
    if (Name[1] == 'r' || Name[1] == 'm') {
      Info.setAllowsRegister();
      Name += 1;
      return true;
    }
    return false;
  }
}

std::string PrimateTargetInfo::convertConstraint(const char *&Constraint) const {
  std::string R;
  switch (*Constraint) {
  case 'v':
    R = std::string("^") + std::string(Constraint, 2);
    Constraint += 1;
    break;
  default:
    R = TargetInfo::convertConstraint(Constraint);
    break;
  }
  return R;
}

static unsigned getVersionValue(unsigned MajorVersion, unsigned MinorVersion) {
  return MajorVersion * 1000000 + MinorVersion * 1000;
}

void PrimateTargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  Builder.defineMacro("__Primate");
  bool Is64Bit = getTriple().isPrimate64();
  Builder.defineMacro("__Primate_xlen", Is64Bit ? "64" : "32");
  StringRef CodeModel = getTargetOpts().CodeModel;
  unsigned FLen = ISAInfo->getFLen();
  unsigned MinVLen = ISAInfo->getMinVLen();
  unsigned MaxELen = ISAInfo->getMaxELen();
  unsigned MaxELenFp = ISAInfo->getMaxELenFp();
  if (CodeModel == "default")
    CodeModel = "small";

  if (CodeModel == "small")
    Builder.defineMacro("__Primate_cmodel_medlow");
  else if (CodeModel == "medium")
    Builder.defineMacro("__Primate_cmodel_medany");

  StringRef ABIName = getABI();
  if (ABIName == "ilp32f" || ABIName == "lp64f")
    Builder.defineMacro("__Primate_float_abi_single");
  else if (ABIName == "ilp32d" || ABIName == "lp64d")
    Builder.defineMacro("__Primate_float_abi_double");
  else
    Builder.defineMacro("__Primate_float_abi_soft");

  if (ABIName == "ilp32e" || ABIName == "lp64e")
    Builder.defineMacro("__Primate_abi_rve");

  Builder.defineMacro("__Primate_arch_test");

  for (auto &Extension : ISAInfo->getExtensions()) {
    auto ExtName = Extension.first;
    auto ExtInfo = Extension.second;

    Builder.defineMacro(Twine("__Primate_", ExtName),
                        Twine(getVersionValue(ExtInfo.Major, ExtInfo.Minor)));
  }

  if (ISAInfo->hasExtension("m") || ISAInfo->hasExtension("zmmul"))
    Builder.defineMacro("__Primate_mul");

  if (ISAInfo->hasExtension("m")) {
    Builder.defineMacro("__Primate_div");
    Builder.defineMacro("__Primate_muldiv");
  }

  if (ISAInfo->hasExtension("a")) {
    Builder.defineMacro("__Primate_atomic");
    Builder.defineMacro("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1");
    Builder.defineMacro("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2");
    Builder.defineMacro("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4");
    if (Is64Bit)
      Builder.defineMacro("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8");
  }

  if (FLen) {
    Builder.defineMacro("__Primate_flen", Twine(FLen));
    Builder.defineMacro("__Primate_fdiv");
    Builder.defineMacro("__Primate_fsqrt");
  }

  if (MinVLen) {
    Builder.defineMacro("__Primate_v_min_vlen", Twine(MinVLen));
    Builder.defineMacro("__Primate_v_elen", Twine(MaxELen));
    Builder.defineMacro("__Primate_v_elen_fp", Twine(MaxELenFp));
  }

  if (ISAInfo->hasExtension("c"))
    Builder.defineMacro("__Primate_compressed");

  if (ISAInfo->hasExtension("zve32x")) {
    Builder.defineMacro("__Primate_vector");
    // Currently we support the v0.12 RISC-V V intrinsics.
    Builder.defineMacro("__Primate_v_intrinsic", Twine(getVersionValue(0, 12)));
  }

  auto VScale = getVScaleRange(Opts);
  if (VScale && VScale->first && VScale->first == VScale->second)
    Builder.defineMacro("__Primate_v_fixed_vlen",
                        Twine(VScale->first * llvm::Primate::PRVBitsPerBlock));

  if (FastUnalignedAccess)
    Builder.defineMacro("__Primate_misaligned_fast");
  else
    Builder.defineMacro("__Primate_misaligned_avoid");

  if (ISAInfo->hasExtension("e")) {
    if (Is64Bit)
      Builder.defineMacro("__Primate_64e");
    else
      Builder.defineMacro("__Primate_32e");
  }
}

static constexpr Builtin::Info BuiltinInfo[] = {
// #define BUILTIN(ID, TYPE, ATTRS)                                               \
//   {#ID, TYPE, ATTRS, nullptr, HeaderDesc::NO_HEADER, ALL_LANGUAGES},
// #define TARGET_BUILTIN(ID, TYPE, ATTRS, FEATURE)                               \
//   {#ID, TYPE, ATTRS, FEATURE, HeaderDesc::NO_HEADER, ALL_LANGUAGES},
// #include "clang/Basic/BuiltinsPrimateVector.def"
#define BUILTIN(ID, TYPE, ATTRS)                                               \
  {#ID, TYPE, ATTRS, nullptr, HeaderDesc::NO_HEADER, ALL_LANGUAGES},
#define TARGET_BUILTIN(ID, TYPE, ATTRS, FEATURE)                               \
  {#ID, TYPE, ATTRS, FEATURE, HeaderDesc::NO_HEADER, ALL_LANGUAGES},
#include "clang/Basic/BuiltinsPrimate.def"
};

ArrayRef<Builtin::Info> PrimateTargetInfo::getTargetBuiltins() const {
  return llvm::ArrayRef(BuiltinInfo,
                        clang::Primate::LastTSBuiltin - Builtin::FirstTSBuiltin);
}

bool PrimateTargetInfo::initFeatureMap(
    llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags, StringRef CPU,
    const std::vector<std::string> &FeaturesVec) const {

  unsigned XLen = 32;

  if (getTriple().isPrimate64()) {
    Features["64bit"] = true;
    XLen = 64;
  } else {
    Features["32bit"] = true;
  }

  // If a target attribute specified a full arch string, override all the ISA
  // extension target features.
  const auto I = llvm::find(FeaturesVec, "__Primate_TargetAttrNeedOverride");
  if (I != FeaturesVec.end()) {
    std::vector<std::string> OverrideFeatures(std::next(I), FeaturesVec.end());

    // Add back any non ISA extension features, e.g. +relax.
    auto IsNonISAExtFeature = [](StringRef Feature) {
      assert(Feature.size() > 1 && (Feature[0] == '+' || Feature[0] == '-'));
      StringRef Ext = Feature.substr(1); // drop the +/-
      return !llvm::PrimateISAInfo::isSupportedExtensionFeature(Ext);
    };
    llvm::copy_if(llvm::make_range(FeaturesVec.begin(), I),
                  std::back_inserter(OverrideFeatures), IsNonISAExtFeature);

    return TargetInfo::initFeatureMap(Features, Diags, CPU, OverrideFeatures);
  }

  // Otherwise, parse the features and add any implied extensions.
  std::vector<std::string> AllFeatures = FeaturesVec;
  auto ParseResult = llvm::PrimateISAInfo::parseFeatures(XLen, FeaturesVec);
  if (!ParseResult) {
    std::string Buffer;
    llvm::raw_string_ostream OutputErrMsg(Buffer);
    handleAllErrors(ParseResult.takeError(), [&](llvm::StringError &ErrMsg) {
      OutputErrMsg << ErrMsg.getMessage();
    });
    Diags.Report(diag::err_invalid_feature_combination) << OutputErrMsg.str();
    return false;
  }

  // Append all features, not just new ones, so we override any negatives.
  llvm::append_range(AllFeatures, (*ParseResult)->toFeatures());
  return TargetInfo::initFeatureMap(Features, Diags, CPU, AllFeatures);
}

std::optional<std::pair<unsigned, unsigned>>
PrimateTargetInfo::getVScaleRange(const LangOptions &LangOpts) const {
  // Primate::PRVBitsPerBlock is 64.
  unsigned VScaleMin = ISAInfo->getMinVLen() / llvm::Primate::PRVBitsPerBlock;

  if (LangOpts.VScaleMin || LangOpts.VScaleMax) {
    // Treat Zvl*b as a lower bound on vscale.
    VScaleMin = std::max(VScaleMin, LangOpts.VScaleMin);
    unsigned VScaleMax = LangOpts.VScaleMax;
    if (VScaleMax != 0 && VScaleMax < VScaleMin)
      VScaleMax = VScaleMin;
    return std::pair<unsigned, unsigned>(VScaleMin ? VScaleMin : 1, VScaleMax);
  }

  if (VScaleMin > 0) {
    unsigned VScaleMax = ISAInfo->getMaxVLen() / llvm::Primate::PRVBitsPerBlock;
    return std::make_pair(VScaleMin, VScaleMax);
  }

  return std::nullopt;
}

/// Return true if has this feature, need to sync with handleTargetFeatures.
bool PrimateTargetInfo::hasFeature(StringRef Feature) const {
  bool Is64Bit = getTriple().isPrimate64();
  auto Result = llvm::StringSwitch<std::optional<bool>>(Feature)
                    .Case("Primate", true)
                    .Case("Primate32", !Is64Bit)
                    .Case("Primate64", Is64Bit)
                    .Case("32bit", !Is64Bit)
                    .Case("64bit", Is64Bit)
                    .Case("experimental", HasExperimental)
                    .Default(std::nullopt);
  if (Result)
    return *Result;

  return ISAInfo->hasExtension(Feature);
}

/// Perform initialization based on the user configured set of features.
bool PrimateTargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                           DiagnosticsEngine &Diags) {
  unsigned XLen = getTriple().isArch64Bit() ? 64 : 32;
  auto ParseResult = llvm::PrimateISAInfo::parseFeatures(XLen, Features);
  if (!ParseResult) {
    std::string Buffer;
    llvm::raw_string_ostream OutputErrMsg(Buffer);
    handleAllErrors(ParseResult.takeError(), [&](llvm::StringError &ErrMsg) {
      OutputErrMsg << ErrMsg.getMessage();
    });
    Diags.Report(diag::err_invalid_feature_combination) << OutputErrMsg.str();
    return false;
  } else {
    ISAInfo = std::move(*ParseResult);
  }

  if (ABI.empty())
    ABI = ISAInfo->computeDefaultABI().str();

  if (ISAInfo->hasExtension("zfh") || ISAInfo->hasExtension("zhinx"))
    HasLegalHalfType = true;

  FastUnalignedAccess = llvm::is_contained(Features, "+fast-unaligned-access");

  if (llvm::is_contained(Features, "+experimental"))
    HasExperimental = true;

  if (ABI == "ilp32e" && ISAInfo->hasExtension("d")) {
    Diags.Report(diag::err_invalid_feature_combination)
        << "ILP32E cannot be used with the D ISA extension";
    return false;
  }
  return true;
}

bool PrimateTargetInfo::isValidCPUName(StringRef Name) const {
  bool Is64Bit = getTriple().isArch64Bit();
  return llvm::Primate::parseCPU(Name, Is64Bit);
}

void PrimateTargetInfo::fillValidCPUList(
    SmallVectorImpl<StringRef> &Values) const {
  bool Is64Bit = getTriple().isArch64Bit();
  llvm::Primate::fillValidCPUArchList(Values, Is64Bit);
}

bool PrimateTargetInfo::isValidTuneCPUName(StringRef Name) const {
  bool Is64Bit = getTriple().isArch64Bit();
  return llvm::Primate::parseTuneCPU(Name, Is64Bit);
}

void PrimateTargetInfo::fillValidTuneCPUList(
    SmallVectorImpl<StringRef> &Values) const {
  bool Is64Bit = getTriple().isArch64Bit();
  llvm::Primate::fillValidTuneCPUArchList(Values, Is64Bit);
}

static void handleFullArchString(StringRef FullArchStr,
                                 std::vector<std::string> &Features) {
  Features.push_back("__Primate_TargetAttrNeedOverride");
  auto RII = llvm::PrimateISAInfo::parseArchString(
      FullArchStr, /* EnableExperimentalExtension */ true);
  if (llvm::errorToBool(RII.takeError())) {
    // Forward the invalid FullArchStr.
    Features.push_back("+" + FullArchStr.str());
  } else {
    // Append a full list of features, including any negative extensions so that
    // we override the CPU's features.
    std::vector<std::string> FeatStrings =
        (*RII)->toFeatures(/* AddAllExtensions */ true);
    Features.insert(Features.end(), FeatStrings.begin(), FeatStrings.end());
  }
}

ParsedTargetAttr PrimateTargetInfo::parseTargetAttr(StringRef Features) const {
  ParsedTargetAttr Ret;
  if (Features == "default")
    return Ret;
  SmallVector<StringRef, 1> AttrFeatures;
  Features.split(AttrFeatures, ";");
  bool FoundArch = false;

  for (auto &Feature : AttrFeatures) {
    Feature = Feature.trim();
    StringRef AttrString = Feature.split("=").second.trim();

    if (Feature.starts_with("arch=")) {
      // Override last features
      Ret.Features.clear();
      if (FoundArch)
        Ret.Duplicate = "arch=";
      FoundArch = true;

      if (AttrString.starts_with("+")) {
        // EXTENSION like arch=+v,+zbb
        SmallVector<StringRef, 1> Exts;
        AttrString.split(Exts, ",");
        for (auto Ext : Exts) {
          if (Ext.empty())
            continue;

          StringRef ExtName = Ext.substr(1);
          std::string TargetFeature =
              llvm::PrimateISAInfo::getTargetFeatureForExtension(ExtName);
          if (!TargetFeature.empty())
            Ret.Features.push_back(Ext.front() + TargetFeature);
          else
            Ret.Features.push_back(Ext.str());
        }
      } else {
        // full-arch-string like arch=rv64gcv
        handleFullArchString(AttrString, Ret.Features);
      }
    } else if (Feature.starts_with("cpu=")) {
      if (!Ret.CPU.empty())
        Ret.Duplicate = "cpu=";

      Ret.CPU = AttrString;

      if (!FoundArch) {
        // Update Features with CPU's features
        StringRef MarchFromCPU = llvm::Primate::getMArchFromMcpu(Ret.CPU);
        if (MarchFromCPU != "") {
          Ret.Features.clear();
          handleFullArchString(MarchFromCPU, Ret.Features);
        }
      }
    } else if (Feature.starts_with("tune=")) {
      if (!Ret.Tune.empty())
        Ret.Duplicate = "tune=";

      Ret.Tune = AttrString;
    }
  }
  return Ret;
}
