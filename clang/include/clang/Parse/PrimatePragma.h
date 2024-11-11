//===--- PrimatePragma.h - Types for PrimatePragma ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_PARSE_PRIMATE_H
#define LLVM_CLANG_PARSE_PRIMATE_H

#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/Ownership.h"
#include "clang/Sema/ParsedAttr.h"

namespace clang {

/// Primate pragma.
struct PrimatePragma {
  // Source range of the directive.
  SourceRange Range;
  // Identifier corresponding to the name of the pragma.  "primate" for
  // "#pragma primate" directives.
  IdentifierLoc *PragmaNameLoc;
  // Name of the primate option.  "blue", "green", "reg", "model"
  IdentifierLoc *OptionLoc;
  // Name of the primate Functional unit. Depends on option.
  IdentifierLoc *FuncUnitNameLoc;
  // Name of the primate instruction for this functional unit (IO unit has input and output operations)
  IdentifierLoc *InstructionNameLoc;
  // Expression for the option argument if it exists, null otherwise.
  Expr *ValueArg0;
  Expr *ValueArg1;

  PrimatePragma()
      : PragmaNameLoc(nullptr), OptionLoc(nullptr), FuncUnitNameLoc(nullptr), 
        InstructionNameLoc(nullptr),
        ValueArg0(nullptr), ValueArg1(nullptr) {}
};

} // end namespace clang

#endif // LLVM_CLANG_PARSE_PRIMATE_H
