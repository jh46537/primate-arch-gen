//===--- PrimateToolchain.h - Primate ToolChain Implementations -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PRIMATETOOLCHAIN_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PRIMATETOOLCHAIN_H

#include "Gnu.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY PrimateToolChain : public Generic_ELF {
public:
  PrimateToolChain(const Driver &D, const llvm::Triple &Triple,
                   const llvm::opt::ArgList &Args);

  static bool hasGCCToolchain(const Driver &D, const llvm::opt::ArgList &Args);
  bool IsIntegratedAssemblerDefault() const override { return true; }
  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind) const override;
  RuntimeLibType GetDefaultRuntimeLibType() const override;
  UnwindLibType
  GetUnwindLibType(const llvm::opt::ArgList &Args) const override;
  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void
  addLibStdCxxIncludePaths(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const override;

protected:
  Tool *buildLinker() const override;

private:
  std::string computeSysRoot() const override;
};

} // end namespace toolchains

namespace tools {
namespace Primate {
class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("Primate::Linker", "ld", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};
} // end namespace Primate
} // end namespace tools

} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PRIMATETOOLCHAIN_H
