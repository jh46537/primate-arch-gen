//===-- PrimateAttributeParser.h - Primate Attribute Parser -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_PRIMATEATTRIBUTEPARSER_H
#define LLVM_SUPPORT_PRIMATEATTRIBUTEPARSER_H

#include "llvm/Support/ELFAttributeParser.h"
#include "llvm/Support/PrimateAttributes.h"

namespace llvm {
class PrimateAttributeParser : public ELFAttributeParser {
  struct DisplayHandler {
    PrimateAttrs::AttrType attribute;
    Error (PrimateAttributeParser::*routine)(unsigned);
  };
  static const DisplayHandler displayRoutines[];

  Error handler(uint64_t tag, bool &handled) override;

  Error unalignedAccess(unsigned tag);
  Error stackAlign(unsigned tag);

public:
  PrimateAttributeParser(ScopedPrinter *sw)
      : ELFAttributeParser(sw, PrimateAttrs::getPrimateAttributeTags(),
          "primate") {}
  PrimateAttributeParser()
      : ELFAttributeParser(PrimateAttrs::getPrimateAttributeTags(),
          "primate") {}
};

} // namespace llvm

#endif
