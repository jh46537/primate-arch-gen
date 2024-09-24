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


class PrimateBuiltin<string name, string prototype, string intrin_name, string bfname> {
    string Name = name;
    string PType = prototype;
    string IntrinName = intrin_name;
    string BFUName = bfname;
}


def input:      PrimateBuiltin<"__primate_input", "Bi", "primate_input", "IO">;
def inputDone:  PrimateBuiltin<"__primate_input_done", "", "primate_input_done", "IO">;
def output:     PrimateBuiltin<"__primate_output", "Bi", "primate_output", "IO">;
def outputDone: PrimateBuiltin<"__primate_output_done", "", "primate_output_done", "IO">;
