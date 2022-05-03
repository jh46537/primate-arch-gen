add_llvm_component_library(LLVMPrimateDesc
  PrimateAsmBackend.cpp
  PrimateBaseInfo.cpp
  PrimateELFObjectWriter.cpp
  PrimateInstPrinter.cpp
  PrimateMCAsmInfo.cpp
  PrimateMCCodeEmitter.cpp
  PrimateMCExpr.cpp
  PrimateMCTargetDesc.cpp
  PrimateMatInt.cpp
  PrimateTargetStreamer.cpp
  PrimateELFStreamer.cpp

  LINK_COMPONENTS
  MC
  PrimateInfo
  Support

  ADD_TO_COMPONENT
  Primate
)
