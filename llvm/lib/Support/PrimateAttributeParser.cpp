//===-- PrimateAttributeParser.cpp - Primate Attribute Parser -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/PrimateAttributeParser.h"
#include "llvm/ADT/StringExtras.h"

using namespace llvm;

const PrimateAttributeParser::DisplayHandler
    PrimateAttributeParser::displayRoutines[] = {
        {
            PrimateAttrs::ARCH,
            &ELFAttributeParser::stringAttribute,
        },
        {
            PrimateAttrs::PRIV_SPEC,
            &ELFAttributeParser::integerAttribute,
        },
        {
            PrimateAttrs::PRIV_SPEC_MINOR,
            &ELFAttributeParser::integerAttribute,
        },
        {
            PrimateAttrs::PRIV_SPEC_REVISION,
            &ELFAttributeParser::integerAttribute,
        },
        {
            PrimateAttrs::STACK_ALIGN,
            &PrimateAttributeParser::stackAlign,
        },
        {
            PrimateAttrs::UNALIGNED_ACCESS,
            &PrimateAttributeParser::unalignedAccess,
        }};

Error PrimateAttributeParser::unalignedAccess(unsigned tag) {
  static const char *strings[] = {"No unaligned access", "Unaligned access"};
  return parseStringAttribute("Unaligned_access", tag, ArrayRef(strings));
}

Error PrimateAttributeParser::stackAlign(unsigned tag) {
  uint64_t value = de.getULEB128(cursor);
  std::string description =
      "Stack alignment is " + utostr(value) + std::string("-bytes");
  printAttribute(tag, value, description);
  return Error::success();
}

Error PrimateAttributeParser::handler(uint64_t tag, bool &handled) {
  handled = false;
  for (unsigned AHI = 0, AHE = std::size(displayRoutines); AHI != AHE;
       ++AHI) {
    if (uint64_t(displayRoutines[AHI].attribute) == tag) {
      if (Error e = (this->*displayRoutines[AHI].routine)(tag))
        return e;
      handled = true;
      break;
    }
  }

  return Error::success();
}
