
  //===- PrimateInstrInfoBFU.td - Target Description for Primate *- tablegen -*-===//
  //
  // Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===-------------------------------------------------------------------------===//
  //
  // This file describes the Primate BFU instructions in TableGen format.
  //
  //===-------------------------------------------------------------------------===//

  let Itinerary = ItinBlue0 in
  let hasSideEffects = 1, mayLoad = 1, mayStore = 1 in
  def BFU0 :
      PRInstI<0b000, OPC_PR_ASCII, (outs WIDEREG:$rd), (ins WIDEREG:$rs1),
          "bfu0", "$rd, $rs1">, Sched<[WriteIALU, ReadIALU]> {
            let IsBFUInstruction = 1;
            let imm12 = 0;
          }
  

  def : Pat<(int_primate_BFU_0 WIDEREG:$rs1), (BFU0 WIDEREG:$rs1)>;


  def : Pat<(int_primate_BFU_0 WIDEREG:$rs1), (BFU0 WIDEREG:$rs1)>;

  
