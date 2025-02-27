//===-- PrimateISAInfo.h - Primate ISA Information -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_PRIMATEISAINFO_H
#define LLVM_SUPPORT_PRIMATEISAINFO_H

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <map>
#include <string>
#include <vector>

namespace llvm {
void primateExtensionsHelp(StringMap<StringRef> DescMap);

class PrimateISAInfo {
public:
  PrimateISAInfo(const PrimateISAInfo &) = delete;
  PrimateISAInfo &operator=(const PrimateISAInfo &) = delete;

  /// Represents the major and version number components of a Primate extension.
  struct ExtensionVersion {
    unsigned Major;
    unsigned Minor;
  };

  static bool compareExtension(const std::string &LHS, const std::string &RHS);

  /// Helper class for OrderedExtensionMap.
  struct ExtensionComparator {
    bool operator()(const std::string &LHS, const std::string &RHS) const {
      return compareExtension(LHS, RHS);
    }
  };

  /// OrderedExtensionMap is std::map, it's specialized to keep entries
  /// in canonical order of extension.
  typedef std::map<std::string, ExtensionVersion, ExtensionComparator>
      OrderedExtensionMap;

  PrimateISAInfo(unsigned XLen, OrderedExtensionMap &Exts)
      : XLen(XLen), FLen(0), MinVLen(0), MaxELen(0), MaxELenFp(0), Exts(Exts) {}

  /// Parse Primate ISA info from arch string.
  /// If IgnoreUnknown is set, any unrecognised extension names or
  /// extensions with unrecognised versions will be silently dropped, except
  /// for the special case of the base 'i' and 'e' extensions, where the
  /// default version will be used (as ignoring the base is not possible).
  static llvm::Expected<std::unique_ptr<PrimateISAInfo>>
  parseArchString(StringRef Arch, bool EnableExperimentalExtension,
                  bool ExperimentalExtensionVersionCheck = true,
                  bool IgnoreUnknown = false);

  /// Parse Primate ISA info from an arch string that is already in normalized
  /// form (as defined in the psABI). Unlike parseArchString, this function
  /// will not error for unrecognized extension names or extension versions.
  static llvm::Expected<std::unique_ptr<PrimateISAInfo>>
  parseNormalizedArchString(StringRef Arch);

  /// Parse Primate ISA info from feature vector.
  static llvm::Expected<std::unique_ptr<PrimateISAInfo>>
  parseFeatures(unsigned XLen, const std::vector<std::string> &Features);

  /// Convert Primate ISA info to a feature vector.
  std::vector<std::string> toFeatures(bool AddAllExtensions = false,
                                      bool IgnoreUnknown = true) const;

  const OrderedExtensionMap &getExtensions() const { return Exts; }

  unsigned getXLen() const { return XLen; }
  unsigned getFLen() const { return FLen; }
  unsigned getMinVLen() const { return MinVLen; }
  unsigned getMaxVLen() const { return 65536; }
  unsigned getMaxELen() const { return MaxELen; }
  unsigned getMaxELenFp() const { return MaxELenFp; }

  bool hasExtension(StringRef Ext) const;
  std::string toString() const;
  StringRef computeDefaultABI() const;

  static bool isSupportedExtensionFeature(StringRef Ext);
  static bool isSupportedExtension(StringRef Ext);
  static bool isSupportedExtensionWithVersion(StringRef Ext);
  static bool isSupportedExtension(StringRef Ext, unsigned MajorVersion,
                                   unsigned MinorVersion);
  static llvm::Expected<std::unique_ptr<PrimateISAInfo>>
  postProcessAndChecking(std::unique_ptr<PrimateISAInfo> &&ISAInfo);
  static std::string getTargetFeatureForExtension(StringRef Ext);

private:
  PrimateISAInfo(unsigned XLen)
      : XLen(XLen), FLen(0), MinVLen(0), MaxELen(0), MaxELenFp(0) {}

  unsigned XLen;
  unsigned FLen;
  unsigned MinVLen;
  unsigned MaxELen, MaxELenFp;

  OrderedExtensionMap Exts;

  void addExtension(StringRef ExtName, ExtensionVersion Version);

  Error checkDependency();

  void updateImplication();
  void updateCombination();
  void updateFLen();
  void updateMinVLen();
  void updateMaxELen();
};

} // namespace llvm

#endif
