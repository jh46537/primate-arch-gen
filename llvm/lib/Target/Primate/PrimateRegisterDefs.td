
  // Integer registers
  // CostPerUse is set higher for registers that may not be compressible as they
  // are not part of GPRC, the most restrictive register class used by the
  // compressed instruction set. This will influence the greedy register
  // allocator to reduce the use of registers that can't be encoded in 16 bit
  // instructions.
  foreach Index = 1-31 in {
    // Pseudo regs
    let RegAltNameIndices = [ABIRegAltName] in {
      def X#Index  : PrimateReg<Index, "x"#Index, ["x"#Index]>, DwarfRegNum<[Index]>;
      def H#Index  : PrimateReg<Index, "h"#Index, ["h"#Index]>, DwarfRegNum<[Index]>{
        let SubRegIndices = [gpr_idx];
        let SubRegs = [!cast<Register>("X"#Index)];
      }
      def P#Index : PrimateReg<Index, "p"#Index, ["p"#Index]>, DwarfRegNum<[Index]> {
        let SubRegIndices = [Pri_hanger];
        let SubRegs = [!cast<Register>("H"#Index)];
        //let CoveredBySubRegs = true;
      }
    }
  }
  
