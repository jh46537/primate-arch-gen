
#include "PrimateMachineFunctionInfo.h"

using namespace llvm;

yaml::PrimateMachineFunctionInfo::PrimateMachineFunctionInfo(const llvm::PrimateMachineFunctionInfo &MFI): VarArgsFrameIndex(MFI.getVarArgsFrameIndex()),
													   VarArgsSaveSize(MFI.getVarArgsSaveSize()) {}

void yaml::PrimateMachineFunctionInfo::mappingImpl(yaml::IO &YamlIO)  {
  MappingTraits<PrimateMachineFunctionInfo>::mapping(YamlIO, *this);
}


MachineFunctionInfo *PrimateMachineFunctionInfo::clone(
    BumpPtrAllocator &Allocator, MachineFunction &DestMF,
    const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
    const {
  return DestMF.cloneInfo<PrimateMachineFunctionInfo>(*this);
}

void PrimateMachineFunctionInfo::initializeBaseYamlFields(
    const yaml::PrimateMachineFunctionInfo &YamlMFI) {
  VarArgsFrameIndex = YamlMFI.VarArgsFrameIndex;
  VarArgsSaveSize = YamlMFI.VarArgsSaveSize;
}
