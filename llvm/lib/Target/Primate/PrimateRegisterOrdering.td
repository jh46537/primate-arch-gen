// The order of registers represents the preferred allocation sequence.
  // Registers are listed in the order caller-save, callee-save, specials.
  def GPR : GPRRegisterClass<(add (sequence "X%u", 0, 127))>;

  // The order of registers represents the preferred allocation sequence.
  // Registers are listed in the order caller-save, callee-save, specials.
  def GPR128 : RegisterClass<"Primate", [HoldingRegVT], 32, (add
      (sequence "H%u", 0, 127)
    )> {
    let RegInfos = HoldingRegRI;
  }

  def WIDEREG : RegisterClass<"Primate", [PrimateAGGVT], 32, (add 
      (sequence "P%u", 0, 127)
    )> {
      let RegInfos = PrimateAGGRI;
    }
  
