#include "Primate.h"
#include "PrimateVLIWPacketizer.h"
#include "PrimatePacketPostProc.h"
#include "PrimateInstrInfo.h"
#include "PrimateRegisterInfo.h"
#include "PrimateSubtarget.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/Pass.h"

using namespace llvm;


namespace llvm {

bool PrimatePacketPostProc::addOpForInsert(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder) {
    assert(MI->getOperand(1).isReg() && "insert reads non reg.");
    bool ret = false;
    dbgs() << "checking insert to add ops ";
    MI->dump();

    MachineOperand& defReg = *(MI->defs().begin());
    assert(defReg.getReg() == MI->getOperand(1).getReg() && "insert reads and writes a different register.");

    // check that the value inserted is actually present
    for(unsigned i = 2; i < MI->getNumOperands(); i++) {
        assert(MI->getSlotIdx() >= 0 && "insert without a SLOT!");
        bool found = false;
        MachineOperand& reg = MI->getOperand(i);
        if (!reg.isReg()) {
            continue;
        }

        auto foundExtItr = std::find_if(ops.begin(), ops.end(), [&](MachineInstr *a) -> bool{
            auto temp = a->defs();
            return std::find_if(temp.begin(), temp.end(), [&](MachineOperand& b) { 
                return b.getReg() == reg.getReg();
            }) != temp.end();
        });

        found = foundExtItr != ops.end();
        assert((!found || (found && ((*foundExtItr)->getSlotIdx() == (MI->getSlotIdx() + 1)))) && "found the op but in the wrong slot...");       

        if (!found) {
            dbgs() << "No op found! add the op we want.\n";
            MachineInstr* bypass_op = BuildMI(*MF, llvm::DebugLoc(), 
                                                PII->get(Primate::ADDI), 
                                                reg.getReg())
                .addReg(reg.getReg())
                .addImm(0);

            bypass_op->setSlotIdx(MI->getSlotIdx() + 1);
            builder.insert(MI->getIterator(), bypass_op);
            ops.push_back(bypass_op);
            dbgs() << "created op: "; bypass_op->dump();
        }
    }
    dbgs() << "----------\n";
    return ret;
}

bool PrimatePacketPostProc::fixDanglingExt(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder) {
    // if its dangled at this point its live out of the packet
    bool ret = false;
    dbgs() << "checking ext to fix dangling ";
    MI->dump();

    auto reg = (MI->defs().begin())->getReg();
    


    return ret;
}

bool PrimatePacketPostProc::fixMaterializedReg(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder) {
    bool ret = false;
    dbgs() << "checking op to materialize result ";
    MI->dump();

    auto reg = (MI->defs().begin());
    reg->dump();
    // killed. idc
    if(!reg->isReg() || reg->isKill() || reg->getReg() == Primate::X0){
        return ret;
    }

    // not killed. check the consumer for killed.
    auto foundExtItr = std::find_if(ins.begin(), ins.end(), [&](MachineInstr *a) -> bool{
        auto temp = a->uses();
        return std::find_if(temp.begin(), temp.end(), [&](MachineOperand& b) { 
            if(!b.isReg()) 
                return false;
            return b.getReg() == reg->getReg();
        }) != temp.end();
    });

    // no one consumes. Need to put it in a reg
    bool materialize = false;
    if (foundExtItr == ins.end()) {
        dbgs() << "op is not consumed in packet. materialize it using insert.\n";
        materialize = true;
    }
    else {
        auto consInstrUses = (*foundExtItr)->uses();
        auto consReg = std::find_if(consInstrUses.begin(), consInstrUses.end(),
            [&](MachineOperand& a) -> bool {
                if(a.isReg()) {
                    return a.getReg() == reg->getReg();
                }
                return false;
        });
        assert(consReg != consInstrUses.end() && "instr suddenly doesn't use a reg...");
        if(!consReg->isKill()) {
            dbgs() << "found a consumer but its not killed";
            if((*foundExtItr)->getOpcode() == Primate::INSERT) {
                dbgs() << "!!!!!!PLEASE GO FIX THE INSERT LIVE OUT!!!!!!";
            }
            materialize = true;
        }
    }
    if(materialize) {
        MachineInstr* bypass_op = BuildMI(*MF, llvm::DebugLoc(), 
                                            PII->get(Primate::INSERT), 
                                            reg->getReg())
                .addReg(reg->getReg())
                .addImm(0); // TODO: SCALAR

        bypass_op->setSlotIdx(MI->getSlotIdx() - 1);
        builder.insert(MI->getIterator(), bypass_op);
        ins.push_back(bypass_op);
        dbgs() << "created op: "; bypass_op->dump();
        ret = true;
    }

    return ret;
}

// function, instruction to add for, operand for MOP, builder
bool PrimatePacketPostProc::addExtractForOp(MachineFunction* MF, MachineInstr* MI, MIBundleBuilder& builder) {
    bool ret = false;
    dbgs() << "checking op to add extracts ";
    MI->dump();
    for(MachineOperand& reg: MI->uses()) {
        bool found = false;
        if(!reg.isReg())
            continue;
        if(reg.getReg() == Primate::X0)
            found = true;
        
        auto foundExtItr = std::find_if(exts.begin(), exts.end(), [=](MachineInstr *a) -> bool{
            auto temp = a->defs();
            return std::find_if(temp.begin(), temp.end(), [=](MachineOperand& b) { 
                return b.getReg() == reg.getReg();
            }) != temp.end();
        });

        found = (foundExtItr != exts.end());

        if(!found) {
            reg.dump();
            dbgs() << " NOT FOUND!!!! adding op:\n";
            MachineInstr* bypass_op = BuildMI(*MF, llvm::DebugLoc(), 
                                                PII->get(Primate::EXTRACT), 
                                                reg.getReg())
                .addReg(reg.getReg())
                .addImm(0);
            unsigned attemptedSlotIdx = MI->getSlotIdx() + 1;
            bool slotTaken = std::find_if(exts.begin(), exts.end(), [=](MachineInstr* a) -> bool {
                return a->getSlotIdx() == attemptedSlotIdx;
            }) != exts.end();
            if (slotTaken)
                attemptedSlotIdx++;
            slotTaken = std::find_if(exts.begin(), exts.end(), [=](MachineInstr* a) -> bool {
                return a->getSlotIdx() == attemptedSlotIdx;
            }) != exts.end();
            if(slotTaken)
                llvm_unreachable("no slot for required extract...");
            bypass_op->setSlotIdx(attemptedSlotIdx);//op->getSlotIdx()+1);
            builder.insert(MI->getIterator(), bypass_op);
            //builder.append(bypass_op);
            bypass_op->dump();
            ret = true;
        } 
        else if (foundExtItr != exts.end()) {
            dbgs() << "Found the extract for op\n";
            auto foundExt = *foundExtItr;
            // set the idx
            if (foundExt->getSlotIdx() == (unsigned)-1) {
                dbgs() << "extract has no slot\n";
                unsigned attemptedSlotIdx = MI->getSlotIdx() + 1;
                bool slotTaken = std::find_if(exts.begin(), exts.end(), [=](MachineInstr* a) -> bool {
                    return a->getSlotIdx() == attemptedSlotIdx;
                }) != exts.end();
                if (slotTaken)
                    attemptedSlotIdx++;
                slotTaken = std::find_if(exts.begin(), exts.end(), [=](MachineInstr* a) -> bool {
                    return a->getSlotIdx() == attemptedSlotIdx;
                }) != exts.end();
                if(slotTaken)
                    llvm_unreachable("no slot for required extract...");
                foundExt->setSlotIdx(attemptedSlotIdx);//op->getSlotIdx()+1);
                ret = true;
            }
        }
    }
    dbgs() << "-------\n";
    return ret;
}

bool PrimatePacketPostProc::fixBranchOperandIndexes(MachineInstr* branchInstr, MachineInstr* bundle) {
    bool ret = false;
    dbgs() << "fixing branch indexes ";
    branchInstr->dump();
    auto it = branchInstr->uses().begin();
    for(int opCount = 0; it != branchInstr->uses().end(); it++, opCount++) {
        MachineOperand& reg = *it;
        bool found = false;
        if(!reg.isReg())
            continue;
        if(reg.getReg() == Primate::X0)
            found = true;
        
        auto pktStart = getBundleStart(bundle->getIterator());
        auto pktEnd = getBundleEnd(bundle->getIterator());

        auto foundExtItr = std::find_if(ops.begin(), ops.end(), [=](MachineInstr* a) -> bool{
            auto temp = a->defs();
            return std::find_if(temp.begin(), temp.end(), [=](MachineOperand& b) { 
                return b.getReg() == reg.getReg();
            }) != temp.end();
        });
        
        if(foundExtItr == ops.end()) {

            llvm_unreachable("BRANCH NOT WITH CONSUMER OPS!!!!");
        }
        else {
            unsigned slotIdx = (*foundExtItr)->getSlotIdx();
            auto newReg = Primate::X0 + slotIdx;
            it->setReg(newReg);
            ret = true;
        }
    }
    dbgs() << "-------\n";
    return ret;
}

bool PrimatePacketPostProc::addNOPs(MachineFunction* MF, MachineInstr& bundle, MIBundleBuilder& builder){
    bool ret = false;
    for(int i = 0; i < 10; i++) {
        auto pktStart = getBundleStart(bundle.getIterator());
        auto pktEnd   = getBundleEnd(bundle.getIterator());
        SmallVector<std::pair<int, MachineInstr*>> usedSlots;
        for(auto curInst = pktStart; curInst != pktEnd; curInst++) {
            usedSlots.push_back({curInst->getSlotIdx(), &(*curInst)});
        }

        
        auto found = std::find_if(usedSlots.begin(), usedSlots.end(), [&](std::pair<int, MachineInstr*>& a) -> bool {
            return a.first == i;
        });
        if(found == usedSlots.end()) {
            MachineInstr* bypass_op = BuildMI(*MF, llvm::DebugLoc(), 
                                                PII->get(Primate::ADDI), 
                                                Primate::X0)
                .addReg(Primate::X0)
                .addImm(0);
            bypass_op->setSlotIdx(i);
            builder.insert(found->second->getIterator(), bypass_op);
            ret = true;
        }
    }
    return ret;
}


bool PrimatePacketPostProc::runOnMachineFunction(MachineFunction& MF) {
    PII = MF.getSubtarget<PrimateSubtarget>().getInstrInfo();
    bool ret = true;
    for(MachineBasicBlock& MBB: MF) {
        if (MBB.empty()) {
            dbgs() << "Ran into an empty Machine Basic Block\n";
            continue;
        }
        for(MachineInstr& machineBundle: MBB) {
            auto pktStart = getBundleStart(machineBundle.getIterator());
            auto pktEnd = getBundleEnd(machineBundle.getIterator());
            auto it = pktStart;
            int count = 0;
            while(it != pktEnd) {it++; count++;}
            if(count == 0) {
                dbgs() << "empty packet\n";
            }
            exts.clear();
            ins.clear();
            ops.clear();
            for(auto curInst = pktStart; curInst != pktEnd; curInst++) {
                if(curInst->getOpcode() == Primate::EXTRACT) {
                    dbgs() << "adding extract to work list: ";
                    curInst->dump();
                    exts.push_back(&(*curInst));
                }
                else if(curInst->getOpcode() == Primate::PseudoInsert) {
                    dbgs() << "adding insert to work list: ";
                    curInst->dump();
                    ins.push_back(&(*curInst));
                }
                else if(curInst->getOpcode() != Primate::BUNDLE && curInst->getOpcode() != Primate::IMPLICIT_DEF) {
                    dbgs() << "adding operation to work list: ";
                    curInst->dump();
                    ops.push_back(&(*curInst));
                }
            }
            dbgs() << "----------\n";

            MIBundleBuilder builder(&machineBundle);
            for(MachineInstr* insertInstr: ins) {
                ret = addOpForInsert(&MF, insertInstr, builder) || ret;
            }

            // verfy ops
            for(MachineInstr* op: ops) {
                // no ext or ins for these
                if(op->getOpcode() == Primate::OUTPUTHEADER ||
                   op->getOpcode() == Primate::OUTPUTMETA ||
                   op->getOpcode() == Primate::INPUT_EXTRACT ||
                   op->getOpcode() == Primate::MATCH) {
                    continue;
                }
                // no branch. look for exts and ins
                if(!op->isBranch()) {
                    ret = addExtractForOp(&MF, op, builder) || ret;
                }
            }

            // at this point all extracts should have been sloted. IF its not slotted, we need to materialize this
            for(MachineInstr* op: exts) {
                if (op->getSlotIdx() == (unsigned)-1) {
                    ret = fixDanglingExt(&MF, op, builder) || ret;
                    llvm_unreachable("unsloted extract....");
                }
            }

            // everything is happy at this point. go check ops that create regs that are not killed later
            for(MachineInstr* op: ops) {
                if(op->getOpcode() == Primate::OUTPUTHEADER ||
                   op->getOpcode() == Primate::OUTPUTMETA ||
                   op->getOpcode() == Primate::INPUT_EXTRACT ||
                   op->getOpcode() == Primate::MATCH) {
                    continue;
                }
                ret = fixMaterializedReg(&MF, op, builder) || ret;
            }

            // register to slot id
            for(MachineInstr* op: ops) {
                if(!op->isBranch()) {
                    continue;
                }
                ret = fixBranchOperandIndexes(op, &machineBundle) || ret;
            }

            // ret = addNOPs(&MF, machineBundle, builder) || ret;
            
        }

    }
    dbgs() << "BUNDLE PACKET PEEPHOLE\n";
    MF.dump();
    dbgs() << "-------\n";
    return ret;
}

MachineFunctionPass *createPrimatePacketPostProc();
void initializePrimatePacketPostProcPass(PassRegistry&);
}

INITIALIZE_PASS_BEGIN(PrimatePacketPostProc, "primate-packetizer",
                      "Primate Packetizer", false, false)
INITIALIZE_PASS_DEPENDENCY(PrimatePacketizer)
INITIALIZE_PASS_END(PrimatePacketPostProc, "primate-packetizer",
                    "Primate Packetizer", false, false)

MachineFunctionPass *llvm::createPrimatePacketPostProc() {
  return new llvm::PrimatePacketPostProc();
}