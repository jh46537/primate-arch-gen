//===-- PrimateAttributes.h - Primate Attributes ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains enumerations for Primate attributes as defined in Primate
// ELF psABI specification.
//
// Primate ELF psABI specification
//
// https://github.com/primate/primate-elf-psabi-doc/blob/master/primate-elf.md
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_SUPPORT_PRIMATEATTRIBUTES_H
#define LLVM_SUPPORT_PRIMATEATTRIBUTES_H

#include "llvm/Support/ELFAttributes.h"

namespace llvm {
namespace PrimateAttrs {

const TagNameMap &getPrimateAttributeTags();

enum AttrType : unsigned {
  // Attribute types in ELF/.primate.attributes.
  STACK_ALIGN = 4,
  ARCH = 5,
  UNALIGNED_ACCESS = 6,
  PRIV_SPEC = 8,
  PRIV_SPEC_MINOR = 10,
  PRIV_SPEC_REVISION = 12,
};

enum StackAlign { ALIGN_4 = 4, ALIGN_16 = 16 };

enum { NOT_ALLOWED = 0, ALLOWED = 1 };

} // namespace PrimateAttrs
} // namespace llvm

#endif
