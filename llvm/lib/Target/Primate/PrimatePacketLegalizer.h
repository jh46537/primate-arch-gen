/*
 * Primate Packet Legalizer
 * Required pass that takes packetized instructions and creates operand gather/scatter
 * instructions. 
 * This is better than the old approach as we are accepting that we missed all 
 * old optimizations and are going to packetize as is. no hope for finding opts, this is pure correctnesss
 * OPMerge pass is responsible for finding the optimizations that we would like
 */

#ifndef PRIMATEPACKETLEGALIZE_H_
#define PRIMATEPACKETLEGALIZE_H_

#include "llvm/Pass.h"

#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "PrimateInstrInfo.h"
#include "PrimateSubtarget.h"

#define DEBUG_TYPE "primate-packet-legalizer"

namespace llvm {
class PrimatePacketLegalizer : public MachineFunctionPass {

public:
    static char ID;
    PrimatePacketLegalizer() : MachineFunctionPass(ID){};

    bool runOnMachineFunction(MachineFunction& MF) override;

private:
    const PrimateTargetLowering *TLI;
    const PrimateInstrInfo *PII;
    const TargetRegisterInfo *TRI;
    DFAPacketizer* ResourceTracker;
    bool isWideReg(const Register) const;
    void fixBundle(MachineInstr *BundleMI);
    void fixBFUInstr(SmallVector<MachineInstr*>& newBundle, SmallVector<bool>& isNewInstr, MachineInstr* BundleMI, int slotIdx);
    bool hasScalarRegs(MachineInstr*);
    bool hasScalarDefs(MachineInstr*);
    bool hasScalarOps(MachineInstr*);
};

char PrimatePacketLegalizer::ID = 0;
static RegisterPass<PrimatePacketLegalizer> X("PrimatePacketLegalizer", "Primate Packet Legalizer",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
}

#endif
