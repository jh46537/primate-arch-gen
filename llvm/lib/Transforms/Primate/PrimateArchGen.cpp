//	PrimateArchGen.cpp
//	To generate architectural parameters for Primate template
//
//	Based on code from Todd C. Mowry
//	Modified by Arthur Peters
//	Modified by Ankit Goyal
//  Modified by Rui Ma
/////////////////////////////////////////////////////////////////////////////////////

/******************************************************************************

Liveness: OUT[n] = UNION_{s E succ[n]} IN[s]  //meet
IN[n] = GEN[n] U (OUT[n] - KILL[n]) //transfer function

Flow Direction: Backward
A BitVector stored at each node for IN and OUT. Bit vector contains an entry for all the values

Boundary Conditions: empty set for flow value. identified by no successors.

 *********************************************************************************/

#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Transforms/Primate/PrimateArchGen.h>
#include <cstddef>
#include <system_error>
#include "llvm/Demangle/Demangle.h"

using namespace llvm;

// set the boundary condition for block
// explicit constructor of BitVector
void PrimateArchGen::setBoundaryCondition(BitVector *BlkBoundry) {
    *BlkBoundry = BitVector(domainSize, false); 
}

//union (bitwise OR) operator '|=' overriden in BitVector class
void PrimateArchGen::meetOp(BitVector* lhs, const BitVector* rhs) {
    *lhs |= *rhs; 
}

// empty set initially; each bit represent a value
BitVector* 
PrimateArchGen::initializeFlowValue(BasicBlock& BB, SetType setType) { 
    return new BitVector(domainSize, false); 
}

//transfer function:
//IN[n] = USE[n] U (OUT[n] - DEF[n])
BitVector* PrimateArchGen::transferFn(BasicBlock& BB) {
    BitVector* outNowIn = new BitVector(*((*out)[&BB]));
    BitVector* immIn = outNowIn; // for empty blocks
    Instruction* tempInst;
    
    // go through instructions in reverse
    BasicBlock::iterator ii = --(BB.end()), ib = BB.begin();
    while (true) {
        // inherit data from next instruction
        tempInst = &*ii;
        immIn = (*instrInSet)[tempInst];            
        *immIn = *outNowIn;

        // if this instruction is a new definition, remove it
        if (isDefinition(tempInst)) {
            (*immIn)[(*valueToBitVectorIndex)[tempInst]] = false;
        }
        
        if (isa<CallInst>(*tempInst)) {
            auto *inst = dyn_cast<llvm::CallInst>(&*tempInst);
            Function* foo = inst->getCalledFunction();
            if (foo->getName().contains("lifetime.start")) {
                Value* ptrOp = inst->getOperand(1);
                (*immIn)[(*valueToBitVectorIndex)[(*aliasMap)[ptrOp]]] = false;
                // tempInst->print(errs());
                // errs() << " kill " << (*aliasMap)[ptrOp]->getName() << "\n";
            }
        }

        // add the arguments, unless it is a phi node
        if (!isa<PHINode>(*ii)) {
            // skip if can be combined into a single branch instruction
            if (combinedBranchInst.find(&*ii) == combinedBranchInst.end()) {
                // skip getElementPtrInst, bitCast, lifetime call instructions
                if (!(isa<GetElementPtrInst>(*ii) || isa<BitCastInst>(*ii) || 
                                                     isLifetimeCall(&*ii))) {
                    User::op_iterator OI, OE;
                    for (OI = tempInst->op_begin(), 
                         OE = tempInst->op_end(); OI != OE; ++OI) {
                        if (isa<Instruction>(*OI) || isa<Argument>(*OI)) {
                            Value *op = (*aliasMap)[*OI];
                            (*immIn)[(*valueToBitVectorIndex)[op]] = true;
                        }
                    }
                }
            }
        } else if(isa<PHINode>(*ii)) {
            PHINode* phiNode = cast<PHINode>(&*ii);
            for (unsigned int incomingIdx = 0; 
                 incomingIdx < phiNode->getNumIncomingValues(); incomingIdx++) {
                Value* val = phiNode->getIncomingValue(incomingIdx);
                if (isa<Instruction>(val) || isa<Argument>(val)) {
                    int valIdx = (*valueToBitVectorIndex)[(*aliasMap)[val]];
                    BasicBlock* 
                        incomingBlock = phiNode->getIncomingBlock(incomingIdx);
                    if ((*neighbourSpecificValues).find(incomingBlock) == 
                        (*neighbourSpecificValues).end())
                        (*neighbourSpecificValues)[incomingBlock] = 
                            new BitVector(domainSize);
                    (*(*neighbourSpecificValues)[incomingBlock]).set(valIdx);
                }
            }
        }
        outNowIn = immIn;
        if (ii == ib) break;
        --ii;
    }
    return immIn;
}

bool PrimateArchGen::isLifetimeCall(Instruction *i) {
    bool res = false;
    if (isa<CallInst>(*i)) {
        auto *inst = dyn_cast<llvm::CallInst>(&*i);
        Function* foo = inst->getCalledFunction();
        if (foo->getName().contains("lifetime")) {
            res = true;
        }
    }
    return res;
}

bool PrimateArchGen::isDefinition(Instruction *i) {
    return !(i->isTerminator()) ;
}

void PrimateArchGen::calculate(const Instruction *ii) {
    //static unsigned int live[50], n=0;
    unsigned int count = 0;

    if (!isa<PHINode>(*(ii))) {
    const BitVector *bv = (*instrInSet)[&*ii];

    for(size_t i = 0; i < bv->size(); i++){
        if((*bv)[i])
            count++;
    }

    if(count > n) {
        n = count+1;
    }

    live[count]++;
    }
}

bool PrimateArchGen::isBlueCall(Instruction *ii) {
    bool res = false;
    if (isa<llvm::CallInst>(&*ii)) {
        MDNode *PrimateMetadata = ii->getMetadata("primate");
        if (PrimateMetadata != NULL) {
            // this is probably not safe to assume the type, 
            // you probably want to type check...
            if (cast<MDString>(
                PrimateMetadata->getOperand(0))->getString() == "blue") { 
                res = true;
            }
        }
    }
    return res;
}

unsigned PrimateArchGen::getArrayWidth(ArrayType &a, unsigned start) {
    unsigned num_elem = a.getNumElements();
    auto elem = a.getElementType();
    unsigned elemWidth;

    if (isa<llvm::IntegerType>(*elem)) {
        elemWidth = elem->getIntegerBitWidth();
    } else if (isa<llvm::ArrayType>(*elem)) {
        auto *selem = dyn_cast<llvm::ArrayType>(elem);
        elemWidth = getArrayWidth(*selem, 0);
    } else if (isa<llvm::StructType>(*elem)) {
        auto *selem = dyn_cast<llvm::StructType>(elem);
        elemWidth = getStructWidth(*selem, 0, false);
    }
    return (start + num_elem * elemWidth);
}

unsigned PrimateArchGen::getArrayWidthArcGen(ArrayType &a, unsigned start) {
    unsigned width = start;
    auto elem = a.getElementType();
    unsigned elemWidth = 0;

    if (isa<llvm::IntegerType>(*elem)) {
        elemWidth = elem->getIntegerBitWidth();
        // insert new gather mode if doesn't exist
        gatherModes->insert(elemWidth);
    }

    for (int i = 0; i < a.getNumElements(); i++) {
        if (fieldIndex->find(width) == fieldIndex->end()) {
            (*fieldIndex)[width] = new std::set<unsigned>();
        }

        if (isa<llvm::IntegerType>(*elem)) {
            // the width of all numbers possibly stored at each index
            if (std::find((*fieldIndex)[width]->begin(), 
                          (*fieldIndex)[width]->end(), 
                          elemWidth) == (*fieldIndex)[width]->end()) {
                (*fieldIndex)[width]->insert(elemWidth);
            }
            width += elemWidth;
        } else if (isa<llvm::ArrayType>(*elem)) {
            auto *selem = dyn_cast<llvm::ArrayType>(elem);
            unsigned elemWidth = getArrayWidthArcGen((*selem), width);
            // (*fieldIndex)[width]->insert(elemWidth);
            width = elemWidth;
        } else if (isa<llvm::StructType>(*elem)) {
            auto *selem = dyn_cast<llvm::StructType>(elem);
            unsigned elemWidth = getStructWidth((*selem), width, true);
            // (*fieldIndex)[width]->insert(elemWidth);
            width = elemWidth;
        }
    }
    return width;
}

unsigned 
PrimateArchGen::getStructWidth(StructType &s, unsigned start, const bool arcGen) {
    unsigned width = start;
    for (auto elem = s.element_begin(); elem != s.element_end(); elem++) {
        if (arcGen) {
            if (fieldIndex->find(width) == fieldIndex->end()) {
                (*fieldIndex)[width] = new std::set<unsigned>();
            }
        }
        if (isa<llvm::IntegerType>(**elem)) {
            unsigned elemWidth = (*elem)->getIntegerBitWidth();
            if (arcGen) {
                // insert new gather mode if doesn't exist
                gatherModes->insert(elemWidth);
                // the width of all numbers possibly stored at each index
                if (std::find((*fieldIndex)[width]->begin(), 
                              (*fieldIndex)[width]->end(), 
                              elemWidth) == (*fieldIndex)[width]->end()) {
                    (*fieldIndex)[width]->insert(elemWidth);
                }
            }
            width += elemWidth;
        } else if (isa<llvm::ArrayType>(**elem)) {
                       
            auto *selem = dyn_cast<llvm::ArrayType>(*elem);
            unsigned elemWidth;
            if (arcGen)  {
                elemWidth = getArrayWidthArcGen((*selem), width);
                // neeed to acess this element since its a struct member
                (*fieldIndex)[width]->insert(getArrayWidth((*selem), 0));
            }
            else elemWidth = getArrayWidth((*selem), width);
            width = elemWidth;
        } else if (isa<llvm::StructType>(**elem)) {
            auto *selem = dyn_cast<llvm::StructType>(*elem);
            unsigned elemWidth = getStructWidth((*selem), width, arcGen);
            if (arcGen)  {
                (*fieldIndex)[width]->insert(getStructWidth((*selem), 0, false));
            }
            width = elemWidth;
        }
    }
    // if (arcGen) {
    //     if (fieldIndex->find(width) == fieldIndex->end()) {
    //         (*fieldIndex)[width] = new std::set<unsigned>();
    //     }
    // }
    return width;
}

// why does this function need to have side-effects? 
unsigned PrimateArchGen::getTypeBitWidth(Type *ty, bool trackSizes) {
    unsigned size;
    if (ty->isIntegerTy()) {
        size = ty->getIntegerBitWidth();
        if (fieldIndex->find(0) == fieldIndex->end()) {
            (*fieldIndex)[0] = new std::set<unsigned>();
        }
        if (trackSizes)  {
            (fieldIndex->at(0))->insert(size);
            gatherModes->insert(size);
        }
    } else if (ty->isStructTy()) {
        StructType *sty = dyn_cast<StructType>(ty);
        size = getStructWidth(*sty, 0, trackSizes);
    } else if (ty->isArrayTy()) {
        ArrayType *aty = dyn_cast<ArrayType>(ty);
        size = getArrayWidth(*aty, 0);
    }
    return size;
}

Type* followPointerForType(Value* start) {
    Value* curInst = start;
    while(true) {
        if(AllocaInst* allocaArg = dyn_cast<AllocaInst>(curInst)) {
            return allocaArg->getAllocatedType();
            break;
        }
        else if(BitCastInst* bci = dyn_cast<BitCastInst>(curInst)) {
            curInst = bci->getOperand(0);
        }
        else if(GetElementPtrInst* gepI = dyn_cast<GetElementPtrInst>(curInst)) {
            curInst = gepI->getPointerOperand(); 
        }
        else if(auto* loadI = dyn_cast<LoadInst>(curInst)) {
            curInst = loadI->getPointerOperand();
        }
        else if(auto* storeI = dyn_cast<StoreInst>(curInst)) {
            curInst = storeI->getPointerOperand();
        }
        else {
            curInst->dump();
            llvm_unreachable("can't follow a pointer..");
        }
    }
}

void PrimateArchGen::printRegfileKnobs(Module &M, raw_fd_stream &primateCFG) {
    auto structTypes = M.getIdentifiedStructTypes();
    unsigned maxRegWidth = 0;
    gatherModes = new std::set<unsigned>();
    gatherModes->insert(32);
    fieldIndex = new std::map<unsigned, std::set<unsigned>*>(); // offset -> sizes

    // need to check functions that are marked as BFUs for types, 
    // not the structs themselves.
    for (auto& F: M) {
        MDNode* primateMD = F.getMetadata("primate");
        if (primateMD && 
            dyn_cast<MDString>(primateMD->getOperand(0))->getString() == "blue") {
            LLVM_DEBUG(dbgs() << "found primate Function: "; F.dump(););
            int argIdx = 0;
            for (auto& arg: F.args()) {
                // this uses the type of pointers. 
                // we need to use the call instruction 
                // so we don't use the type of the pointer :/
                Type *argTy = nullptr;
                LLVM_DEBUG(dbgs() << "arg type: "; arg.dump();
                           dbgs() << "arg type: "; arg.getType()->dump();
                           F.getType()->dump(););
                if(arg.getType()->isPointerTy()) {
                    // get users (calls) and check their args
                    for (auto user: F.users()) {
                        if (auto *call = dyn_cast<CallInst>(user)) {
                            auto* callArg = call->getOperand(argIdx);
                            if(callArg->getType()->isPointerTy()) {
                                argTy = followPointerForType(callArg);
                            }
                            else {
                                llvm_unreachable("pointer in signature is not a pointer in call");
                            }
                        }
                    }
                }
                else {
                    argTy = arg.getType();
                }
                unsigned regWidth = getTypeBitWidth(argTy, true);
                if (regWidth > maxRegWidth) {
                    maxRegWidth = regWidth;
                }
                LLVM_DEBUG(dbgs() << "reg width of type : "; argTy->dump(););
                LLVM_DEBUG(dbgs() << regWidth << "\n";);
                argIdx++;
            }
            LLVM_DEBUG(dbgs() << "return type: "; F.getReturnType()->dump(););
            Type* retType = F.getReturnType();
            if(retType->isPointerTy()) {
                LLVM_DEBUG(dbgs() << "**WARNING** Function returns a pointer\n";);
            }
            else if(!retType->isVoidTy()) {
                unsigned regWidth = getTypeBitWidth(retType, true);
                if (regWidth > maxRegWidth) {
                    maxRegWidth = regWidth;
                }
                LLVM_DEBUG(dbgs() << "reg width of type : "; retType->dump(););
                LLVM_DEBUG(dbgs() << regWidth << "\n";);
            }
        }
    }
    // fieldIndex contains the sizes and offsets of all fields in all Primate Structs
    if(maxRegWidth == 0) {
        maxRegWidth = 32;
    }
    if(fieldIndex->size() == 0) {
        (*fieldIndex)[0] = new std::set<unsigned>();
        fieldIndex->at(0)->insert(maxRegWidth);
    }
    else {
        fieldIndex->at(0)->insert(maxRegWidth);
        // fieldIndex contains the sizes and offsets of all fields in all Primate Structs
    }

    primateCFG << "REG_WIDTH=" << maxRegWidth << "\n";

    LLVM_DEBUG(dbgs() << "after checking all function calls we have field index mappings: \n";
    for(const auto& [index, value]: *fieldIndex) {
        dbgs() << "index: " << index << " field: ";
        for(const auto& vv: *value) { 
            dbgs() << vv << ", ";
        }
        dbgs() << "\n";
    }
    dbgs() << "max reg width: " << maxRegWidth << "\n";); 

    std::set<unsigned> allBitEnds;
    for (const auto& [offset, sizes]: *fieldIndex) {
        for(const auto& size: *sizes) {
            allBitEnds.insert(offset + size);
        }
    }

    primateCFG << "REG_BLOCK_WIDTH=";
    auto lastBlockEnd = 0;
    std::vector<std::pair<unsigned, unsigned>> regBlockWidths; // (width, mask)
    unsigned mask = 1;
    for (auto currentBlock: allBitEnds) {
        regBlockWidths.push_back({currentBlock - lastBlockEnd, mask});
        primateCFG << currentBlock - lastBlockEnd << " ";
        lastBlockEnd = currentBlock;
        mask = (mask << 1);
    }
    primateCFG << "\n";

    primateCFG << "NUM_REGBLOCKS=" << fieldIndex->size() - 1 << "\n";
    primateCFG << "SRC_POS=";
    std::set<unsigned> allOffsets;
    auto it = fieldIndex->begin();
    for (const auto& [offset, sizes]: *fieldIndex) {
        allOffsets.insert(offset);
        primateCFG << (offset) << " ";
    }
    primateCFG << "\n";

    primateCFG << "SRC_MODE=";

    std::set<unsigned> allSizes;
    for (const auto& [offset, sizes]: *fieldIndex) {
        for(const auto& size: *sizes) {
            allSizes.insert(size);
        }
    }

    int last_mode;
    for (auto &size: allSizes) {
        primateCFG << size << " ";
    }
    primateCFG << "\n";
    primateCFG << "MAX_FIELD_WIDTH=" << *(--allSizes.end()) << "\n";
    primateCFG << "NUM_SRC_POS=" << allOffsets.size() << "\n";
    primateCFG << "NUM_SRC_MODES=" << allSizes.size() << "\n";

    // offset positions
    primateCFG << "DST_POS=";
    for (auto &offset: allOffsets) {
        primateCFG << (offset) << " ";
    }
    primateCFG << "\n";

    // DST_ENCODE is a field representing the encoding of the 
    primateCFG << "DST_ENCODE=";
    for (int i = 0; i < allOffsets.size(); i++) {
        primateCFG << i << " ";
    } 
    primateCFG << "\n";

    // Enable encoding for each offset. 
    // formatted: offest size; offset size; offset size; ...
    primateCFG << "DST_EN_ENCODE=";
    int offsetEncode = 0;
    for (const auto& [offset, sizes] : *fieldIndex) {
        for (const auto& size: *sizes) {
            primateCFG << offsetEncode << " " 
                       << std::distance(allSizes.begin(), allSizes.find(size)) 
                       << ";";
        }
        offsetEncode++;
    }
    primateCFG << "\n";

    // DST_EN is a field representing the block write enables required to cover an 
    // offset, size pair
    std::vector<unsigned> scatterWbens; 
    int numBlocks = regBlockWidths.size();
    for (const auto& [offset, sizes] : *fieldIndex) {
        // skip blocks until we match offset
        unsigned offsetTemp = offset;
        int maskSkippedBlocks = 0;
        auto blockIterator = regBlockWidths.begin();
        while (offsetTemp > 0) {
            maskSkippedBlocks += 1;
            offsetTemp -= blockIterator->first;
            blockIterator++;
        }

        // enable blocks until we cover the size
        for(const auto& size: *sizes) {
            unsigned blockMask = 0;
            unsigned sizeCounter = size;
            auto sizeBlockIterator = blockIterator;
            while(sizeCounter > 0 && sizeBlockIterator != regBlockWidths.end()) {
                blockMask |= sizeBlockIterator->second << maskSkippedBlocks;
                sizeCounter -= sizeBlockIterator->first;
                sizeBlockIterator++;
            } 
            assert(sizeCounter == 0 && "failed to enable for the given reg blocks and the size");
            scatterWbens.push_back(blockMask);
        }
    }

    primateCFG << "DST_EN=";
    for (int i = 0; i < scatterWbens.size(); i++) {
        primateCFG << scatterWbens[i] << " ";
    }
    primateCFG << "\n"
               << "NUM_DST_POS=" << fieldIndex->size()-1 << "\n"
               << "NUM_WB_ENS=" << scatterWbens.size() << "\n";
}

void PrimateArchGen::generate_header(Module &M, raw_fd_stream &primateHeader) {
    auto structTypes = M.getIdentifiedStructTypes();
    unsigned maxRegWidth = 0;
    gatherModes = new std::set<unsigned>();
    fieldIndex = new std::map<unsigned, std::set<unsigned>*>();
    primateHeader << "import chisel3._\nimport chisel3.util._\n\n";
    for (auto it = structTypes.begin(); it != structTypes.end(); it++) {
        if ((*it)->getName().contains("input_t")) {
            unsigned elemWidth = getStructWidth((**it), 0, false);
            primateHeader << "class input_t extends Bundle {\n";
            primateHeader << "    val empty = UInt(" << (elemWidth+7)/8 << ".W)\n";
            primateHeader << "    val data = UInt(" << elemWidth << ".W)\n";
            primateHeader << "}\n";
        } else if ((*it)->getName().contains("output_t")) {
            unsigned elemWidth = getStructWidth((**it), 0, false);
            primateHeader << "class output_t extends Bundle {\n";
            primateHeader << "    val empty = UInt(" << (elemWidth+7)/8 << ".W)\n";
            primateHeader << "    val data = UInt(" << elemWidth << ".W)\n";
            primateHeader << "}\n";
        }
    }
}

unsigned PrimateArchGen::getMaxConst(Function &F) {
    APInt maxVal(32, 0);
    for (inst_iterator instruction = inst_begin(F), 
         e = inst_end(F); instruction != e; ++instruction) {
        Instruction *tempInst = &*instruction;
        User::op_iterator OI;
        if (!(isa<AllocaInst>(*tempInst)        || 
              isa<GetElementPtrInst>(*tempInst) || 
              isa<BitCastInst>(*tempInst)       || 
              isa<ZExtInst>(*tempInst)          || 
              isa<BranchInst>(*tempInst)        || 
              isa<CallInst>(*tempInst))) {
            for (OI = tempInst->op_begin(); OI != tempInst->op_end(); ++OI) {
                if (isa<ConstantInt>(*OI)) {
                    auto* constOp = dyn_cast<llvm::ConstantInt>(&*OI);
                    unsigned bitWidth = constOp->getBitWidth();
                    auto constVal = constOp->getValue();
                    APInt val = constVal;
                    if (bitWidth > 32) {
                        val = constVal.truncSSat(32);
                    } else if (bitWidth < 32) {
                        val = constVal.sext(32);
                    }
                    if (val.abs().ugt(maxVal)) {
                        // errs() << constVal << ": ";
                        // tempInst->print(errs());
                        // errs() << " " << val << "\n";
                        maxVal = val.abs();
                    }
                }
            }
        }
    }
    // errs() << "Max immediate: " << maxVal << "\n";
    return unsigned(maxVal.getZExtValue());
}

std::vector<Value*>* PrimateArchGen::getBFCOutputs(Instruction *ii) {
    if (isa<llvm::CallInst>(&*ii)) {
        MDNode *PrimateMetadata = ii->getMetadata("primate");
        if (PrimateMetadata != NULL) {
            if (cast<MDString>(
                PrimateMetadata->getOperand(0))->getString() == "blue") {
                
                auto numIn_i = 
                    cast<ConstantInt>(cast<ConstantAsMetadata>(
                            PrimateMetadata->getOperand(3))->getValue())
                                                           ->getValue();
                
                unsigned numIn = unsigned(numIn_i.getZExtValue());

                // last operand is always metadata
                if (numIn < (ii->getNumOperands()-1)) { 
                    std::vector<Value*> *outList = new std::vector<Value*>();
                    for (int i = numIn; i < (ii->getNumOperands()-1); i++) {
                        outList->push_back(ii->getOperand(i));
                    }
                    return outList;
                }
            }
        }
    }
    return NULL;
}

std::vector<Value*>* PrimateArchGen::getBFCInputs(Instruction *ii) {
    if (isa<llvm::CallInst>(&*ii)) {
        MDNode *PrimateMetadata = ii->getMetadata("primate");
        if (PrimateMetadata != NULL) {
            if (cast<MDString>(
                PrimateMetadata->getOperand(0))->getString() == "blue") {
                
                auto numIn_i = 
                    cast<ConstantInt>(cast<ConstantAsMetadata>(
                        PrimateMetadata->getOperand(3))->getValue())->getValue();
                
                unsigned numIn = unsigned(numIn_i.getZExtValue());
                
                if (numIn > 0) {
                    std::vector<Value*> *inList = new std::vector<Value*>();
                    for (int i = 0; i < numIn; i++) {
                        inList->push_back(ii->getOperand(i));
                    }
                    return inList;
                }
            }
        }
    }
    return NULL;
}

bool
PrimateArchGen::checkMemAlias(Value *ptr0, unsigned size0, 
                              Value *ptr1, unsigned size1) {
    if (pointerMap->find(ptr0) == pointerMap->end()) {
        LLVM_DEBUG(ptr0->dump();
        errs() << "pointer0 not initialized\n";
        ptr0->print(errs());
        errs() << "\n";);
        return true;
    }
    if (!(*pointerMap)[ptr0]->known_pointer) {
        return true;
    }
    
    if (pointerMap->find(ptr1) == pointerMap->end()) {
        LLVM_DEBUG(ptr1->dump();
        errs() << "pointer1 not initialized\n";
        ptr1->print(errs());
        errs() << "\n";);
        return true; // if we miss in the map assume its an alias
    }
    if (!(*pointerMap)[ptr1]->known_pointer) {
        return true;
    }

    Value* base0 = (*pointerMap)[ptr0]->base;
    Value* base1 = (*pointerMap)[ptr1]->base;
    unsigned offset0 = (*pointerMap)[ptr0]->offset;
    unsigned offset1 = (*pointerMap)[ptr1]->offset;

    if (base0 == base1) {
        if (!((offset0 + size0 <= offset1) || (offset0 >= offset1 + size1))) {
            return true;
        }
    }
    return false;
}

bool PrimateArchGen::isReachable(Value* src, std::set<Value*> &dst) {
    // simple DFS
    std::vector<Value*> stack;
    std::set<Value*> visitedNodes; 
    for (auto it = (*dependencyForest)[src]->begin(); 
         it != (*dependencyForest)[src]->end(); it++) {
        stack.push_back(it->first);
    }
    while (!stack.empty()) {
        Value *inst = stack.back();
        visitedNodes.insert(inst);
        if (dst.find(inst) != dst.end()) {
            return true;
        } else {
            stack.pop_back();
            if(dependencyForest->find(inst) == dependencyForest->end()) {
                // no dep info, idk what we use reachability for
                // fairly sure that true is conservative
                dbgs() << "depforest is missing an instruction\n";
                inst->dump();
                return true;
            }
            for (auto it = (*dependencyForest)[inst]->begin(); 
                 it != (*dependencyForest)[inst]->end(); it++) {
                if (visitedNodes.find(it->first) == visitedNodes.end()) {
                    stack.push_back(it->first);
                }
            }
        }
    }
    return false;
}

inline void 
PrimateArchGen::memInstAddRAWDep(Instruction* inst, Value* srcPtr, unsigned size, 
                                 ValueMap<Value*, 
                                    std::vector<std::pair<Value*, 
                                    unsigned>>> &storeInsts) {
    for (auto si = storeInsts.begin(); si != storeInsts.end(); si++) {
        // check all instructions that write memory
        // si->first->print(errs());
        // errs() << ":\n";
        bool isAlias = false;
        for (auto sp = si->second.rbegin(); sp != si->second.rend(); sp++) {
            if (checkMemAlias(srcPtr, size, sp->first, sp->second)) {
                // RAW dependency
                isAlias = true;
                // (*dependencyForest[&*inst])[si->first] = true;
                break;
            }
        }
        if (isAlias) {
            // Only add to the dependency list if it's an immediate dependency
            std::set<Value*> dst{si->first};
            bool immDep = true;
            for (auto dep = (*dependencyForest)[&*inst]->begin(); 
                 dep != (*dependencyForest)[&*inst]->end(); dep++) {
                if (dep->second && (isReachable(dep->first, dst))) {
                    immDep = false;
                    break;
                }
            }
            if (immDep) 
                (*(*dependencyForest)[&*inst])[si->first] = true;
        }
    }
}

inline void 
PrimateArchGen::memInstAddWARDep(Instruction* inst, Value* dstPtr, unsigned size, 
                                 ValueMap<Value*, 
                                    std::vector<std::pair<Value*, 
                                    unsigned>>> &loadInsts) {
    for (auto li = loadInsts.begin(); li != loadInsts.end(); li++) {
        if(llvm::dyn_cast<llvm::CallInst>(li->first)) {
            LLVM_DEBUG(errs() << "checking alias on a call inst.... NOT!\n";);
            continue;
        }
        // check all instructions that read memory
        // li->first->print(errs());
        // errs() << ":\n";
        for (auto lp = li->second.begin(); lp != li->second.end(); lp++) {
            if (checkMemAlias(dstPtr, size, lp->first, lp->second)) {
                // WAR dependency
                (*dependencyForest)[&*inst]->insert({li->first, false});
            }
        }
    }
}

void PrimateArchGen::initializeDependencyForest(Function &F) {
    ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> loadInsts;
    ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> storeInsts;
    dependencyForest->clear();
    loadMergedInst.clear();
    for (Function::iterator bi = F.begin(), be = F.end(); bi != be; bi++) {
        BasicBlock *bb = &*bi;
        loadInsts.clear();
        storeInsts.clear();
        for (BasicBlock::iterator ii = bb->begin(), 
                                  ie = bb->end(); ii != ie; ii++) {
            Instruction *inst = &*ii;
            // inst->print(errs());
            // errs() << ":\n";
            if (isa<LoadInst>(*inst)) {
                // inst->print(errs());
                // errs() << ":\n";
                (*dependencyForest)[&*inst] = new std::map<Value*, bool>();
                Value* srcPtr = inst->getOperand(0);
                // PointerType *ptrType = dyn_cast<PointerType>(srcPtr->getType());
                // Type *pteType = ptrType->getElementType();
                // unsigned size = getTypeBitWidth(pteType, false);
                unsigned size = getTypeBitWidth(inst->getType(), false);
                loadInsts[&*inst].push_back({srcPtr, size});
                memInstAddRAWDep(inst, srcPtr, size, storeInsts);
            } else if (isa<StoreInst>(*inst)) {
                // inst->print(errs());
                // errs() << ":\n";
                (*dependencyForest)[&*inst] = new std::map<Value*, bool>();
                auto *tmp = dyn_cast<llvm::StoreInst>(&*inst);
                Value *srcOp = tmp->getValueOperand();
                Value *ptrOp = tmp->getPointerOperand();
                if (isa<Instruction>(*srcOp)) {
                    Instruction *op_inst = dyn_cast<Instruction>(srcOp);
                    if (op_inst->getParent() == bb && (!isa<PHINode>(*op_inst)))
                        (*(*dependencyForest)[&*inst])[srcOp] = true;
                }
                PointerType *ptrType = dyn_cast<PointerType>(ptrOp->getType());
                // Type *pteType = ptrType->getElementType();
                // unsigned size = getTypeBitWidth(pteType, false);
                unsigned size = 
                    getTypeBitWidth(tmp->getValueOperand()->getType(), false);
                storeInsts[&*inst].push_back({ptrOp, size});
                memInstAddWARDep(inst, ptrOp, size, loadInsts);
            } else if (isa<CallInst>(*inst)) {
                auto *tmp = dyn_cast<llvm::CallInst>(&*inst);
                Function* foo = tmp->getCalledFunction();
                if (foo->getName().contains("memcpy")) {
                    (*dependencyForest)[&*inst] = new std::map<Value*, bool>();
                    Value *dstPtr = tmp->getOperand(0);
                    Value *srcPtr = tmp->getOperand(1);
                    Value *size = tmp->getOperand(2);
                    if (!isa<ConstantInt>(size)) {
                        errs() << "Error: memcpy does not have constant size\n";
                        inst->print(errs());
                        errs() << "\n";
                        exit(1);
                    }
                    auto *size_const = dyn_cast<ConstantInt>(size);
                    auto size_val = size_const->getValue();
                    int size_u = int(size_val.getSExtValue());
                    memInstAddRAWDep(inst, srcPtr, size_u, storeInsts);
                    memInstAddWARDep(inst, dstPtr, size_u, loadInsts);
                    loadInsts[&*inst].push_back({srcPtr, size_u*8});
                    storeInsts[&*inst].push_back({dstPtr, size_u*8});
                } else if (isBlueCall(inst)) {
                    (*dependencyForest)[&*inst] = new std::map<Value*, bool>();
                    std::vector<std::pair<Value*, unsigned>> inOps;
                    std::vector<std::pair<Value*, unsigned>> outOps;
                    auto inList = getBFCInputs(inst);
                    if (inList != NULL) {
                        for (auto op = inList->begin(); op != inList->end(); op++) {
                            Type* op_type = (*op)->getType();
                            if (op_type->isPointerTy()) {
                                Value *srcPtr = *op;
                                // PointerType *ptrType = dyn_cast<PointerType>(op_type);
                                // Type *pteType = ptrType->getElementType();
                                // unsigned size = getTypeBitWidth(pteType, false);
                                unsigned size = getTypeBitWidth(op_type, false);
                                inOps.push_back({srcPtr, size});
                                memInstAddRAWDep(inst, srcPtr, size, storeInsts);
                            }
                        }
                    }
                    auto outList = getBFCOutputs(inst);
                    if (outList != NULL) {
                        for (auto op = outList->begin(); op != outList->end(); op++) {
                            Type* op_type = (*op)->getType();
                            if (op_type->isPointerTy()) {
                                Value *dstPtr = *op;
                                // PointerType *ptrType = dyn_cast<PointerType>(op_type);
                                // Type *pteType = ptrType->getElementType();
                                // unsigned size = getTypeBitWidth(pteType);
                                unsigned size = getTypeBitWidth(op_type);
                                outOps.push_back({dstPtr, size});
                                memInstAddWARDep(inst, dstPtr, size, loadInsts);
                            }
                        }
                    }
                    if (inList != NULL)
                        loadInsts[&*inst].insert(loadInsts[&*inst].end(), 
                                                 inOps.begin(), inOps.end());
                    if (outList != NULL)
                        storeInsts[&*inst].insert(storeInsts[&*inst].end(), 
                                                  outOps.begin(), outOps.end());
                }
            } else if (!(isa<GetElementPtrInst>(*inst) || 
                         isa<BitCastInst>(*inst)       || 
                         isa<AllocaInst>(*inst)        || 
                         isa<PHINode>(*inst))) {
                (*dependencyForest)[&*inst] = new std::map<Value*, bool>();
                for (auto OI = inst->op_begin(); OI != inst->op_end(); ++OI) {
                    if (isa<Instruction>(*OI)) {
                        Instruction* op_inst = dyn_cast<Instruction>(OI);
                        if (op_inst->getParent() == bb && (!isa<PHINode>(*op_inst)))
                            (*(*dependencyForest)[&*inst])[&*op_inst] = true;
                    }
                }
            }
        }
    }
}

void PrimateArchGen::mergeExtInstructions() {
    for (auto it = dependencyForest->begin(); it != dependencyForest->end(); it++) {
        for (auto dep = it->second->begin(); dep != it->second->end();) {
            Value* dep_inst = dep->first;
            if (isa<ZExtInst>(*dep_inst) || isa<SExtInst>(*dep_inst)) {
                Value* new_dep = (*dependencyForest)[dep_inst]->begin()->first;
                bool rel = dep->second;
                // errs() << "start erase\n";
                // errs() << "erase success\n";
                if (new_dep != NULL) {
                    if (rel)
                        (*(it->second))[new_dep] = rel;
                    else
                        it->second->insert({new_dep, rel});
                }
                // errs() << "insert success\n";
                it->second->erase(dep++);
            } else {
                ++dep;
            }
        }
    }
    for (auto it = dependencyForest->begin(); it != dependencyForest->end();) {
        Value *inst = it->first;
        if (isa<ZExtInst>(*inst) || isa<SExtInst>(*inst)) {
            dependencyForest->erase(it++);
        } else {
            ++it;
        }
    }
}

void PrimateArchGen::mergeLoadInstructions() {
    ValueMap<Value*, std::set<Value*>> loadRAWDependents;
    ValueMap<Value*, std::set<Value*>> loadWARDependents;
    ValueMap<Value*, bool> loadMergeable;
    unmergeableLoad.clear();
    for (auto it = dependencyForest->begin(); it != dependencyForest->end(); it++) {
        if (isa<LoadInst>(*(it->first))) {
            loadMergeable[it->first] = true;
        } else {
            for (auto dep = it->second->begin(); dep != it->second->end(); dep++) {
                if (isa<LoadInst>(*(dep->first))) {
                    if (dep->second)
                        loadRAWDependents[dep->first].insert(it->first);
                    else
                        loadWARDependents[dep->first].insert(it->first);
                }
            }
        }
    }
    for (auto it = loadRAWDependents.begin(); it != loadRAWDependents.end(); it++) {
        for (auto dep = it->second.begin(); dep != it->second.end(); dep++) {
            if (isReachable(*dep, loadWARDependents[it->first])) {
                // load is not mergeable if any of its RAW dependents depend on its WAR dependents
                loadMergeable[it->first] = false;
                unmergeableLoad.insert(it->first);
                break;
            }
        }
    }
    for (auto it = loadMergeable.begin(); it != loadMergeable.end(); it++) {
        if (it->second) {
            // errs() << "load mergeable\n";
            auto newDepList = (*dependencyForest)[it->first];
            for (auto inst_it = loadRAWDependents[it->first].begin(); 
                 inst_it != loadRAWDependents[it->first].end(); inst_it++) {
                loadMergedInst.insert(*inst_it);
                int erase_count = (*dependencyForest)[*inst_it]->erase(it->first);
                // (*inst_it)->print(errs());
                // errs() << ": erase " << erase_count << "\n";
                for (auto newDep = newDepList->begin(); 
                     newDep != newDepList->end(); newDep++) {
                    (*(*dependencyForest)[*inst_it])[newDep->first] = newDep->second;
                }
            }
            for (auto inst_it = loadWARDependents[it->first].begin(); 
                 inst_it != loadWARDependents[it->first].end(); inst_it++) {
                (*dependencyForest)[*inst_it]->erase(it->first);
            }
            dependencyForest->erase(it->first);
        }
    }
}

void PrimateArchGen::mergeStoreInstructions() {
    ValueMap<Value*, Value*> storeMap;
    unmergeableStore.clear();
    for (auto it = dependencyForest->begin(); it != dependencyForest->end();) {
        if(isa<StoreInst>(*(it->first))) {
            std::set<Value*> storeRAWDependents;
            std::set<Value*> storeWARDependents;
            Value* storeSrc;
            for (auto dep = it->second->begin(); dep != it->second->end(); dep++) {
                if (dep->second) {
                    storeSrc = dep->first;
                    storeRAWDependents.insert(dep->first);
                } else {
                    storeWARDependents.insert(dep->first);
                }
            }
            bool mergeable = true;
            if (storeRAWDependents.empty()) mergeable = false;
            for (auto dep = storeWARDependents.begin(); 
                 dep != storeWARDependents.end(); dep++) {
                if (isReachable(*dep, storeRAWDependents)) {
                    mergeable = false;
                    break;
                }
            }
            if (mergeable) {
                for (auto dep = storeWARDependents.begin(); 
                     dep != storeWARDependents.end(); dep++) {
                    if(dependencyForest->find(storeSrc) == dependencyForest->end()) {
                        dbgs() << "storeSrc not in depforest\n";
                        storeSrc->dump();
                    }
                    (*dependencyForest)[storeSrc]->insert({*dep, false});
                }
                storeMap[it->first] = storeSrc;
                dependencyForest->erase(it++);
            } else {
                unmergeableStore.insert(it->first);
                ++it;
            }
        } else {
            ++it;
        }
    }
    for (auto it = dependencyForest->begin(); it != dependencyForest->end(); it++) {
        for (auto dep = it->second->begin(); dep != it->second->end();) {
            if (storeMap.find(dep->first) != storeMap.end()) {
                // depend on a mergeable store instruction
                Value* newDep = storeMap[dep->first];
                bool depType = dep->second;
                if (depType)
                    (*(it->second))[newDep] = depType;
                else
                    it->second->insert({newDep, depType});
                it->second->erase(dep++);
            } else {
                ++dep;
            }
        }
    }
}

void PrimateArchGen::addControlDependency() {
    for (auto it = dependencyForest->begin(); it != dependencyForest->end(); it++) {
        Instruction *inst = dyn_cast<Instruction>(it->first);
        if (inst->isTerminator()) {
            // terminator instruction has control dependence on 
            // all other instructions in the same BB
            BasicBlock *bb = inst->getParent();
            for (BasicBlock::iterator dep = bb->begin(); dep != bb->end(); dep++) {
                if ((dependencyForest->find(&*dep) != dependencyForest->end()) && 
                    (&*dep != &*inst))
                    (*dependencyForest)[&*inst]->insert({&*dep, false});
            }
        }
    }
}

void PrimateArchGen::bruMerge(Instruction* inst, int level, int numALU) {
    // inst->print(errs());
    // errs() << ": " << inst->getNumOperands() << "\n";
    // Branch frontier has all instructions combinable to a single Primate branch instruction
    std::set<Value*> *frontier = new std::set<Value*>();
    std::set<Value*> newFrontier;
    int aluBudget;
    BasicBlock *bb = inst->getParent();
    if (isa<CallInst>(*inst) || isa<LoadInst>(*inst) || isa<StoreInst>(*inst)) {
        frontier->insert(&*inst);
    } else {
        newFrontier.insert(&*inst);
    }
    for (int i = 0; i < level; i++) {
        std::set<Value*> newFrontierNext;
        aluBudget = numALU - (frontier->size() + newFrontier.size());
        for (auto it = newFrontier.begin(); it != newFrontier.end(); it++) {
            // Inst needs to occupy an ALU slot if a load instruction is merged to it
            bool erasable = (loadMergedInst.find(*it) == loadMergedInst.end());
            Instruction *mergeInst = dyn_cast<Instruction>(*it);
            // Inst needs to occupy an ALU slot to load operand if any of its 
            // operand is not computed in the same BB
            for (auto OI = mergeInst->op_begin(); OI != mergeInst->op_end(); OI++) {
                if (isa<Instruction>(*OI)) {
                    Instruction *op_inst = dyn_cast<Instruction>(&*OI);
                    if (op_inst->getParent() != mergeInst->getParent()) {
                        erasable = false;
                        break;
                    }
                } else if (isa<Argument>(*OI)) {
                    erasable = false;
                    break;
                }
            }
            int numRAWDep = 0;
            std::vector<Value*> mergeableDep;
            // Check all RAW dependencies and find which instruction is mergeable
            for (auto dep = (*dependencyForestOp)[*it]->begin(); 
                 dep != (*dependencyForestOp)[*it]->end(); dep++) {
                if (dep->second) {
                    Value *RAWDepInst = dep->first;
                    bool mergeable = true;
                    if (!isBlueCall(dyn_cast<Instruction>(RAWDepInst))) {
                        // Check all Primate instructions in the same BB
                        for (auto pInst = bb->rbegin(); pInst != bb->rend(); pInst++) {
                            auto pInstDep = dependencyForestOp->find(&*pInst);
                            if (pInstDep != dependencyForestOp->end()) {
                                // Only instructions in the new frontier can depend 
                                // on the instruction to be merged to
                                if (newFrontier.find(&*pInst) == newFrontier.end()) {
                                    if (pInstDep->second->find(RAWDepInst) != 
                                        pInstDep->second->end()) {
                                        mergeable = false;
                                        break;
                                    }
                                }
                            }
                        }
                    } else {
                        mergeable = false;
                    }
                    if (mergeable) {
                        numRAWDep++;
                        mergeableDep.push_back(RAWDepInst);
                    } else {
                        erasable = false;
                    }
                }
            }
            if (erasable && (numRAWDep <= aluBudget+1)) {
                aluBudget++;
            } else {
                erasable = false;
            }
            for (int j = 0; j < aluBudget, j < numRAWDep; j++) {
                Value *mergeDep = mergeableDep.front();
                if (isa<CallInst>(*mergeDep) || 
                    isa<LoadInst>(*mergeDep) || 
                    isa<StoreInst>(*mergeDep)) {
                    frontier->insert(mergeDep);
                } else {
                    newFrontierNext.insert(mergeDep);
                }
                (*(*dependencyForestOp)[*it])[mergeDep] = false;
                mergeableDep.erase(mergeableDep.begin());
            }
            if (erasable) {
                combinedBranchInst.insert(*it);
                dependencyForestOp->erase(*it);
            } else {
                frontier->insert(*it);
            }
        }
        newFrontier = newFrontierNext;
    }
    for (auto it = newFrontier.begin(); it != newFrontier.end(); it++) {
        frontier->insert(*it);
    }
    // Instructions in the frontier must be scheduled with or after all other instructions
    // Add WAR dependency on all other instructions to the frontier instructions
    for (auto pInst = bb->begin(); pInst != bb->end(); pInst++) {
        auto pInstDep = dependencyForestOp->find(&*pInst);
        if (pInstDep != dependencyForestOp->end()) {
            if (frontier->find(pInstDep->first) == frontier->end()) {
                for (auto brInst = frontier->begin(); 
                     brInst != frontier->end(); brInst++) {
                    (*dependencyForestOp)[*brInst]->insert({pInstDep->first, false});
                }
            }
        }
    }
    frontiers[bb] = frontier;
}

void PrimateArchGen::mergeBranchInstructions(int level, int numALU) {
    combinedBranchInst.clear();
    for (auto it = dependencyForest->begin(); it != dependencyForest->end(); it++) {
        Instruction *inst = dyn_cast<Instruction>(it->first);
        Value* cond;
        if (inst->isTerminator()) {
            if (isa<BranchInst>(*inst)) {
                BranchInst* brInst = dyn_cast<BranchInst>(inst);
                if (brInst->isUnconditional()) {
                    dependencyForestOp->erase(it->first);
                    continue;
                } else {
                    cond = brInst->getCondition();
                }
            } else if (isa<SwitchInst>(*inst)) {
                SwitchInst* swInst = dyn_cast<SwitchInst>(inst);
                cond = swInst->getCondition();
            } else if (isa<ReturnInst>(*inst)) {
                // Primate program must return results through BFU
                dependencyForestOp->erase(it->first);
                continue;
            } else {
                errs() << "Terminator instruction not supported!\n";
                exit(1);
            }

            if (isa<Instruction>(*cond)) {
                auto condInst = dyn_cast<Instruction>(cond);
                // Condition value must be an instruction to merge to
                if (dependencyForestOp->find(cond) != dependencyForestOp->end()) {
                    bool mergeable = true;
                    BasicBlock *bb = inst->getParent();
                    // Check all Primate instructions in the same BB
                    for (auto pInst = bb->rbegin(); pInst != bb->rend(); pInst++) {
                        auto pInstDep = dependencyForestOp->find(&*pInst);
                        if (pInstDep != dependencyForestOp->end()) {
                            // No instruction other than branch instruction depends 
                            // on the condition value
                            if (pInstDep->first != (&*inst)) {
                                if (pInstDep->second->find(cond) != 
                                    pInstDep->second->end()) {
                                    mergeable = false;
                                    break;
                                }
                            }
                        }
                    }
                    if (mergeable) {
                        combinedBranchInst.insert(it->first);
                        dependencyForestOp->erase(it->first);
                        bruMerge(condInst, level-1, numALU);
                    } else {
                        frontiers[bb] = new std::set<Value*>({it->first});
                    }
                }
            } else {
                errs() << "Condition value is not a instruction\n";
                exit(1);
            }
        }
    }
}

void PrimateArchGen::buildDependencyForest(Function &F) {
    initializeDependencyForest(F);
    mergeExtInstructions();
    mergeLoadInstructions();
    mergeStoreInstructions();
    // addControlDependency();
    // int n = 0;
    // for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ii++) {
    //     if (dependencyForest.find(&*ii) != dependencyForest.end()) {
    //         ii->print(errs());
    //         errs() << "\ndepends on:\n";
    //         int i = 1;
    //         auto depList = dependencyForest[&*ii];
    //         for (auto dep = depList->begin(); dep != depList->end(); dep++) {
    //             errs() << i << ": ";
    //             dep->first->print(errs());
    //             errs() << "; " << dep->second << "\n";
    //             i++;
    //         }
    //         errs() << "\n";
    //         // if (n > 50) break;
    //         n++;
    //     }
    // }
}

void PrimateArchGen::optimizeDependencyForest(int bruDepth, int numALU) {
    dependencyForestOp->clear();
    frontiers.clear();
    for (auto it = dependencyForest->begin(); 
         it != dependencyForest->end(); it++) {
        (*dependencyForestOp)[it->first] = 
            new std::map<Value*, bool>((*it->second));
    }
    mergeBranchInstructions(bruDepth, numALU);
}

void PrimateArchGen::printDependencyForest(Function &F) {
    int n = 0;
    for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ii++) {
        if (dependencyForestOp->find(&*ii) != dependencyForestOp->end()) {
            ii->print(errs());
            errs() << "\ndepends on:\n";
            int i = 1;
            auto depList = (*dependencyForestOp)[&*ii];
            for (auto dep = depList->begin(); dep != depList->end(); dep++) {
                errs() << i << ": ";
                dep->first->print(errs());
                errs() << "; " << dep->second << "\n";
                i++;
            }
            errs() << "\n";
            // if (n > 50) break;
            n++;
        }
    }
    n = 0;
    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock *bb = &*bi;
        errs() << "frontier " << n << ":\n";
        if (frontiers.find(bb) != frontiers.end()) {
            for (auto it = frontiers[bb]->begin(); it != frontiers[bb]->end(); it++) {
                (*it)->print(errs());
                errs() << "\n";
            }
            errs() << "\n";
        }
        n++;
    }
}

void PrimateArchGen::annotatePriority(Function &F, bool optimized) {
    ValueMap<Value*, std::map<Value*, bool>*>* dag;
    if (optimized) 
        dag = dependencyForestOp;
    else
        dag = dependencyForest;
    
    instPriority->clear();
    std::vector<Value*> waitlist;
    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock *bb = &*bi;
        waitlist.clear();
        for (auto ii = bb->rbegin(); ii != bb->rend(); ++ii) {
            if (dag->find(&*ii) != dag->end()) {
                // is a Primate instruction
                int selfPriority = 0;
                if (instPriority->find(&*ii) == instPriority->end()) {
                    (*instPriority)[(&*ii)] = 0;
                } else {
                    selfPriority = (*instPriority)[&*ii];
                }
                for (auto dep = (*dag)[&*ii]->begin(); dep != (*dag)[&*ii]->end(); dep++) {
                    int newPriority;
                    Instruction *inst = dyn_cast<Instruction>(&*ii);
                    if (isBlueCall(inst)) newPriority = selfPriority;
                    else {
                        if (dep->second) {
                            newPriority = selfPriority + 2;
                        } else {
                            newPriority = selfPriority + 1;
                        }
                    }
                    if (instPriority->find(dep->first) == instPriority->end()) {
                        (*instPriority)[dep->first] = newPriority;
                    } else if ((*instPriority)[dep->first] < newPriority) {
                        (*instPriority)[dep->first] = newPriority;
                        waitlist.push_back(dep->first);
                    }
                }
            }
        }
        while (!waitlist.empty()) {
            Value* pInst = waitlist.front();
            waitlist.erase(waitlist.begin());
            if (dag->find(pInst) == dag->end()) {
                LLVM_DEBUG(errs() << "Instruction not found in DAG\n"; pInst->print(errs()); errs() << "\n";);
                continue;
            }
            for (auto dep = (*dag)[pInst]->begin(); dep != (*dag)[pInst]->end(); dep++) {
                int newPriority;
                Instruction *inst = dyn_cast<Instruction>(pInst);
                if (isBlueCall(inst)) newPriority = (*instPriority)[pInst];
                else {
                    if (dep->second) {
                        newPriority = (*instPriority)[pInst] + 2;
                    } else {
                        newPriority = (*instPriority)[pInst] + 1;
                    }
                }
                if ((*instPriority)[dep->first] < newPriority) {
                    (*instPriority)[dep->first] = newPriority;
                    waitlist.push_back(dep->first);
                }
            }
        }
    }
}

int PrimateArchGen::estimateNumALUs(Function &F) {
    annotatePriority(F, false);
    int numALU = 0;
    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock *bb = &*bi;
        int maxPriority = 0;
        int numALUInst = 0;
        for (auto ii = bb->begin(); ii != bb->end(); ++ii) {
            if (dependencyForest->find(&*ii) != dependencyForest->end()) {
                Instruction *inst = dyn_cast<Instruction>(&*ii);
                if (!isBlueCall(inst) && !inst->isTerminator()) {
                    // inst->print(errs());
                    // errs() << "\n";
                    numALUInst++;
                }
                if ((*instPriority)[&*ii] > maxPriority) {
                    maxPriority = (*instPriority)[&*ii];
                }
            }
        }
        // errs() << "maxPriority: " << maxPriority << "\n";
        // errs() << "numALUInst: " << numALUInst << "\n";
        float numALUBB = numALUInst*2.0/(maxPriority+2.0);
        if (ceil(numALUBB) > numALU) numALU = ceil(numALUBB);
    }
    if (numALU < numALU_min) numALU = numALU_min;
    // errs() << "estimated ALUs: " << numALU << "\n";
    return numALU;
}

void 
PrimateArchGen::addBFCDependency(Value* bfc, 
                                 std::map<Value*, std::set<Value*>> &bfcConflict, 
                                 std::set<Value*> &toScheduleInst) {
    for (auto dep = (*dependencyForestOp)[bfc]->begin(); 
         dep != (*dependencyForestOp)[bfc]->end(); dep++) {
        Instruction* depInst = dyn_cast<Instruction>(dep->first);
        if (dep->second) {
            if ((!isBlueCall(depInst)) && 
                (toScheduleInst.find(dep->first) != toScheduleInst.end())) {
                bfcConflict[&*depInst].insert(bfc);
            }
        }
    }
}

void 
PrimateArchGen::addBFDependency(std::map<Value*, 
                                std::set<Value*>> &bfcConflict, 
                                std::set<Value*> &bfcSet) {
    std::map<std::pair<Value*, Value*>, int> bfcConflictCounter;
    for (auto bfc0 = bfcSet.begin(); bfc0 != bfcSet.end(); bfc0++) {
        for (auto bfc1 = bfcSet.begin(); bfc1 != bfc0; bfc1++) {
            std::pair<Value*, Value*> bfcPair({*bfc1, *bfc0});
            bfcConflictCounter[bfcPair] = 0;
        }
    }
    for (auto dep = bfcConflict.begin(); 
         dep != bfcConflict.end(); dep++) {
        for (auto bfc0 = (dep->second).begin(); 
             bfc0 != (dep->second).end(); bfc0++) {
            for (auto bfc1 = (dep->second).begin(); 
                 bfc1 != bfc0; bfc1++) {
                std::pair<Value*, Value*> bfcPair{*bfc1, *bfc0};
                bfcConflictCounter[bfcPair] += 1;
            }
        }
    }
    for (auto bfcPair = bfcConflictCounter.begin(); 
         bfcPair != bfcConflictCounter.end(); bfcPair++) {
        auto *tmp0 = dyn_cast<llvm::CallInst>((bfcPair->first).first);
        Function *foo = tmp0->getCalledFunction();
        int fooIdx;
        auto fooIdx_it = bfIdx->find(&*foo);
        if (fooIdx_it != bfIdx->end()) {
            fooIdx = fooIdx_it->second;
        } else {
            errs() << "Blue Function not found!\n";
            exit(1);
        }
        auto *tmp1 = dyn_cast<llvm::CallInst>((bfcPair->first).second);
        Function *bar = tmp1->getCalledFunction();
        int barIdx;
        auto barIdx_it = bfIdx->find(&*bar);
        if (barIdx_it != bfIdx->end()) {
            barIdx = barIdx_it->second;
        } else {
            errs() << "Blue Function not found!\n";
            exit(1);
        }
        int conflictCount = bfcPair->second;
        if (conflictCount == 0) {
            bfConflictMap_tmp[fooIdx][barIdx] = 0;
            bfConflictMap_tmp[barIdx][fooIdx] = 0;
        } else {
            bfConflictMap_tmp[fooIdx][barIdx] = conflictCount;
            bfConflictMap_tmp[barIdx][fooIdx] = conflictCount;
        }
    }
}

// this is fundamentally wrong
// the dependency should be done via use-defs chains not this ad-hoc way
// memory dependencies are not handled correctly
// we should simple keep the loads and stores in order pointer alias checking is unreliable 
void PrimateArchGen::VLIWSim(Function &F, int numALU) {
    return;
    errs() << F.getName() << "\n";
    annotatePriority(F, true);
    int totalInst = 0;
    int totalVLIWInst = 0;
    for (int i = 0; i < numBFs; i++) {
        for (int j = 0; j < numBFs; j++) {
            bfConflictMap_tmp[i][j] = -1;
        }
    }
    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        errs() << "New Basic Block\n";
        BasicBlock *bb = &*bi;
        std::vector<std::pair<int, Value*>> nonFrontier;
        std::vector<Value*> blueCalls;
        int frontier_size = 0;
        auto frontier_it = frontiers.find(bb);
        if (frontier_it != frontiers.end()) {
            frontier_size = frontier_it->second->size();
        }
        for (auto ii = bb->begin(); ii != bb->end(); ++ii) {
            if (dependencyForestOp->find(&*ii) != dependencyForestOp->end()) {
                Instruction *inst = dyn_cast<Instruction>(&*ii);
                if (frontier_it != frontiers.end()) {
                    if (frontier_it->second->find(&*ii) == frontier_it->second->end()) {
                        if (isBlueCall(inst)) {
                            blueCalls.push_back(&*ii);
                        } else {
                            nonFrontier.push_back({(*instPriority)[&*ii], &*ii});
                        }
                    }
                } else {
                    if (isBlueCall(inst)) {
                        blueCalls.push_back(&*ii);
                    } else {
                        nonFrontier.push_back({(*instPriority)[&*ii], &*ii});
                    }
                }
            }
        }
        std::sort(nonFrontier.rbegin(), nonFrontier.rend());
        std::set<Value*> scheduledInst;
        int aluLeft = numALU;
        int n = 0;
        int numVLIWInst = 0;
        int numInst = 0;
        while (!(nonFrontier.empty() && blueCalls.empty())) {
            aluLeft = numALU;
            std::set<Value*> toScheduleInst;
            // First schedule ALU instructions
            auto inst = nonFrontier.begin();
            while (inst != nonFrontier.end()) {
                if (aluLeft == 0) {
                    break;
                }
                bool scheduleable = true;
                for (auto dep = (*dependencyForestOp)[inst->second]->begin(); 
                     dep != (*dependencyForestOp)[inst->second]->end(); dep++) {
                    // Loop all instructions it depends on
                    if (dep->second) {
                        if (scheduledInst.find(dep->first) == scheduledInst.end()) {
                            // Someone it strictly depends on hasn't been scheduled yet
                            scheduleable = false;
                            break;
                        }
                    } else {
                        if ((scheduledInst.find(dep->first) == scheduledInst.end()) && 
                            (toScheduleInst.find(dep->first) == toScheduleInst.end())) {
                            // Someone it loosely depends on hasn't been scheduled and is 
                            // not gonna be scheduled at the same time
                            scheduleable = false;
                            break;
                        }
                    }
                }
                if (scheduleable) {
                    aluLeft--;
                    toScheduleInst.insert(inst->second);
                    inst = nonFrontier.erase(inst);
                    numInst++;
                } else {
                    inst++;
                }
            }
            // Then schedule Blue function calls
            auto binst = blueCalls.begin();
            std::map<Value*, std::set<Value*>> bfcConflict;
            std::set<Value*> bfcSet;
            while (binst != blueCalls.end()) {
                bool scheduleable = true;
                int aluNeed = 0;
                for (auto dep = (*dependencyForestOp)[*binst]->begin(); 
                     dep != (*dependencyForestOp)[*binst]->end(); dep++) {
                    Instruction* depInst = dyn_cast<Instruction>(dep->first);
                    if (dep->second) {
                        // BFC inputs
                        if (isBlueCall(depInst)) {
                            if (scheduledInst.find(dep->first) == scheduledInst.end()) {
                                // Someone it strictly depends on is another BFC 
                                // and hasn't been scheduled yet
                                scheduleable = false;
                                break;
                            }
                        } else {
                            if (scheduledInst.find(dep->first) != scheduledInst.end()) {
                                // Dependency already executed, 
                                // need ALU to pass operand from regfile
                                aluNeed++;
                                if (aluLeft - aluNeed < 0) {
                                    scheduleable = false;
                                    break;
                                }
                            } else if (toScheduleInst.find(dep->first) == toScheduleInst.end()) {
                                // Dependency not executed and won't be 
                                // scheduled in the same instruction
                                scheduleable = false;
                                break;
                            }
                        }
                    } else {
                        if ((scheduledInst.find(dep->first) == scheduledInst.end()) && 
                            (toScheduleInst.find(dep->first) == toScheduleInst.end())) {
                            scheduleable = false;
                            break;
                        }
                    }
                }
                if (scheduleable) {
                    addBFCDependency(*binst, bfcConflict, toScheduleInst);
                    bfcSet.insert(*binst);
                    toScheduleInst.insert(*binst);
                    binst = blueCalls.erase(binst);
                    aluLeft -= aluNeed;
                } else {
                    binst++;
                }
            }
            // VLIW instruction scheduled
            addBFDependency(bfcConflict, bfcSet);
            numVLIWInst++;
            errs() << "From greedy packetize\n";
            errs() << "VLIW Inst " << n << ":\n";
            if(toScheduleInst.size() == 0) {
                errs() << "No instruction to schedule!!!!\n";
                for(auto& inst: nonFrontier) {
                    inst.second->print(errs());
                    errs() << "\n";
                }
            } else {
                for (auto inst = toScheduleInst.begin(); 
                    inst != toScheduleInst.end(); inst++) {
                    scheduledInst.insert(*inst);
                    (*inst)->print(errs());
                    errs() << "\n";
                }
            }
            n++;
        }
        // Schedule frontier instructions
        if (frontier_size > aluLeft) {
            errs() << "frontier size: " << frontier_size << "\n";
            errs() << "VLIW Inst " << n << ":\n";
            numVLIWInst++;
        } else if (frontier_size != 0) {
            if (numVLIWInst == 0) {
                errs() << "first VLIW Inst\n";
                errs() << "VLIW Inst " << n << ":\n";
                numVLIWInst = 1;
            }
            bool newInst = false;
            // If any instruction in the frontier has RAW dependency on 
            // previous instructions, it must be a new VLIW instruction
            for (auto inst = frontier_it->second->begin(); 
                 inst != frontier_it->second->end(); inst++) {
                for (auto dep = (*dependencyForestOp)[*inst]->begin(); 
                     dep != (*dependencyForestOp)[*inst]->end(); dep++) {
                    if (dep->second) {
                        newInst = true;
                        break;
                    }
                }
                if (newInst) {
                    break;
                }
            }
            if (newInst) {
                errs() << "new isnt due to RAW dependency\n";
                errs() << "VLIW Inst " << n << ":\n";
                numVLIWInst++;
            }
        }

        numInst += frontier_size;
        totalVLIWInst += numVLIWInst;
        totalInst += numInst;
        if (frontier_size != 0) {
            for (auto inst = frontier_it->second->begin(); 
                 inst != frontier_it->second->end(); inst++) {
                (*inst)->print(errs());
                errs() << "\n";
            }
        }
        bbNumVLIWInst[bb] = numVLIWInst;
        bbNumInst[bb] = numInst;
    }
    // if (totalVLIWInst != 0) {
    //     int aluUtil = round((totalInst+0.0)/totalVLIWInst/numALU*100.0);
    //     errs() << "Total number of VLIW instructions: " << totalVLIWInst << "\n";
    //     errs() << "Average ALU utilization:" << aluUtil << "%\n";
    // }
}

void PrimateArchGen::initializeBBWeight(Function &F) {
    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock *bb = &*bi;
        bbWeight[bb] = 1.0;
    }
}

int PrimateArchGen::evalPerf(Function &F, int numALU, 
                             double &perf, double &util) {
    optimizeDependencyForest(2, numALU);
    VLIWSim(F, numALU);

    perf = 0.0;
    util = 0.0;

    int numBB = 0;
    int numInst = 0;
    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock *bb = &*bi;
        if (bbNumVLIWInst[bb] > 0) {
            numBB++;
            numInst += bbNumVLIWInst[bb];
            perf += (bbWeight[bb] * bbNumVLIWInst[bb]);
            util += (bbWeight[bb] * bbNumInst[bb] / bbNumVLIWInst[bb] / numALU);
        }
    }

    if (numBB != 0) {
        perf /= numBB;
        util /= numBB;
    }

    errs() << "numALU: " << numALU 
           << ", perf: " << perf 
           << ", utilization: " << util << "\n"; 

    return numInst;
}

void PrimateArchGen::numALUDSE(Function &F, int &numALU, 
                               int &numInst, int option) {
    initializeBBWeight(F);
    double perf = 0.0;
    double util = 0.0;
    numInst = evalPerf(F, numALU, perf, util);

    if (numInst == 0) return;

    while (true) {
        double new_perf;
        double new_util;
        int new_numALU = numALU + 1;

        int new_numInst = evalPerf(F, new_numALU, new_perf, new_util);
        if (option == MAX_PERF) {
            if (new_perf >= perf) {
                break;
            }
        } else if (option == BALANCE) {
            if (util/new_util > perf/new_perf) {
                break;
            }
        }
        perf = new_perf;
        util = new_util;
        numALU = new_numALU;
        numInst = new_numInst;
    }

    optimizeDependencyForest(2, numALU);

    VLIWSim(F, numALU);
}

void PrimateArchGen::initializeBFCMeta(Module &M) {
    // Collect info about blue functions
    numBFs = 0;
    numALU_min = 1;
    for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
        MDNode *metadata = MI->getMetadata("primate");
        if (metadata) {
            if (cast<MDString>(metadata->getOperand(0))->getString() == "blue") {
                blueFunctions.push_back(&*MI);
                (*bfIdx)[&*MI] = numBFs;
                numBFs++;
                auto numIn_i = 
                    cast<ConstantInt>(
                    cast<ConstantAsMetadata>(metadata->getOperand(3))
                        ->getValue())->getValue();
                unsigned numIn = unsigned(numIn_i.getZExtValue());
                if (numIn > numALU_min) numALU_min = numIn;
                auto bfuName = cast<MDString>(metadata->getOperand(1))
                    ->getString().str();
                if (bfu2bf.find(bfuName) != bfu2bf.end()) {
                    LLVM_DEBUG(errs() << "Found another BFU with name: " << bfuName << "\n";);
                    (bfu2bf[bfuName])->insert(&*MI);
                    if (numIn > bfuNumInputs[bfuName]) bfuNumInputs[bfuName] = numIn;
                } else {
                    LLVM_DEBUG(errs() << "Found a new BFU with name: " << bfuName << "\n";);
                    bfu2bf[bfuName] = new std::set<Value*>();
                    bfu2bf[bfuName]->insert(&*MI);
                    bfuNumInputs[bfuName] = numIn;
                }
            }
        }
    }
    bfConflictMap = new int*[numBFs];
    bfConflictMap_tmp = new int*[numBFs];

    for (int i = 0; i < numBFs; i++) {
        bfConflictMap[i] = new int[numBFs];
        bfConflictMap_tmp[i] = new int[numBFs];
        for (int j = 0; j < numBFs; j++) {
            bfConflictMap[i][j] = -1;
            bfConflictMap_tmp[i][j] = -1;
        }
    }
}

void PrimateArchGen::generateInterconnect(int numALU, 
                                          raw_fd_stream &interconnectCFG) {
    std::map<std::string, int> bfuIdx;
    std::map<std::string, std::set<int>> bfuALUassigned;
    int id = -1;
    for (int i = 0; i < numBFs; i++) {
        blueFunctions[i]->print(errs());
        errs() << '\n';
    }
    for (int i = 0; i < numBFs; i++) {
        for (int j = 0; j < numBFs; j++) {
            errs() << bfConflictMap[i][j] << "\t";
        }
        errs() << '\n';
    }
    for (auto bfu = bfu2bf.begin(); bfu != bfu2bf.end(); bfu++) {
        int conflictCount = -1;
        int numInputs = bfuNumInputs[bfu->first];
        bfuIdx[bfu->first] = id++;
        std::set<int> conflictIdx;
        std::set<int> shareIdx;
        for (auto bf = bfu->second->begin(); bf != bfu->second->end(); bf++) {
            int idx = (*bfIdx)[*bf];
            for (int i = 0; i < numBFs-1; i++) {
                if (bfConflictMap[idx][i] > 0) {
                    conflictCount = 0;
                    shareIdx.insert(i);
                } else if (bfConflictMap[idx][i] == 0) {
                    conflictCount = 0;
                    conflictIdx.insert(i);
                }
            }
        }
        if (conflictCount == -1) {
            interconnectCFG << bfu->first << ": ";
            for (int i = 0; i < numInputs; i++) {
                bfuALUassigned[bfu->first].insert(i);
                interconnectCFG << "1 ";
            }
            for (int i = numInputs; i < numALU; i++) {
                interconnectCFG << "0 ";
            }
            interconnectCFG << "\n";
        } else {
            std::set<int> bfuConflictIdx;
            std::set<int> bfuShareIdx;
            for (auto idx_it = conflictIdx.begin(); idx_it != conflictIdx.end(); idx_it++) {
                int idx = *idx_it;
                Function* bf = dyn_cast<Function>(blueFunctions[idx]);
                MDNode *metadata = bf->getMetadata("primate");
                auto bfuName = cast<MDString>(metadata->getOperand(1))->getString().str();
                if (bfuALUassigned.find(bfuName) != bfuALUassigned.end()) {
                    // for (auto tmp_it = bfuALUassigned[bfuName].begin(); 
                    //      tmp_it != bfuALUassigned[bfuName].end(); tmp_it++) {
                    //     errs() << *tmp_it << " ";
                    // }
                    // errs() << "\n";
                    bfuConflictIdx.insert(bfuALUassigned[bfuName].begin(), 
                                          bfuALUassigned[bfuName].end());
                }
            }
            for (auto idx_it = shareIdx.begin(); idx_it != shareIdx.end(); idx_it++) {
                int idx = *idx_it;
                Function* bf = dyn_cast<Function>(blueFunctions[idx]);
                MDNode *metadata = bf->getMetadata("primate");
                auto bfuName = cast<MDString>(metadata->getOperand(1))->getString().str();
                if (bfuALUassigned.find(bfuName) != bfuALUassigned.end()) {
                    // for (auto tmp_it = bfuALUassigned[bfuName].begin(); 
                    //      tmp_it != bfuALUassigned[bfuName].end(); tmp_it++) {
                    //     errs() << *tmp_it << " ";
                    // }
                    // errs() << "\n";
                    bfuShareIdx.insert(bfuALUassigned[bfuName].begin(), 
                                       bfuALUassigned[bfuName].end());
                }
            }
            int numAssigned = 0;
            // Try to assign shared ALU first
            for (auto idx_it = bfuShareIdx.begin(); 
                 idx_it != bfuShareIdx.end(); idx_it++) {
                if (bfuConflictIdx.find(*idx_it) == bfuConflictIdx.end()) {
                    bfuALUassigned[bfu->first].insert(*idx_it);
                    numAssigned++;
                }
                if (numAssigned == numInputs) break;
            }
            if (numAssigned < numInputs) {
                for (int i = 0; i < numALU; i++) {
                    if ((bfuConflictIdx.find(i) == bfuConflictIdx.end()) && 
                        (bfuShareIdx.find(i) == bfuShareIdx.end())) {
                        bfuALUassigned[bfu->first].insert(i);
                        numAssigned++;
                    }
                    if (numAssigned == numInputs) break;
                }
            }
            if (numAssigned < numInputs) {
                errs() << bfu->first 
                       << ": Warning! Unable to assign conflict-free ALUs\n";
                interconnectCFG << bfu->first << ": ";
                bfuALUassigned[bfu->first].clear();
                for (int i = 0; i < numInputs; i++) {
                    bfuALUassigned[bfu->first].insert(i);
                    interconnectCFG << "1 ";
                }
                for (int i = numInputs; i < numALU; i++) {
                    interconnectCFG << "0 ";
                }
                interconnectCFG << "\n";
            } else {
                interconnectCFG << bfu->first << ": ";
                for (int i = 0; i < numALU; i++) {
                    if (bfuALUassigned[bfu->first].find(i) != 
                        bfuALUassigned[bfu->first].end()) {
                        interconnectCFG << "1 ";
                    } else {
                        interconnectCFG << "0 ";
                    }
                }
                interconnectCFG << "\n";
            }
        }
        // else {
        //     interconnectCFG << bfu->first << ": ";
        //     for (int i = 0; i < numALU; i++) {
        //         bfuALUassigned[bfu->first].insert(i);
        //         interconnectCFG << "1 ";
        //     }
        //     interconnectCFG << "\n";
        // }
    }
}

unsigned PrimateArchGen::getNumThreads(Module &M, unsigned numALU) {
    APInt maxVal(64, 0);
    for (auto FI = blueFunctions.begin(); FI != blueFunctions.end(); FI++) {
        Function* bf = dyn_cast<Function>(*FI);
        MDNode *metadata = bf->getMetadata("primate");
        auto *latency = cast<ConstantAsMetadata>(metadata->getOperand(2))->getValue();
        auto latency_val = cast<ConstantInt>(latency)->getValue();
        if (latency_val.ugt(maxVal)) {
            maxVal = latency_val;
        }
    }
    return (5 + (4 + numALU) + unsigned(maxVal.getZExtValue()));
}

void PrimateArchGen::InitializeBranchLevel(Function &F) {
    branchLevel = new ValueMap<Value*, int>();
    for (inst_iterator ii = inst_begin(F), 
                       e = inst_end(F); ii != e; ++ii) {
        (*branchLevel)[&*ii] = 0;
    }
}

void PrimateArchGen::InitializePointerMap(Function &F) {
    pointerMap = new ValueMap<Value*, ptrInfo_t*>();
    for (Function::arg_iterator arg = F.arg_begin(); arg != F.arg_end(); ++arg) {
        const Type* arg_type = arg->getType();
        if (arg_type->isPointerTy()) {
            (*pointerMap)[&*arg] = new ptrInfo_t(&*arg, 0);
        }
    }
    for (inst_iterator ii = inst_begin(F), 
                       e = inst_end(F); ii != e; ++ii) {
        if (isa<AllocaInst>(*ii)) {
            (*pointerMap)[&*ii] = new ptrInfo_t(&*ii, 0);
        } else if (isa<GetElementPtrInst>(*ii)) {
            auto *inst = dyn_cast<llvm::GetElementPtrInst>(&*ii);
            // inst->print(errs());
            // errs() << ":\n";
            unsigned offset = 0;
            Value *basePtr = inst->getPointerOperand();
            if (pointerMap->find(basePtr) == pointerMap->end()) {
                LLVM_DEBUG(dbgs() << "Found a pointer that was not from an alloca\n");
                (*pointerMap)[&*basePtr] = new ptrInfo_t(&*basePtr, 0);
            }
            if ((*pointerMap)[basePtr]->base != basePtr) {
                offset = (*pointerMap)[basePtr]->offset;
                basePtr = (*pointerMap)[basePtr]->base;
            }

            Value* curInst = inst->getPointerOperand();
            Type *type = nullptr; // boohoo
            while(true) {
                if(AllocaInst* allocaArg = dyn_cast<AllocaInst>(curInst)) {
                    type = allocaArg->getAllocatedType();
                    break;
                }
                else if(BitCastInst* bci = dyn_cast<BitCastInst>(curInst)) {
                    curInst = bci->getOperand(0);
                }
                else if(GetElementPtrInst* gepI = dyn_cast<GetElementPtrInst>(curInst)) {
                    curInst = gepI->getPointerOperand(); 
                }
                else if(Argument* curArg = dyn_cast<Argument>(curInst)) {
                    LLVM_DEBUG(dbgs() << "This is an argument\n"; curArg->dump(););
                    type = curArg->getType();
                    break;
                }
                else if(GlobalValue* gvVal = dyn_cast<GlobalValue>(curInst)) {
                    LLVM_DEBUG(dbgs() << "This is a globalvalue\n"; gvVal->dump(););
                    type = gvVal->getValueType();
                    break;
                }     
                else if(auto* loadI = dyn_cast<LoadInst>(curInst)) {
                    curInst = loadI->getPointerOperand();
                }
                else if(auto* storeI = dyn_cast<StoreInst>(curInst)) {
                    curInst = storeI->getPointerOperand();
                }           
                else {
                    curInst->dump();
                    llvm_unreachable("can't follow a pointer...");
                }
            }

            LLVM_DEBUG(
                dbgs() << "instr and type: ";
                inst->dump();
                type->dump();
            );

            if(type->isPointerTy()) {
                LLVM_DEBUG(dbgs() << "cannot find out the type for the pointer\n");
                // map an ambigous pointer
                (*pointerMap)[&*ii] = new ptrInfo_t(basePtr, -1, false);
                continue;
            }

            int i = 0;
            bool pointer_constant = true;
            for (auto idx = inst->idx_begin(); idx != inst->idx_end(); idx++) {
                if (auto *idx_const = dyn_cast<ConstantInt>(idx)) {
                    auto idx_val = idx_const->getValue();
                    int idx_u = int(idx_val.getSExtValue());
                    if (isa<StructType>(*type)) {
                        auto stype = dyn_cast<StructType>(type);
                        if (i == 0) {
                            unsigned elemWidth = getStructWidth(*stype, 0, false);
                            offset += idx_u * elemWidth;
                        } else {
                            int j = 0;
                            Type *tmp = nullptr;
                            for (auto elem = stype->element_begin(); 
                                 elem != stype->element_end() && j <= idx_u; 
                                 elem++, j++) {
                                if (j == idx_u) {
                                    tmp = (*elem);
                                    break;
                                }
                                offset += getTypeBitWidth(*elem);
                            }
                            type = tmp;
                        }
                        i = 1;
                    } else if (isa<ArrayType>(*type)) {
                        auto *atype = dyn_cast<llvm::ArrayType>(type);
                        if (i == 0) {
                            unsigned elemWidth = getArrayWidth(*atype, 0);
                            offset += idx_u * elemWidth;
                        } else {
                            Type *elem = atype->getElementType();
                            unsigned elemWidth = getTypeBitWidth(elem);
                            offset += (idx_u * elemWidth);
                            type = elem;
                        }
                        i = 1;
                    }
                    else if(isa<IntegerType>(*type)) {
                        auto *itype = dyn_cast<IntegerType>(type);
                        offset += itype->getBitWidth() * idx_u;
                    } 
                    else {
                        errs() << "Error: undefined type: inst: ";
                        inst->print(errs());
                        errs() << "\nidx: ";
                        idx->get()->dump();
                        errs() << "\n";
                        exit(1);
                    }
                } 
                else {
                    LLVM_DEBUG(errs() << "Error: pointer is not constant: ";
                    inst->print(errs());
                    errs() << "\n";);
                    pointer_constant = false;
                    break;
                }
            }
            (*pointerMap)[&*ii] = new ptrInfo_t(basePtr, offset, pointer_constant);
            // errs() << offset << "\n";
        } else if (isa<BitCastInst>(*ii)) {
            Value *srcOp = ii->getOperand(0);
            (*pointerMap)[&*ii] = (*pointerMap)[&*srcOp];
        }
    }
}

void PrimateArchGen::InitializeAliasMap(Function &F) {
    aliasMap = new ValueMap<Value*, Value*>();
    for (Function::arg_iterator arg = F.arg_begin(); arg != F.arg_end(); ++arg) {
        (*aliasMap)[&*arg] = &*arg;
    }
 
    for (inst_iterator ii = inst_begin(F), 
                       e = inst_end(F); ii != e; ++ii) {
        Value *srcOp;
        if (isa<GetElementPtrInst>(*ii)) {
            auto *inst = dyn_cast<llvm::GetElementPtrInst>(&*ii);
            if (isa<StructType>(*(inst->getSourceElementType()))) {
                auto *type = dyn_cast<StructType>(inst->getSourceElementType());
                if (!(type->isLiteral())) {
                    // getelementptr ii for a struct
                    srcOp = ii->getOperand(0);
                    (*aliasMap)[&*ii] = (*aliasMap)[&*srcOp];
                    // errs() << "getelementptr " << srcOp->getName() << "\n";
                } else {
                    (*aliasMap)[&*ii] = &*ii;
                }
            } else {
                (*aliasMap)[&*ii] = &*ii;
            }
        } else if (isa<LoadInst>(*ii)) {
            srcOp = ii->getOperand(0);
            if (unmergeableLoad.find(&*ii) == unmergeableLoad.end()) {
                (*aliasMap)[&*ii] = (*aliasMap)[&*srcOp];
            } else {
                (*aliasMap)[&*ii] = &*ii;
            }
            // errs() << "load " << srcOp->getName() << "\n";
        } else if (isa<StoreInst>(*ii)) {
            auto *inst = dyn_cast<llvm::StoreInst>(&*ii);
            srcOp = inst->getValueOperand();
            Value *ptrOp = inst->getPointerOperand();
            if (unmergeableStore.find(&*ii) == unmergeableStore.end()) {
                (*aliasMap)[srcOp] = (*aliasMap)[ptrOp];
                // errs() << "store" << ptrOp->getName() << "\n";
            }
        } else if (isa<BitCastInst>(*ii)) {
            srcOp = ii->getOperand(0);
            (*aliasMap)[&*ii] = (*aliasMap)[&*srcOp];
            // errs() << "bitcast " << srcOp->getName() << "\n";
        } else if (isa<ZExtInst>(*ii)) {
            srcOp = ii->getOperand(0);
            (*aliasMap)[&*ii] = (*aliasMap)[&*srcOp];
            // errs() << "zext " << srcOp->getName() << "\n";
        } else {
            (*aliasMap)[&*ii] = &*ii;
        }
    }
}

//evaluate each function
bool 
PrimateArchGen::evalFunc(Function &F, int &numALU, 
                         int &numInst, unsigned &maxConst) {
    domain.clear();
    bvIndexToInstrArg = new std::vector<Value*>();
    valueToBitVectorIndex = new ValueMap<Value*, int>();
    instrInSet = new ValueMap<const Instruction*, BitVector*>();

    InitializePointerMap(F);

    buildDependencyForest(F);

    // numALU = estimateNumALUs(F);
    numALU = 2;

    // printDependencyForest(F);
    numALUDSE(F, numALU, numInst, BALANCE);

    maxConst = getMaxConst(F);

    InitializeAliasMap(F);

    int index = 0;
    for (Function::arg_iterator arg = F.arg_begin(); arg != F.arg_end(); ++arg) {
        domain.push_back(&*arg);
        bvIndexToInstrArg->push_back(&*arg);
        (*valueToBitVectorIndex)[&*arg] = index;
        index++;
    }

    for (inst_iterator ii = inst_begin(F), 
                       e = inst_end(F); ii != e; ++ii) {
        domain.push_back(&*ii);
        bvIndexToInstrArg->push_back(&*ii);
        (*valueToBitVectorIndex)[&*ii] = index;
        index++;
    }

    domainSize = domain.size();

    //initialize the IN set set inside the block for each instruction.     
    for (inst_iterator ii = inst_begin(F), e = inst_end(F); ii != e; ++ii) {
        (*instrInSet)[&*ii] = new BitVector(domainSize, false); 
    }
    //call the backward analysis method in dataflow
    DataFlow<BitVector>::runOnFunction(F);
    // F.print(errs(), this);

    //compute the histogram
    for(inst_iterator ii = inst_begin(F), e = inst_end(F); ii != e; ++ii) {
        calculate(&*ii);
    }
    return false;
}

void 
PrimateArchGen::emitBasicBlockStartAnnot(const BasicBlock *bb, 
                                         formatted_raw_ostream &os) {
    os << "; ";
    if (!isa<PHINode>(*(bb))) {
        const BitVector *bv = (*in)[&*bb];
        for (int i=0; i < bv->size(); i++) {
            if ( (*bv)[i] ) {
                os << (*bvIndexToInstrArg)[i]->getName();
                os << ", ";
            }
        }
    }
    os << "\n";
}

void 
PrimateArchGen::emitInstructionAnnot(const Instruction *i, 
                                     formatted_raw_ostream &os) {
    os << "; ";
    if (!isa<PHINode>(*(i))) {
        const BitVector *bv = (*instrInSet)[&*i];
        /*                    
        live[bv->size()]++;
        if(bv->size() > n)
            n = bv->size();
        */                    
        for (int i=0; i < bv->size(); i++) {
            if ( (*bv)[i] ) {
                os << (*bvIndexToInstrArg)[i]->getName();
                os << ", ";
            }
        }
    }
    os << "\n";
}

PreservedAnalyses PrimateArchGen::run(Module &M, ModuleAnalysisManager& AM) {
    bvIndexToInstrArg = new std::vector<Value*>();
    valueToBitVectorIndex = new ValueMap<Value*, int>();
    instrInSet = new ValueMap<const Instruction*, BitVector*>();
    aliasMap = new ValueMap<Value*, Value*>();
    branchLevel = new ValueMap<Value*, int>();

    dependencyForest = new ValueMap<Value*, std::map<Value*, bool>*>();
    dependencyForestOp = new ValueMap<Value*, std::map<Value*, bool>*>();
    instPriority = new ValueMap<Value*, int>();
    bfIdx = new ValueMap<Value*, int>();

    std::fill_n(live,50,0);

    std::error_code primateEC, interconnEC, primateHeaderEC, asmHeaderEC;

    raw_fd_stream primateCFG("primate.cfg", primateEC);
    raw_fd_stream interconnectCFG("interconnect.cfg", interconnEC);
    raw_fd_stream primateHeader("header.scala", primateHeaderEC);
    raw_fd_stream assemblerHeader("primate_assembler.h", asmHeaderEC);
    // Check error codes

    assemblerHeader << "#include <iostream>\n#include <map>\n#include <string>\n\n";
    printRegfileKnobs(M, primateCFG);
    generate_header(M, primateHeader);

    const int MAX_ALU_POSSIBLE = 7;
    int maxNumALU = 0;
    int maxNumInst = 0;
    unsigned maxConst = 0;

    maxNumALU = maxNumALU > MAX_ALU_POSSIBLE ? MAX_ALU_POSSIBLE : maxNumALU;

    initializeBFCMeta(M);
    for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
        if (demangle(MI->getName()).find("primate_main") == std::string::npos) {
            LLVM_DEBUG(dbgs() << "non primate main. skipping eval\n");
            continue; 
        }
        int numALU = 0, numInst = 0;
        unsigned constVal;
        evalFunc(*MI, numALU, numInst, constVal);
        if (numALU > maxNumALU) maxNumALU = numALU;
        if (numInst > maxNumInst) maxNumInst = numInst;
        if (constVal > maxConst) maxConst = constVal;
        for (int i = 0; i < numBFs; i++) {
            for (int j = 0; j < numBFs; j++) {
                if (bfConflictMap_tmp[i][j] > bfConflictMap[i][j])
                    bfConflictMap[i][j] = bfConflictMap_tmp[i][j];
            }
        }
    }
    int numRegs = 0;
    for(unsigned int i=0;i<n;i++) {
        // errs()<<i<<" : "<<live[i]<<"\n";
        if(live[i] >= 0)
            numRegs = i;
    }
    unsigned maxLatency = getNumThreads(M, maxNumALU);

    primateCFG << "NUM_THREADS=" << int(pow(2, ceil(log2(maxLatency)))) << "\n";
    errs() << "Number of regs: " << numRegs << "\n";
    primateCFG << "NUM_REGS=" << int(pow(2, ceil(log2(numRegs)))) << "\n";
    assemblerHeader << "#define NUM_REGS " << int(pow(2, ceil(log2(numRegs)))) << "\n";
    assemblerHeader << "#define NUM_REGS_LG int(ceil(log2(NUM_REGS)))\n";

    if (bfu2bf.size() > maxNumALU)
        primateCFG << "NUM_ALUS=" << bfu2bf.size() << "\n";
    else
        primateCFG << "NUM_ALUS=" << maxNumALU << "\n";
    
    primateCFG << "NUM_BFUS=" << bfu2bf.size()<< "\n";
    assemblerHeader << "#define NUM_ALUS " << maxNumALU << "\n";
    assemblerHeader << "#define NUM_FUS " << maxNumALU + bfu2bf.size() - 1 << "\n";
    assemblerHeader << "#define NUM_FUS_LG int(ceil(log2(NUM_FUS)))\n";

    primateCFG << "IP_WIDTH=" << 32 << "\n"; // int(ceil(log2(maxNumInst))) << "\n";
    assemblerHeader << "#define IP_W " << int(ceil(log2(maxNumInst))) << "\n";
    errs() << "Number of instructions: " << maxNumInst << "\n";

    primateCFG << "IMM_WIDTH=" << int(ceil(log2(maxConst))) << "\n";
    assemblerHeader << "#define IMM_W " << int(ceil(log2(maxConst))) << "\n";

    generateInterconnect(maxNumALU, interconnectCFG);

    primateCFG.close();
    interconnectCFG.close();
    primateHeader.close();
    assemblerHeader.close();

    delete bvIndexToInstrArg;
    delete valueToBitVectorIndex;
    delete instrInSet;
    delete aliasMap;
    delete branchLevel;

    delete pointerMap;
    delete dependencyForest;
    delete dependencyForestOp;
    delete instPriority;
    delete bfIdx;
    
    cleanDataFlow();

    return PreservedAnalyses::all();
}

void PrimateArchGen::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
}

