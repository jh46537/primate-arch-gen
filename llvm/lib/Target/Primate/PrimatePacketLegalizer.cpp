

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
                if(hasScalarRegs(&*curInst) || PrimateII::isBFUInstr(curInst->getDesc().TSFlags)) {
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

bool hasRegDefs(MachineInstr* curInst) {
    bool ret = false;
    for(auto &res: curInst->defs()) {
        if(!res.isReg()) {
            continue; // wtf?
        } 
        else {
            ret = true;
        }
    }
    return ret;
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

void PrimatePacketLegalizer::fixBFUInstr(SmallVector<MachineInstr*>& newBundle, SmallVector<bool>& isNewInstr, MachineInstr* BundleMI, int slotIdx) {
    dbgs() << "attempt BFU fix up for slotID: " << slotIdx << "\n";
    newBundle[slotIdx]->dump();
    if (TLI->isSlotMergedFU(slotIdx)) {
        // merged slots need inserts and extracts for operands q.q
        MachineInstr* curInst = newBundle[slotIdx];
        dbgs() << "BFU inst needs ins or ext " << slotIdx;
        curInst->dump();
        int insCheck = slotIdx + 1;
        int opNum = 0;
        int extOffset = 2;
        dbgs() << "op needs extract!\n";
        for (auto &op: curInst->uses()) {
            if (!op.isReg() || op.getReg() == Primate::X0) {
                extOffset--;
                continue;
            }
            if (op.isReg()) {
                //scalar ops must be thinged
                int extCheck = slotIdx - extOffset;
                Register wideReg;
                if(TRI->getRegClass(Primate::GPRRegClassID)->contains(op.getReg())) {
                    wideReg = TRI->getMatchingSuperReg(op.getReg(), Primate::gpr_idx, &Primate::WIDEREGRegClass);
                }
                else if(TRI->getRegClass(Primate::GPR128RegClassID)->contains(op.getReg())) {
                    wideReg = TRI->getMatchingSuperReg(op.getReg(), Primate::Pri_hanger, &Primate::WIDEREGRegClass);
                }
                else {
                    wideReg = curInst->getOperand(0).getReg();
                }
                newBundle[extCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                            PII->get(Primate::EXTRACT), 
                                            op.getReg()).addReg(wideReg).addImm(TLI->getWholeRegField());
                newBundle[extCheck]->dump();   
                newBundle[extCheck]->setSlotIdx(extCheck);   
                MIBundleBuilder builder(BundleMI);
                isNewInstr[extCheck] = true;
                auto insertPoint = curInst->getIterator();
                for(int i = 0; i < extOffset-1; i++)
                    if (newBundle[slotIdx-i-1])
                        insertPoint--;
                builder.insert(insertPoint, newBundle[extCheck]);
                BundleMI->getParent()->dump();
                extOffset--;
            } else {
                extOffset--;
            }
            opNum++;
        }
        if (!newBundle[insCheck] && hasRegDefs(curInst)) {
            dbgs() << "op needs insert\n";
            Register wideReg;
            if(TRI->getRegClass(Primate::GPRRegClassID)->contains(curInst->getOperand(0).getReg())) {
                wideReg = TRI->getMatchingSuperReg(curInst->getOperand(0).getReg(), Primate::gpr_idx, &Primate::WIDEREGRegClass);
            }
            else if(TRI->getRegClass(Primate::GPR128RegClassID)->contains(curInst->getOperand(0).getReg())) {
                wideReg = TRI->getMatchingSuperReg(curInst->getOperand(0).getReg(), Primate::Pri_hanger, &Primate::WIDEREGRegClass);
            }
            else {
                wideReg = curInst->getOperand(0).getReg();
            }
            newBundle[insCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                        PII->get(Primate::INSERT), 
                                        wideReg).addReg(wideReg).addReg(curInst->getOperand(0).getReg()).addImm(TLI->getWholeRegField());
            newBundle[insCheck]->dump();   
            newBundle[insCheck]->setSlotIdx(insCheck);   
            MIBundleBuilder builder(BundleMI);
            isNewInstr[insCheck] = true;
            builder.insert(++(curInst->getIterator()), newBundle[insCheck]);
            BundleMI->getParent()->dump();
        }                  
    }
}

// All this does is materialize hanging inserts and extracts
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
        dbgs() << "Adding instr to tracking: ";
        curInst->dump();
        dbgs() << "with slot: " << curInst->getSlotIdx() << "\n";
        newBundle[curInst->getSlotIdx()] = &*curInst;
        isNewInstr[curInst->getSlotIdx()] = false;
    }
    for(unsigned i = 0; i < numSlots; i++) {
        MachineInstr *curInst = newBundle[i];
        if(newBundle[i] && (hasScalarRegs(newBundle[i]) || PrimateII::isBFUInstr(newBundle[i]->getDesc().TSFlags))) {
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
                int extIndex = i-2;
                int opIndex = i;
                int insIndex = i+1; // fix up slots
                newBundle[i]->setSlotIdx(extIndex);
                newBundle[extIndex] = newBundle[i];
                newBundle[opIndex] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                            PII->get(Primate::ADDI), 
                                            Primate::X0 + insIndex)
                                            .addReg(Primate::X0 + extIndex)
                                            .addImm(0);
                newBundle[opIndex]->setSlotIdx(opIndex);
                newBundle[insIndex] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                            PII->get(Primate::INSERT), 
                                            wideReg)
                                            .addReg(wideReg)
                                            .addReg(Primate::X0 + opIndex)
                                            .addImm(TLI->getScalarField());
                newBundle[insIndex]->setSlotIdx(insIndex);
                MIBundleBuilder builder(BundleMI);
                isNewInstr[opIndex] = true;
                isNewInstr[insIndex] = true;
                isNewInstr[extIndex] = true;
                // insert after instr
                builder.insert(++(curInst->getIterator()), newBundle[opIndex]);
                builder.insert(++(++(curInst->getIterator())), newBundle[insIndex]);
                break;
            }
            case Primate::INSERT_hang:
            case Primate::INSERT:{
                if(isNewInstr[i])
                    continue; // new inserts have been handled.
                
                // This instruction is baseline and requires us to add an extract and an op
                // also requires a slotIdx fix
                // check if the op exists
                int insCheck = i + 1;
                int opCheck = i;
                int extCheck = i - 2;
                assert(!newBundle[insCheck] && "hanging insert but there is already an insert there");
                assert(!newBundle[extCheck] && "hanging insert but there is already an extract there");

                newBundle[insCheck] = newBundle[i];
                newBundle[insCheck]->setSlotIdx(insCheck);
                newBundle[i] = nullptr;
                // if no op there then just add it in. Also means that there will be no extract so add that too. 
                dbgs() << "found bad insert\n";
                
                Register wideReg;
                if(TRI->getRegClass(Primate::GPRRegClassID)->contains(curInst->getOperand(2).getReg())) {
                    wideReg = TRI->getMatchingSuperReg(curInst->getOperand(2).getReg(), Primate::gpr_idx, &Primate::WIDEREGRegClass);
                }
                else if(TRI->getRegClass(Primate::GPR128RegClassID)->contains(curInst->getOperand(2).getReg())) {
                    wideReg = TRI->getMatchingSuperReg(curInst->getOperand(2).getReg(), Primate::Pri_hanger, &Primate::WIDEREGRegClass);
                }
                else {
                    llvm_unreachable("insert with reg not wide or scalar. ping kayvan with IR and config.");
                }

                curInst->dump();
                newBundle[opCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                        PII->get(Primate::ADDI), 
                                        Primate::X0+i)
                                        .addReg(Primate::X0 + extCheck)
                                        .addImm(0);
                newBundle[opCheck]->dump();
                newBundle[opCheck]->setSlotIdx(opCheck);
                newBundle[extCheck] = BuildMI(*(BundleMI->getParent()->getParent()), llvm::DebugLoc(), 
                                        PII->get(Primate::EXTRACT), 
                                        Primate::X0 + extCheck)
                                        .addReg(wideReg)
                                        .addImm(TLI->getScalarField());
                newBundle[extCheck]->dump();
                newBundle[extCheck]->setSlotIdx(extCheck);
                MIBundleBuilder builder(BundleMI);
                isNewInstr[insCheck] = true;
                isNewInstr[opCheck] = true;
                isNewInstr[extCheck] = true;
                builder.insert((curInst->getIterator()), newBundle[extCheck]);
                builder.insert((curInst->getIterator()), newBundle[opCheck]);
                break;
            }
            default: {
                if(curInst->isBranch()) {
                    dbgs() << "ran into branch. already handled...\n";
                    continue;
                }
                if(PrimateII::isBFUInstr(curInst->getDesc().TSFlags)) {
                    LLVM_DEBUG(dbgs() << "BFU instr might need fix\n");
                    fixBFUInstr(newBundle, isNewInstr, BundleMI, i);
                    continue;
                }
                // not insert, extract, or memory;
                dbgs() << "op needs ins or ext";
                curInst->dump();
                int insCheck = i + 1;
                int opNum = 0;
                int extOffset = 2;
                if(hasScalarOps(curInst) || TLI->isSlotGFU(i) || TLI->isSlotMergedFU(i)) {
                    dbgs() << "op needs extract!\n";
                    for(auto &op: curInst->uses()) {
                        if(!op.isReg() || op.getReg() == Primate::X0) {
                            extOffset--;
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
                            for(int j = 0; j < extOffset-1; j++)
                                if (newBundle[i-j-1])
                                    insertPoint--;
                            builder.insert(insertPoint, newBundle[extCheck]);
                            extOffset--;
                        }
                        else {
                            extOffset--;
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
                }                  
                break;
            }
            }
        } else if(newBundle[i]) {
            dbgs() << "no fix needed for slot: " << i << " "; newBundle[i]->dump();
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
