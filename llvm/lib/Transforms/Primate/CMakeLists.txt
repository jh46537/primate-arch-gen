# add_llvm_library(LLVMPrimate MODULE
# 	PrimateArchGen.cpp

# 	PLUGIN_TOOL
# 	opt
# )
add_llvm_component_library(LLVMPrimateArchGen
	PrimateArchGen.cpp
    
    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Primate

    # DEPENDS
    # intrinsics_gen

    LINK_COMPONENTS
    Analysis
    Core
    Support
    # Primate
    TargetParser
)
