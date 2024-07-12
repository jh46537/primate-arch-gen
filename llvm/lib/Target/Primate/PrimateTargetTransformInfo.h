//===- PrimateTargetTransformInfo.h - Primate specific TTI ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file defines a TargetTransformInfo::Concept conforming object specific
/// to the Primate target machine. It uses the target's detailed information to
/// provide more precise answers to certain TTI queries, while letting the
/// target independent and default TTI implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATETARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATETARGETTRANSFORMINFO_H

#include "PrimateSubtarget.h"
#include "PrimateTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/IVDescriptors.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/Function.h"

namespace llvm {

class PrimateTTIImpl : public BasicTTIImplBase<PrimateTTIImpl> {
  using BaseT = BasicTTIImplBase<PrimateTTIImpl>;
  using TTI = TargetTransformInfo;

  friend BaseT;

  const PrimateSubtarget *ST;
  const PrimateTargetLowering *TLI;

  const PrimateSubtarget *getST() const { return ST; }
  const PrimateTargetLowering *getTLI() const { return TLI; }

public:
  explicit PrimateTTIImpl(const PrimateTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind);
  InstructionCost getIntImmCostInst(unsigned Opcode, unsigned Idx,
                                    const APInt &Imm, Type *Ty,
                                    TTI::TargetCostKind CostKind,
                                    Instruction *Inst = nullptr);
  InstructionCost getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx,
                                      const APInt &Imm, Type *Ty,
                                      TTI::TargetCostKind CostKind);

  TargetTransformInfo::PopcntSupportKind getPopcntSupport(unsigned TyWidth);

  bool shouldExpandReduction(const IntrinsicInst *II) const;
  bool supportsScalableVectors() const { return ST->hasStdExtV(); }
  std::optional<unsigned> getMaxVScale() const;

  TypeSize getRegisterBitWidth(TargetTransformInfo::RegisterKind K) const {
    switch (K) {
    case TargetTransformInfo::RGK_Scalar:
      return TypeSize::getFixed(ST->getXLen());
    case TargetTransformInfo::RGK_FixedWidthVector:
      return TypeSize::getFixed(
          ST->hasStdExtV() ? ST->getMinPRVVectorSizeInBits() : 0);
    case TargetTransformInfo::RGK_ScalableVector:
      return TypeSize::getScalable(
          ST->hasStdExtV() ? ST->getMinPRVVectorSizeInBits() : 0);
    }

    llvm_unreachable("Unsupported register kind");
  }

  InstructionCost getGatherScatterOpCost(unsigned Opcode, Type *DataTy,
                                         const Value *Ptr, bool VariableMask,
                                         Align Alignment,
                                         TTI::TargetCostKind CostKind,
                                         const Instruction *I);

  bool isLegalElementTypeForPRV(Type *ScalarTy) const {
    if (ScalarTy->isPointerTy())
      return true;

    if (ScalarTy->isIntegerTy(8) || ScalarTy->isIntegerTy(16) ||
        ScalarTy->isIntegerTy(32) || ScalarTy->isIntegerTy(64))
      return true;

    if (ScalarTy->isHalfTy())
      return ST->hasStdExtZfh();
    if (ScalarTy->isFloatTy())
      return ST->hasStdExtF();
    if (ScalarTy->isDoubleTy())
      return ST->hasStdExtD();

    return false;
  }

  bool isLegalMaskedLoadStore(Type *DataType, Align Alignment) {
    if (!ST->hasStdExtV())
      return false;

    // Only support fixed vectors if we know the minimum vector size.
    if (isa<FixedVectorType>(DataType) && ST->getMinPRVVectorSizeInBits() == 0)
      return false;

    if (Alignment <
        DL.getTypeStoreSize(DataType->getScalarType()))
      return false;

    return isLegalElementTypeForPRV(DataType->getScalarType());
  }

  bool isLegalMaskedLoad(Type *DataType, Align Alignment) {
    return isLegalMaskedLoadStore(DataType, Alignment);
  }
  bool isLegalMaskedStore(Type *DataType, Align Alignment) {
    return isLegalMaskedLoadStore(DataType, Alignment);
  }

  bool isLegalMaskedGatherScatter(Type *DataType, Align Alignment) {
    if (!ST->hasStdExtV())
      return false;

    // Only support fixed vectors if we know the minimum vector size.
    if (isa<FixedVectorType>(DataType) && ST->getMinPRVVectorSizeInBits() == 0)
      return false;

    if (Alignment <
        DL.getTypeStoreSize(DataType->getScalarType()))
      return false;

    return isLegalElementTypeForPRV(DataType->getScalarType());
  }

  bool isLegalMaskedGather(Type *DataType, Align Alignment) {
    return isLegalMaskedGatherScatter(DataType, Alignment);
  }
  bool isLegalMaskedScatter(Type *DataType, Align Alignment) {
    return isLegalMaskedGatherScatter(DataType, Alignment);
  }

  /// \returns How the target needs this vector-predicated operation to be
  /// transformed.
  TargetTransformInfo::VPLegalization
  getVPLegalizationStrategy(const VPIntrinsic &PI) const {
    using VPLegalization = TargetTransformInfo::VPLegalization;
    return VPLegalization(VPLegalization::Legal, VPLegalization::Legal);
  }

  bool isLegalToVectorizeReduction(const RecurrenceDescriptor &RdxDesc,
                                   ElementCount VF) const {
    if (!ST->hasStdExtV())
      return false;

    if (!VF.isScalable())
      return true;

    Type *Ty = RdxDesc.getRecurrenceType();
    if (!isLegalElementTypeForPRV(Ty))
      return false;

    switch (RdxDesc.getRecurrenceKind()) {
    case RecurKind::Add:
    case RecurKind::FAdd:
    case RecurKind::And:
    case RecurKind::Or:
    case RecurKind::Xor:
    case RecurKind::SMin:
    case RecurKind::SMax:
    case RecurKind::UMin:
    case RecurKind::UMax:
    case RecurKind::FMin:
    case RecurKind::FMax:
      return true;
    default:
      return false;
    }
  }

  unsigned getMaxInterleaveFactor(ElementCount VF) {
    return ST->getMaxInterleaveFactor();
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_Primate_PrimateTARGETTRANSFORMINFO_H
