add_llvm_component_library(LLVMPrimateInfo
  PrimateTargetInfo.cpp

  LINK_COMPONENTS
  Support

  ADD_TO_COMPONENT
  Primate
  )
