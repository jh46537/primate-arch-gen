//===- PrimateVEmitter.cpp - Generate primate_vector.h for use with clang -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This tablegen backend is responsible for emitting primate_vector.h which
// includes a declaration and definition of each intrinsic functions specified
// in https://github.com/primate/prv-intrinsic-doc.
//
// See also the documentation in include/clang/Basic/primate_vector.td.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Twine.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include <numeric>

using namespace llvm;
using BasicType = char;
using VScaleVal = std::optional<unsigned>;

namespace {

class PRVEmitter {
private:
 RecordKeeper &Records;
 std::string HeaderCode;
 // Concat BasicType, LMUL and Proto as key

public:
 PRVEmitter(RecordKeeper &R) : Records(R) {}

 /// Emit primate_vector.h
 void createHeader(raw_ostream &o);

 /// Emit all the __builtin prototypes and code needed by Sema.
 void createBuiltins(raw_ostream &o);

 /// Emit all the information needed to map builtin -> LLVM IR intrinsic.
 void createCodeGen(raw_ostream &o);

 std::string getSuffixStr(char Type, int Log2LMUL, StringRef Prototypes);

private:
    void EmitTypesIntrin(raw_ostream &o, StringRef Prototypes);
};

void PRVEmitter::EmitTypesIntrin(raw_ostream &o, StringRef Prototype) {
    for (auto t : Prototype) {
        switch(t) {
        case 'i':
            o << "IntrinsicTypes.push_back(llvm::IntegerType::get(getLLVMContext(), 32));\n";
            break;
        case 'B':
            o << "IntrinsicTypes.push_back(llvm::PointerType::getUnqual(getLLVMContext()));\n";
            break;
        case 'v':
            o << "IntrinsicTypes.push_back(llvm::Type::getVoidTy(getLLVMContext()));\n";
            break;
        default:
            llvm_unreachable("unsupported type");
        }
    }
}

void PRVEmitter::createCodeGen(raw_ostream &o) {
    o << "/*===---- primate_bfu_buitin_cg.inc - Primate BFU builtins  "
         "-------------------===\n";
    o << " */\n";

    for(Record* Rec: Records.getAllDerivedDefinitions("PrimateBuiltin")) {
        //Record *Rec = R.get();
        StringRef Name = Rec->getValueAsString("Name");
        StringRef PType = Rec->getValueAsString("PType");
        StringRef ITName = Rec->getValueAsString("IntrinName");

        o << "case Primate::BI" << Name << ":\n";
        o << "ID = Intrinsic::" << ITName << ";\n";
        EmitTypesIntrin(o, PType);
        o << "break;\n";
    }
}

void PRVEmitter::createBuiltins(raw_ostream &o) {
    o << "/*===---- primate_bfu_buitin_sema.inc - Primate BFU builtins "
         "-------------------===\n";
    o << " */\n";
    for(Record* Rec: Records.getAllDerivedDefinitions("PrimateBuiltin")) {
        //Record *Rec = R.get();
        StringRef Name = Rec->getValueAsString("Name");
        StringRef PType = Rec->getValueAsString("PType");
        StringRef ITName = Rec->getValueAsString("IntrinName");

        o << "case Primate::BI" << Name << ":\n";
        o << "return false;\n";
        o << "break;\n";
    }
}

} // namespace

namespace clang {
void EmitPrimateBFUBuiltinCG(RecordKeeper &Records, raw_ostream &OS) {
 PRVEmitter(Records).createCodeGen(OS);
}
void EmitPrimateBFUBuiltinSema(RecordKeeper &Records, raw_ostream &OS) {
 PRVEmitter(Records).createBuiltins(OS);
}
} // End namespace clang
