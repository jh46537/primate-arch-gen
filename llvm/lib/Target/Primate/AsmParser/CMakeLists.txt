add_llvm_component_library(LLVMPrimateAsmParser
  PrimateAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  PrimateDesc
  PrimateInfo
  Support

  ADD_TO_COMPONENT
  Primate
  )
