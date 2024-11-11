//===--- ParsePrimate.cpp - Primate Parsing ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements parsing of all Primate directives and clauses.
//
//===----------------------------------------------------------------------===//
//

#include "clang/Basic/AttributeCommonInfo.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/PrimatePragma.h"

using namespace clang;

// Parse Primate pragma annotating a free function
// use:
//   #pragma primate blue <Functional Unit Name> <Function> ...
//   #pragma primate green input_read
//   #pragma primate green input_seek
//   #pragma primate green output_write
//   #pragma primate green output_seek
//   #pragma primate green extract
//   #pragma primate green insert
SourceLocation Parser::ParsePragmaPrimateFreeFunction(DeclSpec &DS) {
  // Create attribute list.
  ParsedAttributes Attrs(AttrFactory);

  SourceLocation StartLoc = Tok.getLocation();
  SourceLocation EndLoc = SourceLocation{};

  if (!Tok.is(tok::annot_pragma_primate)) {
    DS.SetTypeSpecError();
    return EndLoc;
  }

  // Get primate pragmas and consume annotated token.
  PrimatePragma Pragma;

  if (!HandlePragmaPrimate(Pragma)) {
    DS.SetTypeSpecError();
    return EndLoc;
  }
  EndLoc = Pragma.Range.getEnd();

  ArgsUnion ArgsPragma[] = {Pragma.PragmaNameLoc, Pragma.OptionLoc,
                            Pragma.FuncUnitNameLoc,
                            Pragma.InstructionNameLoc,
                            ArgsUnion(Pragma.ValueArg0),
                            ArgsUnion(Pragma.ValueArg1)};
  Attrs.addNew(Pragma.PragmaNameLoc->Ident, Pragma.Range, nullptr,
               Pragma.PragmaNameLoc->Loc, ArgsPragma, 6,
               AttributeCommonInfo::Form::Pragma());
  DS.takeAttributesFrom(Attrs);

  return EndLoc;
}

// Parse Primate pragma annotating a class member (function or nested class)
// use:
//   #pragma primate blue <FU Name> <Instruction> ...
//   #pragma primate reg
void Parser::ParsePragmaPrimateClassMember(AccessSpecifier &AS,
    ParsedAttributes &AccessAttrs, DeclSpec::TST TagType,
    Decl *TagDecl) {
  // Create attribute list.
  ParsedAttributes Attrs(AttrFactory);

  SourceLocation StartLoc = Tok.getLocation();
  SourceLocation EndLoc = SourceLocation{};

  assert(Tok.is(tok::annot_pragma_primate));

  // Get primate pragmas and consume annotated token.
  PrimatePragma Pragma;

  assert(HandlePragmaPrimate(Pragma));
  EndLoc = Pragma.Range.getEnd();

  ArgsUnion ArgsPragma[] = {Pragma.PragmaNameLoc, Pragma.OptionLoc,
                            Pragma.FuncUnitNameLoc,
                            Pragma.InstructionNameLoc,
                            ArgsUnion(Pragma.ValueArg0),
                            ArgsUnion(Pragma.ValueArg1)};
  Attrs.addNew(Pragma.PragmaNameLoc->Ident, Pragma.Range, nullptr,
               Pragma.PragmaNameLoc->Loc, ArgsPragma, 6,
               AttributeCommonInfo::Form::Pragma());

  if (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
      Tok.isNot(tok::eof)) {
    DeclGroupRef DG = ParseCXXClassMemberDeclarationWithPragmas(
        AS, AccessAttrs, TagType, TagDecl).get();
    if (DG.isSingleDecl()) {
      Decl *D = DG.getSingleDecl();
      Actions.ProcessDeclAttributeList(getCurScope(), D, Attrs);
    }
    MaybeDestroyTemplateIds();
  }
}

// Parse Primate pragma annotating a free struct or class
// use:
//   #pragma primate model
//   #pragma primate reg
void Parser::ParsePragmaPrimateFreeRecord() {
}
