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

#include<llvm/Pass.h>
#include<llvm/IR/DebugInfo.h>
#include<llvm/IR/Function.h>
#include<llvm/IR/Module.h>
#include<llvm/Support/raw_ostream.h>
#include<llvm/Support/FormattedStream.h>
#include<llvm/IR/InstIterator.h>
#include<llvm/IR/Instruction.h>
#include<llvm/IR/AssemblyAnnotationWriter.h>
#include<llvm/ADT/BitVector.h>
#include<llvm/IR/ValueMap.h>
#include<llvm/ADT/DenseMap.h>

#include<llvm/IR/LegacyPassManager.h>
#include<llvm/Transforms/IPO/PassManagerBuilder.h>

#include "dataflow.h"

#include <ostream>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <regex>

#define MAX_BR_LEVEL 2

using namespace llvm;

namespace {

    class PrimateArchGen : public ModulePass, public DataFlow<BitVector>, public AssemblyAnnotationWriter{

        public:
            static char ID;

            // set forward false in the constructor DataFlow()
            PrimateArchGen() : DataFlow<BitVector>(false), ModulePass(ID) {
                bvIndexToInstrArg = new std::vector<Value*>();
                valueToBitVectorIndex = new ValueMap<Value*, int>();
                instrInSet = new ValueMap<const Instruction*, BitVector*>();
                aliasMap = new ValueMap<Value*, Value*>();
                branchLevel = new ValueMap<Value*, int>();
            }

            // domain vector to store all definitions and function arguments
            std::vector<Value*> domain;
            std::vector<Value*> *bvIndexToInstrArg; 				//map values to their bitvector
            ValueMap<Value*, int> *valueToBitVectorIndex;      		//map values (args and variables) to their bit vector index
            ValueMap<const Instruction*, BitVector*> *instrInSet;     //IN set for an instruction inside basic block
            ValueMap<Value*, Value*> *aliasMap;
            ValueMap<Value*, int> *branchLevel;
            std::vector<unsigned> *gatherModes;
            std::map<unsigned, std::vector<unsigned>*> *fieldIndex;

            int domainSize;
            int numArgs;
            int numInstr;
            
            int live[50];
            unsigned int n = 0;

            //print functions
			//live variables before each basic block
            virtual void emitBasicBlockStartAnnot(const BasicBlock *bb, formatted_raw_ostream &os) {
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

			//live variables before each instruction: used for computing histogram
            virtual void emitInstructionAnnot(const Instruction *i, formatted_raw_ostream &os) {
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

            //implementation of functions

            //set the boundary condition for block
            //explicit constructor of BitVector
            virtual void setBoundaryCondition(BitVector *blockBoundary) {
                *blockBoundary = BitVector(domainSize, false); 
            }

            //union (bitwise OR) operator '|=' overriden in BitVector class
            virtual void meetOp(BitVector* lhs, const BitVector* rhs){
                *lhs |= *rhs; 
            }

            //empty set initially; each bit represent a value
            virtual BitVector* initializeFlowValue(BasicBlock& b, SetType setType){ 
                return new BitVector(domainSize, false); 
            }


            //transfer function:
            //IN[n] = USE[n] U (OUT[n] - DEF[n])
            
            virtual BitVector* transferFn(BasicBlock& bb) {
                BitVector* outNowIn = new BitVector(*((*out)[&bb]));
                                   
                BitVector* immIn = outNowIn; // for empty blocks
                Instruction* tempInst;
                bool breakme=false;
                // go through instructions in reverse
                BasicBlock::iterator ii = --(bb.end()), ib = bb.begin();
                while (true) {

                    // inherit data from next instruction
                    tempInst = &*ii;
                    immIn = (*instrInSet)[tempInst];            
                    *immIn = *outNowIn;

                    // if this instruction is a new definition, remove it
                    if (isDefinition(tempInst)){
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
                        if (isa<BranchInst>(*ii) || (((*branchLevel)[tempInst] > 0) && ((*branchLevel)[tempInst] < MAX_BR_LEVEL))) {
                            User::op_iterator OI, OE;
                            // tempInst->print(errs());
                            // errs() << "\n";
                            for (OI = tempInst->op_begin(), OE=tempInst->op_end(); OI != OE; ++OI) {
                                if (isa<Instruction>(*OI) || isa<Argument>(*OI)) {
                                    Value *op = (*aliasMap)[*OI];
                                    if (isa<Instruction>(op)) {
                                        Instruction *op_inst = dyn_cast<Instruction>(op);
                                        // combinable instruction must be in the same BB, must not be callInst or AllocaInst
                                        if ((op_inst->getParent()) != (tempInst->getParent()) || isa<CallInst>(op) || isa<AllocaInst>(op)) {
                                            (*immIn)[(*valueToBitVectorIndex)[op]] = true;
                                        // variable must not be live
                                        } else if (!(*immIn)[(*valueToBitVectorIndex)[op]]) {
                                            (*branchLevel)[op] = (*branchLevel)[tempInst] + 1;
                                        }
                                    } else if (isa<Argument>(op)) {
                                        (*immIn)[(*valueToBitVectorIndex)[op]] = true;
                                    }
                                }
                            }
                        // skip getElementPtrInst, bitCast, lifetime call instructions
                        } else if (!(isa<GetElementPtrInst>(*ii) || isa<BitCastInst>(*ii) || isLifetimeCall(&*ii))) {
                            User::op_iterator OI, OE;
                            for (OI = tempInst->op_begin(), OE=tempInst->op_end(); OI != OE; ++OI) {
                                if (isa<Instruction>(*OI) || isa<Argument>(*OI)) {
                                    Value *op = (*aliasMap)[*OI];
                                    (*immIn)[(*valueToBitVectorIndex)[op]] = true;
                                }
                            }
                        }
                    } else if(isa<PHINode>(*ii)) {
                        PHINode* phiNode = cast<PHINode>(&*ii);
                        for (int incomingIdx = 0; incomingIdx < phiNode->getNumIncomingValues(); incomingIdx++) {
                            Value* val = phiNode->getIncomingValue(incomingIdx);
                            if (isa<Instruction>(val) || isa<Argument>(val)) {
                                int valIdx = (*valueToBitVectorIndex)[(*aliasMap)[val]];
                                BasicBlock* incomingBlock = phiNode->getIncomingBlock(incomingIdx);
                                if ((*neighbourSpecificValues).find(incomingBlock) == (*neighbourSpecificValues).end())
                                    (*neighbourSpecificValues)[incomingBlock] = new BitVector(domainSize);
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

            bool isLifetimeCall(Instruction *ii) {
                bool res = false;
                if (isa<CallInst>(*ii)) {
                    auto *inst = dyn_cast<llvm::CallInst>(&*ii);
                    Function* foo = inst->getCalledFunction();
                    if (foo->getName().contains("lifetime")) {
                        res = true;
                    }
                }
                return res;
            }

            bool isDefinition(Instruction *ii) {
                return !(ii->isTerminator()) ;
            }

			void calculate(const Instruction *i) {
				//static unsigned int live[50], n=0;
				int count=0;
				
				if (!isa<PHINode>(*(i))) {
                    const BitVector *bv = (*instrInSet)[&*i];
                   	
                   	for(int i=0;i<bv->size();i++){
                   		if((*bv)[i])
                   			count++;
                   	}
                   	
                   	if(count > n) {
                   		n = count+1;
                   	}
                   	
                   	live[count]++;
                }

                // if (count > 6) {
                //     i->print(errs());
                //     errs() << ": ";
                //     const BitVector *bv = (*instrInSet)[&*i];
                //     for(int i=0;i<bv->size();i++){
                //         if((*bv)[i])
                //             errs() << (*bvIndexToInstrArg)[i]->getName() << " ";
                //     }
                //     errs() << "\n";
                // }

			}

            bool isBlueCall(Instruction *ii) {
                bool res = false;
                if (isa<llvm::CallInst>(&*ii)) {
                    MDNode *PrimateMetadata = ii->getMetadata("primate");
                    if (PrimateMetadata != NULL) {
                        if (cast<MDString>(PrimateMetadata->getOperand(0))->getString() == "blue") { // this is probably not safe to assume the type, you probably want to type check...
                            res = true;
                        }
                    }
                }
                return res;
            }

            unsigned getStructWidth(StructType &s, unsigned start) {
                unsigned width = start;
                for (auto elem = s.element_begin(); elem != s.element_end(); elem++) {
                    if (fieldIndex->find(width) == fieldIndex->end()) {
                        (*fieldIndex)[width] = new std::vector<unsigned>();
                    }
                    if (isa<llvm::IntegerType>(**elem)) {
                        unsigned elemWidth = (*elem)->getIntegerBitWidth();
                        // insert new gather mode if doesn't exist
                        if (std::find(gatherModes->begin(), gatherModes->end(), elemWidth) == gatherModes->end()) {
                            gatherModes->push_back(elemWidth);
                        }
                        // the width of all numbers possibly stored at each index
                        if (std::find((*fieldIndex)[width]->begin(), (*fieldIndex)[width]->end(), elemWidth) == (*fieldIndex)[width]->end()) {
                            (*fieldIndex)[width]->push_back(elemWidth);
                        }
                        width += elemWidth;
                    } else if (isa<llvm::StructType>(**elem)) {
                        auto *selem = dyn_cast<llvm::StructType>(*elem);
                        unsigned elemWidth = getStructWidth((*selem), width);
                        width = elemWidth;
                    }
                }
                if (fieldIndex->find(width) == fieldIndex->end()) {
                    (*fieldIndex)[width] = new std::vector<unsigned>();
                }
                return width;
            }

            void printRegfileKnobs(Module &M) {
                auto structTypes = M.getIdentifiedStructTypes();
                unsigned maxRegWidth = 0;
                gatherModes = new std::vector<unsigned>();
                fieldIndex = new std::map<unsigned, std::vector<unsigned>*>();
                for (auto it = structTypes.begin(); it != structTypes.end(); it++) {
                    unsigned regWidth = getStructWidth((**it), 0);
                    if (regWidth > maxRegWidth) {
                        maxRegWidth = regWidth;
                    }
                }
                errs() << "Max regfile width: " << maxRegWidth << "\n";

                errs() << "reg block:";
                auto lastIt = fieldIndex->begin();
                for (auto it = (++fieldIndex->begin()); it != fieldIndex->end(); ++it) {
                    errs() << " " << (it->first) - (lastIt->first);
                    lastIt = it;
                }
                errs() << "\n";
                // for (int i = 1; i < regBlockWidth->size(); i++) {
                //     errs() << " " << (*regBlockWidth)[i] - (*regBlockWidth)[i-1];
                // }
                // errs() << "\n";
                errs() << "Gather/Scatter Index:";
                std::map<unsigned, unsigned> indexEncode;
                int i = 0;
                for (auto it = fieldIndex->begin(); it != fieldIndex->end(); ++it) {
                    errs() << " " << (it->first);
                    indexEncode[it->first] = i;
                    i++;
                }
                errs() << "\n";

                errs() << "Gather/Scatter mode Encode:";
                std::sort(gatherModes->begin(), gatherModes->end());
                std::map<unsigned, unsigned> gatherEncode;
                for (int i = 0; i < gatherModes->size(); i++) {
                    gatherEncode[(*gatherModes)[i]] = i;
                    errs() << " " << (*gatherModes)[i];
                }
                gatherEncode[maxRegWidth] = gatherModes->size();
                errs() << " " << maxRegWidth << "\n";

                errs() << "Scatter wben Encode:";
                std::vector<unsigned> scatterWbens;
                int numBlocks = fieldIndex->size();
                errs() << "(";
                i = 0;
                for (auto it = fieldIndex->begin(); it != fieldIndex->end(); it++) {
                    if (it->second->size() > 1) {
                        std::sort(it->second->begin(), it->second->end());
                    }
                    for (int j = 0; j < it->second->size(); j++) {
                        errs() << "(" << i << ", " << gatherEncode[(*(it->second))[j]] << "), ";
                        int blocks = indexEncode[(it->first) + (*(it->second))[j]] - i;
                        scatterWbens.push_back(((1 << blocks) - 1) << i);
                    }
                    if (i == 0) {
                        errs() << "(0, " << gatherEncode[maxRegWidth] << "), ";
                        scatterWbens.push_back((1 << numBlocks) - 1);
                    }
                    i++;
                }
                errs() << ")\n";

                errs() << "Scatter wbens:";
                for (i = 0; i < scatterWbens.size(); i++) {
                    errs() << " " << format_hex(scatterWbens[i], 8) << ",";
                }
                errs() << "\n";
            }

            unsigned getMaxConst(Function &F) {
                APInt maxVal(32, 0);
                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    Instruction *tempInst = &*instruction;
                    User::op_iterator OI;
                    if (!(isa<AllocaInst>(*tempInst) || isa<GetElementPtrInst>(*tempInst) || isa<BitCastInst>(*tempInst) || 
                        isa<ZExtInst>(*tempInst) || isa<BranchInst>(*tempInst) || isa<CallInst>(*tempInst))) {
                        for (OI = tempInst->op_begin(); OI != tempInst->op_end(); ++OI) {
                            if (isa<ConstantInt>(*OI)) {
                                auto* constOp = dyn_cast<llvm::ConstantInt>(&*OI);
                                unsigned bitWidth = constOp->getBitWidth();
                                auto constVal = constOp->getValue();
                                APInt val = constVal;
                                // errs() << constVal << ": ";
                                // tempInst->print(errs());
                                // errs() << "\n";
                                if (bitWidth > 32) {
                                    val = constVal.truncSSat(32);
                                } else if (bitWidth < 32) {
                                    val = constVal.sext(32);
                                }
                                if (val.abs().ugt(maxVal)) {
                                    maxVal = val;
                                }
                            }
                        }
                    }
                }
                // errs() << "Max immediate: " << maxVal << "\n";
                return unsigned(maxVal.getZExtValue());
            }

            virtual void getNumALU(Function &F, int &maxNumALU, int &numInst) {
                std::vector<Value*> notComputedVal;
                std::vector<Value*> unStoredPtr;
                std::list<Instruction*> *instList = new std::list<Instruction*>();
                maxNumALU = 0;
                numInst = 0;
                bool newBB = true;

                for (Function::iterator bi = F.begin(), be = F.end(); bi != be; bi++) {
                    BasicBlock *bb = &*bi;
                    notComputedVal.clear();
                    unStoredPtr.clear();
                    newBB = true;
                    for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii != ie; ii++) {
                        Instruction *tempInst = &*ii;
                        if (isBlueCall(tempInst) || tempInst->isTerminator()) {
                            // blue function call or end of BB
                            int level = 0;
                            int numALU = 0;
                            int aluCount = 0;
                            instList->push_back(NULL);
                            while (!instList->empty()) {
                                Instruction *tmp = instList->front();
                                instList->pop_front();
                                if (tmp == NULL) {
                                    level++;
                                    if ((aluCount != 0) || (newBB && isBlueCall(tempInst))) {
                                        numInst++;
                                    } else {
                                        // tempInst->print(errs());
                                        // errs() << "\n";
                                    }
                                    if (aluCount > maxNumALU) {
                                        // errs() << "new aluCount: " << aluCount << "\n";
                                        // tempInst->print(errs());
                                        // errs() << "\n";
                                        maxNumALU = aluCount;
                                    }
                                    newBB = false;
                                    aluCount = 0;
                                    notComputedVal.clear();
                                    unStoredPtr.clear();
                                    if (!instList->empty()) {
                                        instList->push_back(NULL);
                                    }
                                } else if (isa<StoreInst>(*tmp)) {
                                    auto *tmp2 = dyn_cast<llvm::StoreInst>(&*tmp);
                                    Value *srcOp = tmp2->getValueOperand();
                                    Value *ptrOp = tmp2->getPointerOperand();
                                    unStoredPtr.push_back(ptrOp);
                                    if (std::find(notComputedVal.begin(), notComputedVal.end(), srcOp) == notComputedVal.end()) {
                                        // no dependency
                                        if ((*aliasMap)[srcOp] != (*aliasMap)[ptrOp]) {
                                            aluCount ++;
                                        }
                                    } else {
                                        // dependency found
                                        instList->push_back(tmp);
                                    }
                                } else if (isa<CallInst>(*tmp)) {
                                    // memcpy
                                    Value *dstPtr = tmp->getOperand(0);
                                    Value *srcPtr = tmp->getOperand(1);
                                    unStoredPtr.push_back(dstPtr);
                                    bool isDependent = false;
                                    for (auto it = unStoredPtr.begin(); it != unStoredPtr.end(); it++) {
                                        if ((*aliasMap)[*it] == (*aliasMap)[srcPtr]) {
                                            isDependent = true;
                                            break;
                                        }
                                    }
                                    if (isDependent) {
                                        instList->push_back(tmp);
                                    } else {
                                        aluCount++;
                                    }
                                } else if (isa<LoadInst>(*tmp)) {
                                    Value* srcPtr = tmp->getOperand(0);
                                    if ((std::find(unStoredPtr.begin(), unStoredPtr.end(), srcPtr) != unStoredPtr.end()) || 
                                            (std::find(unStoredPtr.begin(), unStoredPtr.end(), (*aliasMap)[srcPtr]) != unStoredPtr.end())) {
                                        // dependency found
                                        instList->push_back(tmp);
                                        notComputedVal.push_back(tmp);
                                    }
                                } else if (isa<ZExtInst>(*tmp)) {
                                    Value* srcOp = tmp->getOperand(0);
                                    if (std::find(notComputedVal.begin(), notComputedVal.end(), srcOp) != notComputedVal.end()) {
                                        // dependency found
                                        instList->push_back(tmp);
                                        notComputedVal.push_back(tmp);
                                    }
                                } else {
                                    notComputedVal.push_back(tmp);
                                    bool isDependent = false;
                                    for (auto OI = tmp->op_begin(); OI != tmp->op_end(); ++OI) {
                                        if (std::find(notComputedVal.begin(), notComputedVal.end(), (*OI)) != notComputedVal.end()) {
                                            // dependency found
                                            isDependent = true;
                                            break;
                                        }
                                    }
                                    if (isDependent) {
                                        instList->push_back(tmp);
                                    } else {
                                        aluCount++;
                                    }
                                }
                            }
                        } else if (isa<CallInst>(*tempInst)) {
                            auto *inst = dyn_cast<llvm::CallInst>(&*tempInst);
                            Function* foo = inst->getCalledFunction();
                            if (foo->getName().contains("memcpy")) {
                                instList->push_back(tempInst);
                            }
                        } else if (!(isa<GetElementPtrInst>(*tempInst) || isa<BitCastInst>(*tempInst) || isa<AllocaInst>(*tempInst) || isa<PHINode>(*tempInst))) {
                            instList->push_back(tempInst);
                        }
                    }
                }
                // errs() << "num ALU: " << maxNumALU << "\n";
                // errs() << "num Instructions: " << numInst << "\n";
            }

            virtual void InitializeBranchLevel(Function &F) {
                branchLevel = new ValueMap<Value*, int>();
                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    (*branchLevel)[&*instruction] = 0;
                }
            }

            virtual void InitializeAliasMap(Function &F) {
                aliasMap = new ValueMap<Value*, Value*>();
                for (Function::arg_iterator arg = F.arg_begin(); arg != F.arg_end(); ++arg){
                    (*aliasMap)[&*arg] = &*arg;
                }

                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    Value *srcOp;
                    if (isa<GetElementPtrInst>(*instruction)) {
                        auto *inst = dyn_cast<llvm::GetElementPtrInst>(&*instruction);
                        if (isa<StructType>(*(inst->getSourceElementType()))) {
                            auto *type = dyn_cast<StructType>(inst->getSourceElementType());
                            if (!(type->isLiteral())) {
                                // getelementptr instruction for a struct
                                srcOp = instruction->getOperand(0);
                                (*aliasMap)[&*instruction] = (*aliasMap)[&*srcOp];
                                // errs() << "getelementptr " << srcOp->getName() << "\n";
                            } else {
                                (*aliasMap)[&*instruction] = &*instruction;
                            }
                        } else {
                            (*aliasMap)[&*instruction] = &*instruction;
                        }
                    } else if (isa<LoadInst>(*instruction)) {
                        srcOp = instruction->getOperand(0);
                        (*aliasMap)[&*instruction] = (*aliasMap)[&*srcOp];
                        // errs() << "load " << srcOp->getName() << "\n";
                    } else if (isa<StoreInst>(*instruction)) {
                        auto *inst = dyn_cast<llvm::StoreInst>(&*instruction);
                        srcOp = inst->getValueOperand();
                        Value *ptrOp = inst->getPointerOperand();
                        if ((*aliasMap)[srcOp] == srcOp) {
                            // no existing alias
                            (*aliasMap)[srcOp] = (*aliasMap)[ptrOp];
                            // errs() << "store" << ptrOp->getName() << "\n";
                        }
                    } else if (isa<BitCastInst>(*instruction)) {
                        srcOp = instruction->getOperand(0);
                        (*aliasMap)[&*instruction] = (*aliasMap)[&*srcOp];
                        // errs() << "bitcast " << srcOp->getName() << "\n";
                    } else if (isa<ZExtInst>(*instruction)) {
                        srcOp = instruction->getOperand(0);
                        (*aliasMap)[&*instruction] = (*aliasMap)[&*srcOp];
                        // errs() << "zext " << srcOp->getName() << "\n";
                    } else {
                        (*aliasMap)[&*instruction] = &*instruction;
                    }
                }
            }
            
            //evaluate each function
            virtual bool evalFunc(Function &F, int &numALU, int &numInst, unsigned &maxConst) {
                domain.clear();
                bvIndexToInstrArg = new std::vector<Value*>();
                valueToBitVectorIndex = new ValueMap<Value*, int>();
                instrInSet = new ValueMap<const Instruction*, BitVector*>();

                InitializeAliasMap(F);

                getNumALU(F, numALU, numInst);

                maxConst = getMaxConst(F);

                int index = 0;
                for (Function::arg_iterator arg = F.arg_begin(); arg != F.arg_end(); ++arg){
                    domain.push_back(&*arg);
                    bvIndexToInstrArg->push_back(&*arg);
                    (*valueToBitVectorIndex)[&*arg] = index;
                    index++;
                }

                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    domain.push_back(&*instruction);
                    bvIndexToInstrArg->push_back(&*instruction);
                    (*valueToBitVectorIndex)[&*instruction] = index;
                    index++;
                }

                domainSize = domain.size();

                //initialize the IN set set inside the block for each instruction.     
                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    (*instrInSet)[&*instruction] = new BitVector(domainSize, false); 
                }
				//call the backward analysis method in dataflow
                DataFlow<BitVector>::runOnFunction(F);
                // F.print(errs(), this);
                
                //compute the histogram
                for(inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                	calculate(&*instruction);
                }
                

                return false;
            }
            
            virtual bool runOnModule(Module &M){
            	std::fill_n(live,50,0);

                printRegfileKnobs(M);

                int maxNumALU = 0;
                int maxNumInst = 0;
                unsigned maxConst = 0;
            
                for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
                {
                    int numALU, numInst;
                    unsigned constVal;
                    evalFunc(*MI, numALU, numInst, constVal);
                    if (numALU > maxNumALU) maxNumALU = numALU;
                    if (numInst > maxNumInst) maxNumInst = numInst;
                    if (constVal > maxConst) maxConst = constVal;
                }
                int numRegs = 0;
                for(unsigned int i=0;i<n;i++){
                	if(live[i] >= 0)
						// errs()<<i<<" : "<<live[i]<<"\n";
                        numRegs = i;
				}
                errs() << "Num Registers required: " << numRegs << "\n";
                errs() << "Num ALUs required: " << maxNumALU << "\n";
                errs() << "Num Inst estimation: " << maxNumInst << "\n";
                errs() << "Maximum immediate: " << maxConst << "\n";
                return false;
            }

            virtual void getAnalysisUsage(AnalysisUsage &AU) const {
                AU.setPreservesAll();
            }

    };

    char PrimateArchGen::ID = 0;

    static RegisterPass<PrimateArchGen> X("primate", "primate pass");

    static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new PrimateArchGen()); });

}

