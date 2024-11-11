
  // name is the builtin name from clang/include/clang/Basic/BuiltinsPrimate.def
  //
  // prototype gets one char per argument
  // example: BBi is a built in that returns BFU, and takes BFU, integer
  // supported types:
  //      B: BFU type
  //      v: void
  //      i: integers
  //
  // intrin_name is the name of the backend intrinsic to use
  // defined in llvm/include/llvm/IR/IntrinsicsPrimate.td (strip the int_ from the tablegen name)
  // Prototype should only specify the llvm_any_ty from the intrinsics


  class PrimateBuiltin<string name, string prototype, string intrin_name, string bfu_name> {
      string Name = name;
      string PType = prototype;
      string IntrinName = intrin_name;
      string BFUName = bfu_name;
  }


  def input:      PrimateBuiltin<"__primate_input", "Bi", "primate_BFU_IO_input", "IO">;
  def inputDone:  PrimateBuiltin<"__primate_input_done", "", "primate_BFU_IO_input_done", "IO">;
  def output:     PrimateBuiltin<"__primate_output", "Bi", "primate_BFU_IO_output", "IO">;
  def outputDone: PrimateBuiltin<"__primate_output_done", "", "primate_BFU_IO_output_done", "IO">;
  def BFU_aes128:      PrimateBuiltin<"__primate_BFU_aes128", "BB", "primate_BFU_aes128", "aes128">;
  
