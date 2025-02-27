//===-- PrimateRegisterBankInfo.cpp -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the targeting of the RegisterBankInfo class for Primate.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#include "PrimateRegisterBankInfo.h"
#include "MCTargetDesc/PrimateMCTargetDesc.h"
#include "llvm/CodeGen/RegisterBank.h"
#include "llvm/CodeGen/RegisterBankInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_TARGET_REGBANK_IMPL
#include "PrimateGenRegisterBank.inc"

using namespace llvm;

PrimateRegisterBankInfo::PrimateRegisterBankInfo(const TargetRegisterInfo &TRI)
    : PrimateGenRegisterBankInfo() {}
