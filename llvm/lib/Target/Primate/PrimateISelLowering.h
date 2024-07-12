//===-- PrimateISelLowering.h - Primate DAG Lowering Interface ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Primate uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PRIMATE_PRIMATEISELLOWERING_H
#define LLVM_LIB_TARGET_PRIMATE_PRIMATEISELLOWERING_H

#include "Primate.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include <optional>

namespace llvm {
class PrimateSubtarget;
struct PrimateRegisterInfo;
namespace PrimateISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,

  EXTRACT,
  INSERT,

  RET_FLAG,
  URET_FLAG,
  SRET_FLAG,
  MRET_FLAG,
  CALL,
  /// Select with condition operator - This selects between a true value and
  /// a false value (ops #3 and #4) based on the boolean result of comparing
  /// the lhs and rhs (ops #0 and #1) of a conditional expression with the
  /// condition code in op #2, a XLenVT constant from the ISD::CondCode enum.
  /// The lhs and rhs are XLenVT integers. The true and false values can be
  /// integer or floating point.
  SELECT_CC,
  BR_CC,
  BuildPairF64,
  SplitF64,
  TAIL,

  // Add the Lo 12 bits from an address. Selected to ADDI.
  ADD_LO,
  // Get the Hi 20 bits from an address. Selected to LUI.
  HI,

  // Represents an AUIPC+ADDI pair. Selected to PseudoLLA.
  LLA,

  // Selected as PseudoAddTPRel. Used to emit a TP-relative relocation.
  ADD_TPREL,

  // Multiply high for signedxunsigned.
  MULHSU,
  // PR64I shifts, directly matching the semantics of the named Primate
  // instructions.
  SLLW,
  SRAW,
  SRLW,
  // 32-bit operations from PR64M that can't be simply matched with a pattern
  // at instruction selection time. These have undefined behavior for division
  // by 0 or overflow (divw) like their target independent counterparts.
  DIVW,
  DIVUW,
  REMUW,
  // PR64IB rotates, directly matching the semantics of the named Primate
  // instructions.
  ROLW,
  RORW,
  // PR64IZbb bit counting instructions directly matching the semantics of the
  // named Primate instructions.
  CLZW,
  CTZW,

  // RV64IZbb absolute value for i32. Expanded to (max (negw X), X) during isel.
  ABSW,

  // PR64IB/PR32IB funnel shifts, with the semantics of the named Primate
  // instructions, but the same operand order as fshl/fshr intrinsics.
  FSR,
  FSL,
  // PR64IB funnel shifts, with the semantics of the named Primate instructions,
  // but the same operand order as fshl/fshr intrinsics.
  FSRW,
  FSLW,
  // FPR<->GPR transfer operations when the FPR is smaller than XLEN, needed as
  // XLEN is the only legal integer width.
  //
  // FMV_H_X matches the semantics of the FMV.H.X.
  // FMV_X_ANYEXTH is similar to FMV.X.H but has an any-extended result.
  // FMV_W_X_PR64 matches the semantics of the FMV.W.X.
  // FMV_X_ANYEXTW_PR64 is similar to FMV.X.W but has an any-extended result.
  //
  // This is a more convenient semantic for producing dagcombines that remove
  // unnecessary GPR->FPR->GPR moves.
  FMV_H_X,
  FMV_X_ANYEXTH,
  FMV_X_SIGNEXTH,
  FMV_W_X_PR64,
  FMV_X_ANYEXTW_PR64,
  // FP to XLen int conversions. Corresponds to fcvt.l(u).s/d/h on RV64 and
  // fcvt.w(u).s/d/h on RV32. Unlike FP_TO_S/UINT these saturate out of
  // range inputs. These are used for FP_TO_S/UINT_SAT lowering. Rounding mode
  // is passed as a TargetConstant operand using the RISCVFPRndMode enum.
  FCVT_X,
  FCVT_XU,
  // FP to 32 bit int conversions for PR64. These are used to keep track of the
  // result being sign extended to 64 bit.
  FCVT_W_PR64,
  FCVT_WU_PR64,

  // Rounds an FP value to its corresponding integer in the same FP format.
  // First operand is the value to round, the second operand is the largest
  // integer that can be represented exactly in the FP format. This will be
  // expanded into multiple instructions and basic blocks with a custom
  // inserter.
  FROUND,

  FCLASS,

   // Floating point fmax and fmin matching the RISC-V instruction semantics.
  FMAX, FMIN,

  // READ_CYCLE_WIDE - A read of the 64-bit cycle CSR on a 32-bit target
  // (returns (Lo, Hi)). It takes a chain operand.
  READ_CYCLE_WIDE,
  
  // brev8, orc.b, zip, and unzip from Zbb and Zbkb. All operands are i32 or
  // XLenVT.
  BREV8,
  ORC_B,
  ZIP,
  UNZIP,

  // Generalized Reverse and Generalized Or-Combine - directly matching the
  // semantics of the named Primate instructions. Lowered as custom nodes as
  // TableGen chokes when faced with commutative permutations in deeply-nested
  // DAGs. Each node takes an input operand and a control operand and outputs a
  // bit-manipulated version of input. All operands are i32 or XLenVT.
  GREV,
  GREVW,
  GORC,
  GORCW,
  SHFL,
  SHFLW,
  UNSHFL,
  UNSHFLW,
  // Bit Compress/Decompress implement the generic bit extract and bit deposit
  // functions. This operation is also referred to as bit gather/scatter, bit
  // pack/unpack, parallel extract/deposit, compress/expand, or right
  // compress/right expand.
  BCOMPRESS,
  BCOMPRESSW,
  BDECOMPRESS,
  BDECOMPRESSW,

  // Reads value of CSR.
  // The first operand is a chain pointer. The second specifies address of the
  // required CSR. Two results are produced, the read value and the new chain
  // pointer.
  READ_CSR,
  // Write value to CSR.
  // The first operand is a chain pointer, the second specifies address of the
  // required CSR and the third is the value to write. The result is the new
  // chain pointer.
  WRITE_CSR,
  // Read and write value of CSR.
  // The first operand is a chain pointer, the second specifies address of the
  // required CSR and the third is the value to write. Two results are produced,
  // the value read before the modification and the new chain pointer.
  SWAP_CSR,

  STRICT_FCVT_W_PR64 = ISD::FIRST_TARGET_STRICTFP_OPCODE,
  STRICT_FCVT_WU_PR64,
  STRICT_FADD_VL,
  STRICT_FSUB_VL,
  STRICT_FMUL_VL,
  STRICT_FDIV_VL,
  STRICT_FSQRT_VL,
  STRICT_VFMADD_VL,
  STRICT_VFNMADD_VL,
  STRICT_VFMSUB_VL,
  STRICT_VFNMSUB_VL,
  STRICT_FP_ROUND_VL,
  STRICT_FP_EXTEND_VL,
  STRICT_VFNCVT_ROD_VL,
  STRICT_SINT_TO_FP_VL,
  STRICT_UINT_TO_FP_VL,
  STRICT_VFCVT_RM_X_F_VL,
  STRICT_VFCVT_RTZ_X_F_VL,
  STRICT_VFCVT_RTZ_XU_F_VL,
  STRICT_FSETCC_VL,
  STRICT_FSETCCS_VL,
  STRICT_VFROUND_NOEXCEPT_VL,
  LAST_PRIMATE_STRICTFP_OPCODE = STRICT_VFROUND_NOEXCEPT_VL,

  // Memory opcodes start here.
  VLE_VL = ISD::FIRST_TARGET_MEMORY_OPCODE,
  VSE_VL,

  // WARNING: Do not add anything in the end unless you want the node to
  // have memop! In fact, starting from FIRST_TARGET_MEMORY_OPCODE all
  // opcodes will be thought as target memory ops!
};
} // namespace PrimateISD

class PrimateTargetLowering : public TargetLowering {
  const PrimateSubtarget &Subtarget;

  std::vector<int> allSizes;
  std::vector<int> allPoses;
  std::vector<int> allSlotInfo;
  std::map<unsigned, unsigned> slotToFUIndex; // maps a subinstruction slot to the Functional unit slot

  enum SlotTypes{
    GREEN,
    BLUE,
    MERGED,
    EXTRACT,
    INSERT,
    BRANCH
  };

public:
  explicit PrimateTargetLowering(const TargetMachine &TM,
                               const PrimateSubtarget &STI);

  const PrimateSubtarget &getSubtarget() const { return Subtarget; }

  bool getTgtMemIntrinsic(IntrinsicInfo &Info, const CallInst &I,
                          MachineFunction &MF,
                          unsigned Intrinsic) const override;
  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AS,
                             Instruction *I = nullptr) const override;
  bool isLegalICmpImmediate(int64_t Imm) const override;
  bool isLegalAddImmediate(int64_t Imm) const override;
  bool isTruncateFree(Type *SrcTy, Type *DstTy) const override;
  bool isTruncateFree(EVT SrcVT, EVT DstVT) const override;
  bool isZExtFree(SDValue Val, EVT VT2) const override;
  bool isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const override;
  bool isCheapToSpeculateCttz(Type *Ty) const override;
  bool isCheapToSpeculateCtlz(Type *Ty) const override;
  bool isFPImmLegal(const APFloat &Imm, EVT VT,
                    bool ForCodeSize) const override;

  // size 32 pos 0
  unsigned int getScalarField() const {
    int posBits = 32 - __builtin_clz(allPoses.size());
    int sizeBits = 32 - __builtin_clz(allSizes.size());
    if(find(allSizes.begin(), allSizes.end(), 32) == allSizes.end()) {
      llvm_unreachable("unsupported struct element size");
    }
    int sizeIdx = std::distance(allSizes.begin(), find(allSizes.begin(), allSizes.end(), 32));

    dbgs() << "posSize: " << allPoses.size() << " sizeSize: " << allSizes.size() << "\n";
    dbgs() << "Get scalar field as: " << sizeIdx << " Size bits: " << sizeBits << " posBits: " << posBits << "\n";

    return (sizeIdx & ((1 << sizeBits) - 1)) << posBits;
  }

  // max size pos 0
  unsigned int getWholeRegField() const {
    int posBits = 32 - __builtin_clz(allPoses.size());
    int sizeBits = 32 - __builtin_clz(allSizes.size());
    int sizeIdx = allSizes.size() - 1;

    dbgs() << "posSize: " << allPoses.size() << " sizeSize: " << allSizes.size() << "\n";
    dbgs() << "Get scalar field as: " << sizeIdx << " Size bits: " << sizeBits << " posBits: " << posBits << "\n";

    return (sizeIdx & ((1 << sizeBits) - 1)) << posBits;
  }

  virtual unsigned int getSlotFUIndex(unsigned int slotIdx) const {
    if(slotIdx > slotToFUIndex.size()) {
      llvm_unreachable("tried to get FU index of a to large slot");
    }
    return slotToFUIndex.at(slotIdx);
  }
  
  // TODO: Move to subtarget
  virtual bool isSlotBFU(unsigned int slotIdx) const {
    if(slotIdx > allSlotInfo.size()) {
      llvm_unreachable("tried to check slot info of a too large slot");
    }
    return allSlotInfo[slotIdx] == SlotTypes::BLUE;
  }
  virtual bool isSlotGFU(unsigned int slotIdx) const {
    if(slotIdx > allSlotInfo.size()) {
      llvm_unreachable("tried to check slot info of a too large slot");
    }
    return allSlotInfo[slotIdx] == SlotTypes::GREEN;
  }
  virtual bool isSlotMergedFU(unsigned int slotIdx) const {
    if(slotIdx > allSlotInfo.size()) {
      llvm_unreachable("tried to check slot info of a too large slot");
    }
    return allSlotInfo[slotIdx] == SlotTypes::MERGED;
  }
  virtual bool isSlotExtract(unsigned int slotIdx) const {
    if(slotIdx > allSlotInfo.size()) {
      llvm_unreachable("tried to check slot info of a too large slot");
    }
    return allSlotInfo[slotIdx] == SlotTypes::EXTRACT;
  }
  virtual bool isSlotInsert(unsigned int slotIdx) const {
    if(slotIdx > allSlotInfo.size()) {
      llvm_unreachable("tried to check slot info of a too large slot");
    }
    return allSlotInfo[slotIdx] == SlotTypes::INSERT;
  }
  
  virtual unsigned int linearToAggregateIndex(StructType &STy, unsigned int linearIndex) const override {
    // first find a field that fits this width, then attempt to put it at the bottom of that field.

    int posBits = 32 - __builtin_clz(allPoses.size());
    int sizeBits = 32 - __builtin_clz(allSizes.size());
    int bitPos = 0;
    for(unsigned i = 0; i < linearIndex; i++) {
      bitPos += STy.getElementType(i)->getScalarSizeInBits();
      dbgs() << "bit pos currently: " << bitPos << "\n";
    }
    if(find(allPoses.begin(), allPoses.end(), bitPos) == allPoses.end()) {
      llvm_unreachable("unsupported struct position");
    }
    int posIdx = std::distance(allPoses.begin(), find(allPoses.begin(), allPoses.end(), bitPos));
    if(find(allSizes.begin(), allSizes.end(), STy.getElementType(linearIndex)->getScalarSizeInBits()) == allSizes.end()) {
      llvm_unreachable("unsupported struct element size");
    }
    int sizeIdx = std::distance(allSizes.begin(), find(allSizes.begin(), allSizes.end(), STy.getElementType(linearIndex)->getScalarSizeInBits()));
    
    dbgs() << "posSize: " << allPoses.size() << " sizeSize: " << allSizes.size() << "\n";
    dbgs() << "Get scalar field as: " << sizeIdx << " Size bits: " << sizeBits << " posIdx: " << posIdx << " posBits: " << posBits << "\n";
    dbgs() << "final index is: " << ((sizeIdx & ((1 << sizeBits) - 1)) << posBits) + (posIdx & ((1<<posBits) - 1)) << "\n";


    return ((sizeIdx & ((1 << sizeBits) - 1)) << posBits) + (posIdx & ((1<<posBits) - 1));
  }

  virtual bool supportedAggregate(StructType &STy) const override;

  // returns the EVT of a given aggregate if its supported by the target.
  virtual EVT getAggregateVT(StructType &STy) const override {
    return EVT(MVT::Primate_aggregate);
  }


  bool softPromoteHalfType() const override { return true; }

  /// Return the register type for a given MVT, ensuring vectors are treated
  /// as a series of gpr sized integers.
  MVT getRegisterTypeForCallingConv(LLVMContext &Context, CallingConv::ID CC,
                                    EVT VT) const override;

  /// Return the number of registers for a given MVT, ensuring vectors are
  /// treated as a series of gpr sized integers.
  unsigned getNumRegistersForCallingConv(LLVMContext &Context,
                                         CallingConv::ID CC,
                                         EVT VT) const override;

  /// Return true if the given shuffle mask can be codegen'd directly, or if it
  /// should be stack expanded.
  bool isShuffleMaskLegal(ArrayRef<int> M, EVT VT) const override;

  bool
  shouldExpandBuildVectorWithShuffles(EVT VT,
                                      unsigned DefinedValues) const override;

  // Provide custom lowering hooks for some operations.
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  bool targetShrinkDemandedConstant(SDValue Op, const APInt &DemandedBits,
                                    const APInt &DemandedElts,
                                    TargetLoweringOpt &TLO) const override;

  void computeKnownBitsForTargetNode(const SDValue Op,
                                     KnownBits &Known,
                                     const APInt &DemandedElts,
                                     const SelectionDAG &DAG,
                                     unsigned Depth) const override;
  unsigned ComputeNumSignBitsForTargetNode(SDValue Op,
                                           const APInt &DemandedElts,
                                           const SelectionDAG &DAG,
                                           unsigned Depth) const override;

  // This method returns the name of a target specific DAG node.
  const char *getTargetNodeName(unsigned Opcode) const override;

  ConstraintType getConstraintType(StringRef Constraint) const override;

  InlineAsm::ConstraintCode getInlineAsmMemConstraint(StringRef ConstraintCode) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  bool convertSetCCLogicToBitwiseLogic(EVT VT) const override {
    return VT.isScalarInteger();
  }
  bool convertSelectOfConstantsToMath(EVT VT) const override { return true; }

  bool shouldInsertFencesForAtomic(const Instruction *I) const override {
    return isa<LoadInst>(I) || isa<StoreInst>(I);
  }
  Instruction *emitLeadingFence(IRBuilderBase &Builder, Instruction *Inst,
                                AtomicOrdering Ord) const override;
  Instruction *emitTrailingFence(IRBuilderBase &Builder, Instruction *Inst,
                                 AtomicOrdering Ord) const override;

  bool isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                  EVT VT) const override;

  ISD::NodeType getExtendForAtomicOps() const override {
    return ISD::SIGN_EXTEND;
  }

  ISD::NodeType getExtendForAtomicCmpSwapArg() const override {
    return ISD::SIGN_EXTEND;
  }

  bool isDesirableToCommuteWithShift(const SDNode *N,
                                     CombineLevel Level) const override;

  /// If a physical register, this returns the register that receives the
  /// exception address on entry to an EH pad.
  Register
  getExceptionPointerRegister(const Constant *PersonalityFn) const override;

  /// If a physical register, this returns the register that receives the
  /// exception typeid on entry to a landing pad.
  Register
  getExceptionSelectorRegister(const Constant *PersonalityFn) const override;

  bool shouldExtendTypeInLibCall(EVT Type) const override;
  bool shouldSignExtendTypeInLibCall(EVT Type, bool IsSigned) const override;

  /// Returns the register with the specified architectural or ABI name. This
  /// method is necessary to lower the llvm.read_register.* and
  /// llvm.write_register.* intrinsics. Allocatable registers must be reserved
  /// with the clang -ffixed-xX flag for access to be allowed.
  Register getRegisterByName(const char *RegName, LLT VT,
                             const MachineFunction &MF) const override;

  // Lower incoming arguments, copy physregs into vregs
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context) const override;
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                         Type *Ty) const override {
    return true;
  }
  bool mayBeEmittedAsTailCall(const CallInst *CI) const override;
  bool shouldConsiderGEPOffsetSplit() const override { return true; }

  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  TargetLowering::AtomicExpansionKind
  shouldExpandAtomicRMWInIR(AtomicRMWInst *AI) const override;
  Value *emitMaskedAtomicRMWIntrinsic(IRBuilderBase &Builder, AtomicRMWInst *AI,
                                      Value *AlignedAddr, Value *Incr,
                                      Value *Mask, Value *ShiftAmt,
                                      AtomicOrdering Ord) const override;
  TargetLowering::AtomicExpansionKind
  shouldExpandAtomicCmpXchgInIR(AtomicCmpXchgInst *CI) const override;
  Value *emitMaskedAtomicCmpXchgIntrinsic(IRBuilderBase &Builder,
                                          AtomicCmpXchgInst *CI,
                                          Value *AlignedAddr, Value *CmpVal,
                                          Value *NewVal, Value *Mask,
                                          AtomicOrdering Ord) const override;

  /// Returns true if the target allows unaligned memory accesses of the
  /// specified type.
  bool allowsMisalignedMemoryAccesses(
      EVT, unsigned AddrSpace = 0, Align Alignment = Align(1),
      MachineMemOperand::Flags Flags = MachineMemOperand::MONone,
      unsigned * /*Fast*/ = nullptr) const override;

  bool splitValueIntoRegisterParts(SelectionDAG &DAG, const SDLoc &DL,
                                   SDValue Val, SDValue *Parts,
                                   unsigned NumParts, MVT PartVT,
                                   std::optional<CallingConv::ID> CC) const override;

  SDValue
  joinRegisterPartsIntoValue(SelectionDAG &DAG, const SDLoc &DL,
                             const SDValue *Parts, unsigned NumParts,
                             MVT PartVT, EVT ValueVT,
                             std::optional<CallingConv::ID> CC) const override;

  static PrimateII::VLMUL getLMUL(MVT VT);
  static unsigned getRegClassIDForLMUL(PrimateII::VLMUL LMul);
  static unsigned getSubregIndexByMVT(MVT VT, unsigned Index);
  static unsigned getRegClassIDForVecVT(MVT VT);
  static std::pair<unsigned, unsigned>
  decomposeSubvectorInsertExtractToSubRegs(MVT VecVT, MVT SubVecVT,
                                           unsigned InsertExtractIdx,
                                           const PrimateRegisterInfo *TRI);
  MVT getContainerForFixedLengthVector(MVT VT) const;

  bool shouldRemoveExtendFromGSIndex(SDValue Extend, EVT DataVT) const override;

private:
  /// PrimateCCAssignFn - This target-specific function extends the default
  /// CCValAssign with additional information used to lower Primate calling
  /// conventions.
  typedef bool PrimateCCAssignFn(const DataLayout &DL, PrimateABI::ABI,
                               unsigned ValNo, MVT ValVT, MVT LocVT,
                               CCValAssign::LocInfo LocInfo,
                               ISD::ArgFlagsTy ArgFlags, CCState &State,
                               bool IsFixed, bool IsRet, Type *OrigTy,
                               const PrimateTargetLowering &TLI,
                               std::optional<unsigned> FirstMaskArgument);

  void analyzeInputArgs(MachineFunction &MF, CCState &CCInfo,
                        const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet,
                        PrimateCCAssignFn Fn) const;
  void analyzeOutputArgs(MachineFunction &MF, CCState &CCInfo,
                         const SmallVectorImpl<ISD::OutputArg> &Outs,
                         bool IsRet, CallLoweringInfo *CLI,
                         PrimateCCAssignFn Fn) const;

  template <class NodeTy>
  SDValue getAddr(NodeTy *N, SelectionDAG &DAG, bool IsLocal = true) const;

  SDValue getStaticTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG,
                           bool UseGOT) const;
  SDValue getDynamicTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG) const;

  SDValue lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSELECT(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerBRCOND(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftLeftParts(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftRightParts(SDValue Op, SelectionDAG &DAG, bool IsSRA) const;
  SDValue lowerSPLAT_VECTOR_PARTS(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVectorMaskSplat(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVectorMaskExt(SDValue Op, SelectionDAG &DAG,
                             int64_t ExtTrueVal) const;
  SDValue lowerVectorMaskTrunc(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerINSERT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerEXTRACT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_W_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVECREDUCE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVectorMaskVECREDUCE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFPVECREDUCE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerINSERT_SUBVECTOR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerEXTRACT_SUBVECTOR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSTEP_VECTOR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVECTOR_REVERSE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerABS(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerMLOAD(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerMSTORE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFixedLengthVectorFCOPYSIGNToPRV(SDValue Op,
                                               SelectionDAG &DAG) const;
  SDValue lowerMGATHER(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerMSCATTER(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFixedLengthVectorLoadToPRV(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFixedLengthVectorStoreToPRV(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFixedLengthVectorSetccToPRV(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFixedLengthVectorLogicOpToPRV(SDValue Op, SelectionDAG &DAG,
                                             unsigned MaskOpc,
                                             unsigned VecOpc) const;
  SDValue lowerFixedLengthVectorShiftToPRV(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFixedLengthVectorSelectToPRV(SDValue Op,
                                            SelectionDAG &DAG) const;
  SDValue lowerToScalableOp(SDValue Op, SelectionDAG &DAG, unsigned NewOpc,
                            bool HasMask = true) const;
  SDValue lowerVPOp(SDValue Op, SelectionDAG &DAG, unsigned PrimateISDOpc) const;
  SDValue lowerFixedLengthVectorExtendToPRV(SDValue Op, SelectionDAG &DAG,
                                            unsigned ExtendOpc) const;
  SDValue lowerGET_ROUNDING(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSET_ROUNDING(SDValue Op, SelectionDAG &DAG) const;

  SDValue expandUnalignedPRVLoad(SDValue Op, SelectionDAG &DAG) const;
  SDValue expandUnalignedPRVStore(SDValue Op, SelectionDAG &DAG) const;

  bool isEligibleForTailCallOptimization(
      CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
      const SmallVector<CCValAssign, 16> &ArgLocs) const;

  /// Generate error diagnostics if any register used by CC has been marked
  /// reserved.
  void validateCCReservedRegs(
      const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
      MachineFunction &MF) const;

  bool usePRVForFixedLengthVectorVT(MVT VT) const;

  MVT getVPExplicitVectorLengthTy() const override;

  /// PRV code generation for fixed length vectors does not lower all
  /// BUILD_VECTORs. This makes BUILD_VECTOR legalisation a source of stores to
  /// merge. However, merging them creates a BUILD_VECTOR that is just as
  /// illegal as the original, thus leading to an infinite legalisation loop.
  /// NOTE: Once BUILD_VECTOR can be custom lowered for all legal vector types,
  /// this override can be removed.
  bool mergeStoresAfterLegalization(EVT VT) const override;
};

namespace Primate {
// We use 64 bits as the known part in the scalable vector types.
static constexpr unsigned PRVBitsPerBlock = 64;
} // namespace Primate

namespace PrimateVIntrinsicsTable {

struct PrimateVIntrinsicInfo {
  unsigned IntrinsicID;
  uint8_t SplatOperand;
};

using namespace Primate;

#define GET_PrimateVIntrinsicsTable_DECL
#include "PrimateGenSearchableTables.inc"

} // end namespace PrimateVIntrinsicsTable

} // end namespace llvm

#endif
