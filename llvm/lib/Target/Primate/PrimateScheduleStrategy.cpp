#include "PrimateScheduleStrategy.h"

using namespace llvm;

namespace llvm {

void PrimateSchedStrategy::initialize(ScheduleDAGMI *dag) {
    const TargetInstrInfo *TII = dag->TII; 
    ResourceTracker = TII->CreateTargetScheduleState(dag->MF.getSubtarget());
    PII = dag->MF.getSubtarget<PrimateSubtarget>().getInstrInfo();
    TRI = dag->MF.getSubtarget().getRegisterInfo();
}

SUnit *PrimateSchedStrategy::pickNode(bool &IsTopNode) {
    dbgs() << "picking node";
    SUnit* selectedNode = nullptr; 
    IsTopNode = true;
    // done with the region
    if (candidates.size() == 0) {
        dbgs() << "done with region\n";
        return nullptr;
    }

    for (SUnit* SU: candidates) {
      if(ResourceTracker->canReserveResources(*(SU->getInstr()))) {
	if(selectedNode && selectedNode->getDepth() > SU->getDepth()) {
	  selectedNode = SU;
	}
	else if (!selectedNode) {
	  selectedNode = SU;
	}
      }
    }
    if(selectedNode) {
        dbgs() << " that can be bundled, by the resource tracker\n";
        if(!selectedNode->isInstr())
            selectedNode->getNode()->dump();
        candidates.erase(selectedNode);
        ResourceTracker->reserveResources(*(selectedNode->getInstr()));
        return selectedNode;
    }
    else {
        dbgs() << " that will start a new bundle\n";
        ResourceTracker->clearResources();
        selectedNode = *candidates.begin();
        if(!selectedNode->isInstr())
            selectedNode->getNode()->dump();
        else 
            selectedNode->getInstr()->dump();
        candidates.erase(selectedNode);
        ResourceTracker->reserveResources(*(selectedNode->getInstr()));
        return selectedNode;
    }
}

void PrimateSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {

}

void PrimateSchedStrategy::releaseTopNode(SUnit *SU) {
    dbgs() << "released top node: "; SU->getInstr()->dump();
    candidates.insert(SU);
}

void PrimateSchedStrategy::releaseBottomNode(SUnit *SU) {
    dbgs() << "released bottom node: "; SU->getInstr()->dump();
}

}

