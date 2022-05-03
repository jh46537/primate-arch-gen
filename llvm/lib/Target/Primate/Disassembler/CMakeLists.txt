add_llvm_component_library(LLVMPrimateDisassembler
  PrimateDisassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  PrimateDesc
  PrimateInfo
  Support

  ADD_TO_COMPONENT
  Primate
  )
