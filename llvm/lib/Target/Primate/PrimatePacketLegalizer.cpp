

#include "PrimatePacketLegalizer.h"
#include "Primate.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

namespace llvm {

// Goal of this pass: we would like to merge extract with the 
// operation that is doing the extraction
// if 

bool PrimatePacketLegalizer::runOnMachineFunction(MachineFunction& MF) {
    TLI = static_cast<const PrimateTargetLowering *>(MF.getSubtarget().getTargetLowering());
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    ResourceTracker = TII->CreateTargetScheduleState(MF.getSubtarget());
    PII = MF.getSubtarget<PrimateSubtarget>().getInstrInfo();
    TRI = MF.getSubtarget().getRegisterInfo();

    dbgs() << "hello from Primate Packet Legalizer\n";
    MF.dump();
    dbgs() << "starting\n";
    SmallVector<MachineInstr*> worklist; 
    for(MachineBasicBlock &MBB : MF) {
        worklist.clear();
        for(MachineInstr &BundleMI: MBB) {
            BundleMI.dump();
            if(BundleMI.getOpcode() != Primate::BUNDLE) {
                dbgs() << "found a non-bundle instr. skipping for now.\n";
                continue;
            }

            auto pktStart = getBundleStart(BundleMI.getIterator());
            auto pktEnd = getBundleEnd(BundleMI.getIterator());
            auto it = ++pktStart;
            int count = 0;
            while(it != pktEnd) {it++; count++;}
            if(count == 0) {
                dbgs() << "empty packet\n";
            }
            bool need_to_legal = false;
            for(auto curInst = pktStart; curInst != pktEnd; curInst++) {
                if(hasScalarRegs(&*curInst)) {
                    need_to_legal = true;
                }
            }
            if (need_to_legal) {
                worklist.push_back(&BundleMI);
            }
        }

        for(MachineInstr *BundleMI: worklist) {
            // iterate the instructions in the bundle
            fixBundle(BundleMI);
        }
    }

    return false;
}

bool PrimatePacketLegalizer::isWideReg(Register reg) const {
    return TRI->getRegClass(Primate::WIDEREGRegClassID)->contains(reg);
}

bool PrimatePacketLegalizer::hasScalarRegs(MachineInstr* curInst) {
    return hasScalarDefs(curInst) || hasScalarOps(curInst);
}

bool PrimatePacketLegalizer::hasScalarDefs(MachineInstr* curInst) {
    for(auto &res: curInst->defs()) {
        if(!res.isReg()) {
            continue; // wtf?
        } 
        if(!isWideReg(res.getReg()) && res.getReg() != Primate::X0) {
            return true;
        }
    }
    return false;
}
bool PrimatePacketLegalizer::hasScalarOps(MachineInstr* curInst) {
        // check operands to generate extracts
    for(auto &op: curInst->uses()) {
        if(!op.isReg()) {
            continue;
        }
        if(!isWideReg(op.getReg())) {
            //scalar ops must be thinged
            return true;
        }
    }
    return false;
}

void PrimatePacketLegalizer::fixBundle(MachineInstr *BundleMI) {
    const MCSubtargetInfo *STI = BundleMI->getParent()->getParent()->getTarget().getMCSubtargetInfo();
    unsigned const numResourceGroups = STI->getSchedModel().NumProcResourceKinds;
    auto const& lastResourceGroup = STI->getSchedModel().ProcResourceTable[numResourceGroups-1];
    unsigned const numSlots = lastResourceGroup.NumUnits; // ew

    auto pktStart = ++getBundleStart(BundleMI->getIterator());
    auto pktEnd = getBundleEnd(BundleMI->getIterator());
    SmallVector<MachineInstr*> newBundle(numSlots);
    SmallVector<bool>          isNewInstr(numSlots);
    dbgs() << "Slots: " << numSlots << "\n";
    for(auto curInst = pktStart; curInst != pktEnd; curInst++) {
        if(curInst->isCFIInstruction() || curInst->isImplicitDef())
            continue;
        dbgs() << "Looking at instr: ";
        curInst->dump();
        dbgs() << "Has slot: " << curInst->getSlotIdx() << "\n";
        newBundle[curInst->getSlotIdx()] = &*curInst;
        isNewInstr[curInst->getSlotIdx()] = false;
    }
    for(unsigned i = 0; i < numSlots; i++) {
        MachineInstr *curInst = newBundle[i];
        if(newBundle[i] && hasScalarRegs(newBundle[i])) {
            dbgs() << "fixing instruction in slot: " << i << " "; curInst->dump();
            if(newBundle[i]->getOpcode() == Primate::PseudoRET) {
                continue;
            }
            switch (newBundle[i]->getOpcode())
            {
            case Primate::EXTRACT_hang:
            case Primate::EXTRACT:{
                if(isNewInstr[i])
                    continue; // new extracts have been handled. 
                Register wideReg;
                if(TRI->getRegClass(Primate::GPRRegClassID)->contains(curInst->getOperand(0).getReg())) {
                    wideReg = TRI->getMatchingSuperReg(curInst->getOperand(0).getReg(), Primate::gpr_idx, &Primate::WIDEREGRegClass);
                }
                else if(TRI->getRegClass(Primate::GPR128RegClassID)->contains(curInst->getOperand(0).getReg())) {
                    wideReg = TRI->getMatchingSuperReg(curInst->getOperand(0).getReg(), Primate::Pri_hanger, &Primate::WIDEREGRegClass);
                }
                int extIndex = i-1;
                int opIndex = i;
                int insIndex = i+1; // fix up slots
                newBundle[i]->setSlotIdx(extIndex);
                newBundle[extIndex] = newBundle[i];
                newBundle[opIndex] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                            PII->get(Primate::ADDI), 
                                            Primate::X0)
                                            .addReg(Primate::X0)
                                            .addImm(0);
                newBundle[opIndex]->setSlotIdx(opIndex);
                newBundle[insIndex] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                            PII->get(Primate::INSERT), 
                                            wideReg)
                                            .addReg(wideReg)
                                            .addReg(curInst->getOperand(0).getReg())
                                            .addImm(TLI->getScalarField());
                newBundle[insIndex]->setSlotIdx(insIndex);
                MIBundleBuilder builder(BundleMI);
                isNewInstr[opIndex] = true;
                isNewInstr[insIndex] = true;
                isNewInstr[extIndex] = true;
                // insert after instr
                builder.insert(++(curInst->getIterator()), newBundle[opIndex]);
                builder.insert(++(++(curInst->getIterator())), newBundle[insIndex]);
                BundleMI->getParent()->dump();
                break;
            }
            case Primate::INSERT_hang:
            case Primate::INSERT:{
                // check if the op exists
                int opCheck = i - 1;
                // if no op there then just add it in. then the op clean up will clean it. 
                if(!newBundle[opCheck]) {
                    dbgs() << "found bad insert\n";
                    curInst->dump();
                    newBundle[opCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                            PII->get(Primate::ADDI), 
                                            curInst->getOperand(2).getReg())
                                            .addReg(curInst->getOperand(2).getReg())
                                            .addImm(0);
                    newBundle[opCheck]->dump();
                    newBundle[opCheck]->setSlotIdx(opCheck);
                    MIBundleBuilder builder(BundleMI);
                    isNewInstr[opCheck] = true;
                    builder.insert((curInst->getIterator()), newBundle[opCheck]);
                    BundleMI->getParent()->dump();
                }
                break;
            }
            case Primate::LW: {
                break;
            }
            case Primate::SW: {
                break;
            }
            default: {
                if(curInst->isBranch()) {
                    dbgs() << "ran into branch. already handled...\n";
                    continue;
                }
                // not insert, extract, or memory;
                dbgs() << "op needs ins or ext";
                curInst->dump();
                int insCheck = i + 1;
                int opNum = 0;
                int extOffset = 1;
                if(hasScalarOps(curInst)) {
                    dbgs() << "op needs extract!\n";
                    for(auto &op: curInst->uses()) {
                        if(!op.isReg() || op.getReg() == Primate::X0) {
                            continue;
                        }
                        if(!isWideReg(op.getReg())) {
                            //scalar ops must be thinged
                            int extCheck = i - extOffset;
                            Register wideReg;
                            if(TRI->getRegClass(Primate::GPRRegClassID)->contains(op.getReg())) {
                                wideReg = TRI->getMatchingSuperReg(op.getReg(), Primate::gpr_idx, &Primate::WIDEREGRegClass);
                            }
                            else if(TRI->getRegClass(Primate::GPR128RegClassID)->contains(op.getReg())) {
                                wideReg = TRI->getMatchingSuperReg(op.getReg(), Primate::Pri_hanger, &Primate::WIDEREGRegClass);
                            }
                            newBundle[extCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                                        PII->get(Primate::EXTRACT), 
                                                        op.getReg()).addReg(wideReg).addImm(TLI->getScalarField());
                            newBundle[extCheck]->dump();   
                            newBundle[extCheck]->setSlotIdx(extCheck);   
                            MIBundleBuilder builder(BundleMI);
                            isNewInstr[extCheck] = true;
                            auto insertPoint = curInst->getIterator();
                            for(int i = 0; i < extOffset-1; i++)
                                insertPoint--;
                            builder.insert(insertPoint, newBundle[extCheck]);
                            BundleMI->getParent()->dump();
                            extOffset++;
                        }
                        opNum++;
                    }
                }
                if(hasScalarDefs(curInst) && !newBundle[insCheck]) {
                    dbgs() << "op needs insert\n";
                    Register wideReg;
                    if(TRI->getRegClass(Primate::GPRRegClassID)->contains(curInst->getOperand(0).getReg())) {
                        wideReg = TRI->getMatchingSuperReg(curInst->getOperand(0).getReg(), Primate::gpr_idx, &Primate::WIDEREGRegClass);
                    }
                    else if(TRI->getRegClass(Primate::GPR128RegClassID)->contains(curInst->getOperand(0).getReg())) {
                        wideReg = TRI->getMatchingSuperReg(curInst->getOperand(0).getReg(), Primate::Pri_hanger, &Primate::WIDEREGRegClass);
                    }
                    newBundle[insCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                                PII->get(Primate::INSERT), 
                                                wideReg).addReg(wideReg).addReg(curInst->getOperand(0).getReg()).addImm(TLI->getScalarField());
                    newBundle[insCheck]->dump();   
                    newBundle[insCheck]->setSlotIdx(insCheck);   
                    MIBundleBuilder builder(BundleMI);
                    isNewInstr[insCheck] = true;
                    builder.insert(++(curInst->getIterator()), newBundle[insCheck]);
                    BundleMI->getParent()->dump();
                }                  
                break;
            }
            }
        }
    }

}

MachineFunctionPass *createPrimatePacketLegalizerPass();
void initializePrimatePacketLegalizerPass(PassRegistry&);
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(PrimatePacketLegalizer, "PrimatePacketLegalizer",
                      "Primate Packet Legalizer", false, false)
INITIALIZE_PASS_DEPENDENCY(PrimatePacketizer)
INITIALIZE_PASS_END(PrimatePacketLegalizer, "PrimatePacketLegalizer",
                    "Primate Packet Legalizer", false, false)

llvm::MachineFunctionPass *llvm::createPrimatePacketLegalizerPass() {
  return new llvm::PrimatePacketLegalizer();
}
