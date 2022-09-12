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
#include <math.h>

#define MAX_BR_LEVEL 2
#define MAX_PERF 0
#define BALANCE 1

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

            struct ptrInfo_t {
                Value *base;
                int offset;
                ptrInfo_t(Value *base = NULL, int offset = 0) : base(base), offset(offset) {}
            };

            // domain vector to store all definitions and function arguments
            std::vector<Value*> domain;
            std::vector<Value*> *bvIndexToInstrArg; 				//map values to their bitvector
            ValueMap<Value*, int> *valueToBitVectorIndex;      		//map values (args and variables) to their bit vector index
            ValueMap<const Instruction*, BitVector*> *instrInSet;     //IN set for an instruction inside basic block
            ValueMap<Value*, Value*> *aliasMap;
            ValueMap<Value*, int> *branchLevel;
            std::vector<unsigned> *gatherModes;
            std::map<unsigned, std::vector<unsigned>*> *fieldIndex;
            std::set<std::string> *blueFunction;
            ValueMap<Value*, ptrInfo_t*> *pointerMap;
            ValueMap<Value*, std::map<Value*, bool>*> dependencyForest;
            ValueMap<Value*, std::map<Value*, bool>*> dependencyForestOp;
            ValueMap<Value*, int> instPriority;
            std::set<Value*> loadMergedInst;
            std::set<Value*> unmergeableLoad;
            std::set<Value*> unmergeableStore;
            std::set<Value*> combinedBranchInst;
            std::map<BasicBlock*, std::set<Value*>*> frontiers;
            std::map<BasicBlock*, double> bbWeight;
            std::map<BasicBlock*, int> bbNumInst;
            std::map<BasicBlock*, int> bbNumVLIWInst;

            int domainSize;
            int numArgs;
            int numInstr;
            
            int live[50];
            unsigned int n = 0;

            std::ofstream primateCFG;
            std::ofstream primateHeader;
            std::ofstream assemblerHeader;

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
                        if (combinedBranchInst.find(&*ii) == combinedBranchInst.end()) {
                            // skip getElementPtrInst, bitCast, lifetime call instructions
                            if (!(isa<GetElementPtrInst>(*ii) || isa<BitCastInst>(*ii) || isLifetimeCall(&*ii))) {
                                User::op_iterator OI, OE;
                                for (OI = tempInst->op_begin(), OE=tempInst->op_end(); OI != OE; ++OI) {
                                    if (isa<Instruction>(*OI) || isa<Argument>(*OI)) {
                                        Value *op = (*aliasMap)[*OI];
                                        (*immIn)[(*valueToBitVectorIndex)[op]] = true;
                                    }
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

            unsigned getArrayWidth(ArrayType &a, unsigned start) {
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

            unsigned getArrayWidthArcGen(ArrayType &a, unsigned start) {
                unsigned width = start;
                auto elem = a.getElementType();
                unsigned elemWidth = 0;
                if (isa<llvm::IntegerType>(*elem)) {
                    elemWidth = elem->getIntegerBitWidth();
                    // insert new gather mode if doesn't exist
                    if (std::find(gatherModes->begin(), gatherModes->end(), elemWidth) == gatherModes->end()) {
                        gatherModes->push_back(elemWidth);
                    }
                }
                for (int i = 0; i < a.getNumElements(); i++) {
                    if (fieldIndex->find(width) == fieldIndex->end()) {
                        (*fieldIndex)[width] = new std::vector<unsigned>();
                    }
                    if (isa<llvm::IntegerType>(*elem)) {
                        // the width of all numbers possibly stored at each index
                        if (std::find((*fieldIndex)[width]->begin(), (*fieldIndex)[width]->end(), elemWidth) == (*fieldIndex)[width]->end()) {
                            (*fieldIndex)[width]->push_back(elemWidth);
                        }
                        width += elemWidth;
                    } else if (isa<llvm::ArrayType>(*elem)) {
                        auto *selem = dyn_cast<llvm::ArrayType>(elem);
                        unsigned elemWidth = getArrayWidthArcGen((*selem), width);
                        width = elemWidth;
                    } else if (isa<llvm::StructType>(*elem)) {
                        auto *selem = dyn_cast<llvm::StructType>(elem);
                        unsigned elemWidth = getStructWidth((*selem), width, true);
                        width = elemWidth;
                    }
                }
                return width;
            }

            unsigned getStructWidth(StructType &s, unsigned start, const bool arcGen) {
                unsigned width = start;
                for (auto elem = s.element_begin(); elem != s.element_end(); elem++) {
                    if (arcGen) {
                        if (fieldIndex->find(width) == fieldIndex->end()) {
                            (*fieldIndex)[width] = new std::vector<unsigned>();
                        }
                    }
                    if (isa<llvm::IntegerType>(**elem)) {
                        unsigned elemWidth = (*elem)->getIntegerBitWidth();
                        if (arcGen) {
                            // insert new gather mode if doesn't exist
                            if (std::find(gatherModes->begin(), gatherModes->end(), elemWidth) == gatherModes->end()) {
                                gatherModes->push_back(elemWidth);
                            }
                            // the width of all numbers possibly stored at each index
                            if (std::find((*fieldIndex)[width]->begin(), (*fieldIndex)[width]->end(), elemWidth) == (*fieldIndex)[width]->end()) {
                                (*fieldIndex)[width]->push_back(elemWidth);
                            }
                        }
                        width += elemWidth;
                    } else if (isa<llvm::ArrayType>(**elem)) {
                        auto *selem = dyn_cast<llvm::ArrayType>(*elem);
                        unsigned elemWidth;
                        if (arcGen) elemWidth = getArrayWidthArcGen((*selem), width);
                        else elemWidth = getArrayWidth((*selem), width);
                        width = elemWidth;
                    } else if (isa<llvm::StructType>(**elem)) {
                        auto *selem = dyn_cast<llvm::StructType>(*elem);
                        unsigned elemWidth = getStructWidth((*selem), width, arcGen);
                        width = elemWidth;
                    }
                }
                if (arcGen) {
                    if (fieldIndex->find(width) == fieldIndex->end()) {
                        (*fieldIndex)[width] = new std::vector<unsigned>();
                    }
                }
                return width;
            }

            unsigned getTypeBitWidth(Type *ty) {
                unsigned size;
                if (ty->isIntegerTy()) {
                    size = ty->getIntegerBitWidth();
                } else if (ty->isStructTy()) {
                    StructType *sty = dyn_cast<StructType>(ty);
                    size = getStructWidth(*sty, 0, false);
                } else if (ty->isArrayTy()) {
                    ArrayType *aty = dyn_cast<ArrayType>(ty);
                    size = getArrayWidth(*aty, 0);
                }
                return size;
            }

            void printRegfileKnobs(Module &M) {
                auto structTypes = M.getIdentifiedStructTypes();
                unsigned maxRegWidth = 0;
                gatherModes = new std::vector<unsigned>();
                fieldIndex = new std::map<unsigned, std::vector<unsigned>*>();
                for (auto it = structTypes.begin(); it != structTypes.end(); it++) {
                    unsigned regWidth = getStructWidth((**it), 0, true);
                    if (regWidth > maxRegWidth) {
                        maxRegWidth = regWidth;
                    }
                }
                // errs() << "Max regfile width: " << maxRegWidth << "\n";
                primateCFG << "REG_WIDTH=" << maxRegWidth << "\n";

                // errs() << "reg block:";
                primateCFG << "REG_BLOCK_WIDTH=";
                auto lastIt = fieldIndex->begin();
                for (auto it = (++fieldIndex->begin()); it != fieldIndex->end(); ++it) {
                    primateCFG << (it->first) - (lastIt->first) << " ";
                    lastIt = it;
                }
                primateCFG << "\n";

                primateCFG << "NUM_REGBLOCKS=" << fieldIndex->size() - 1 << "\n";
                // for (int i = 1; i < regBlockWidth->size(); i++) {
                //     errs() << " " << (*regBlockWidth)[i] - (*regBlockWidth)[i-1];
                // }
                // errs() << "\n";
                primateCFG << "SRC_POS=";
                std::map<unsigned, unsigned> indexEncode;
                int i = 0;
                auto it = fieldIndex->begin();
                for (; it != std::prev(fieldIndex->end()); ++it) {
                    primateCFG << (it->first) << " ";
                    indexEncode[it->first] = i;
                    i++;
                }
                indexEncode[it->first] = i;
                primateCFG << "\n";

                primateCFG << "SRC_MODE=";
                assemblerHeader << "static std::map<std::string, int> srcType_dict {\n";
                std::sort(gatherModes->begin(), gatherModes->end());
                std::map<unsigned, unsigned> gatherEncode;
                int last_mode;
                for (int i = 0; i < gatherModes->size(); i++) {
                    gatherEncode[(*gatherModes)[i]] = i;
                    last_mode = (*gatherModes)[i];
                    primateCFG << (*gatherModes)[i] << " ";
                    assemblerHeader << "    {\"uint" << (*gatherModes)[i] << "\", " << i << "},\n";
                }
                gatherEncode[maxRegWidth] = gatherModes->size();
                primateCFG << last_mode << " " << "\n";
                assemblerHeader << "    {\"uint\", " << gatherModes->size() << "},\n";
                assemblerHeader << "    {\"uimm\", " << gatherModes->size()+1 << "}\n};\n";

                primateCFG << "MAX_FIELD_WIDTH=" << (*gatherModes).back() << "\n";

                primateCFG << "NUM_SRC_POS=" << fieldIndex->size()-1 << "\n";
                assemblerHeader << "#define NUM_SRC_POS " << fieldIndex->size()-1 << "\n";
                assemblerHeader << "#define NUM_SRC_POS_LG int(ceil(log2(NUM_SRC_POS)))\n";

                primateCFG << "NUM_SRC_MODES=" << gatherModes->size()+1 << "\n";
                assemblerHeader << "#define NUM_SRC_MODE " << gatherModes->size()+1 << "\n";
                assemblerHeader << "#define NUM_SRC_MODE_LG int(ceil(log2(NUM_SRC_MODE)))\n";

                primateCFG << "DST_POS=";
                for (auto it = fieldIndex->begin(); it != std::prev(fieldIndex->end()); ++it) {
                    primateCFG << (it->first) << " ";
                }
                primateCFG << "\n";

                primateCFG << "DST_ENCODE=";
                for (i = 0; i < fieldIndex->size() - 1; i++) {
                    primateCFG << i << " ";
                }
                primateCFG << "\n";

                primateCFG << "DST_EN_ENCODE=";
                std::vector<unsigned> scatterWbens;
                int numBlocks = fieldIndex->size();
                i = 0;
                for (auto it = fieldIndex->begin(); it != fieldIndex->end(); it++) {
                    if (it->second->size() > 1) {
                        std::sort(it->second->begin(), it->second->end());
                    }
                    for (int j = 0; j < it->second->size(); j++) {
                        primateCFG << i << " " << gatherEncode[(*(it->second))[j]] << ";";
                        errs() << (it->first) + (*(it->second))[j] << "\n";
                        int blocks = indexEncode[(it->first) + (*(it->second))[j]] - i;
                        scatterWbens.push_back(((1 << blocks) - 1) << i);
                    }
                    if (i == 0) {
                        // primateCFG << "0 " << gatherEncode[maxRegWidth] << ";";
                        primateCFG << "0 -1;";
                        scatterWbens.push_back((1 << numBlocks) - 1);
                    }
                    i++;
                }
                primateCFG << "\n";

                primateCFG << "DST_EN=";
                for (i = 0; i < scatterWbens.size(); i++) {
                    // errs() << " " << format_hex(scatterWbens[i], 8) << ",";
                    primateCFG << scatterWbens[i] << " ";
                }
                primateCFG << "\n";

                primateCFG << "NUM_DST_POS=" << fieldIndex->size()-1 << "\n";

                primateCFG << "NUM_DST_MODE=" << gatherModes->size()+1 << "\n";
            }

            void generate_header(Module &M) {
                auto structTypes = M.getIdentifiedStructTypes();
                unsigned maxRegWidth = 0;
                gatherModes = new std::vector<unsigned>();
                fieldIndex = new std::map<unsigned, std::vector<unsigned>*>();
                primateHeader << "import chisel3._\nimport chisel3.util._\n\n";
                for (auto it = structTypes.begin(); it != structTypes.end(); it++) {
                    if ((*it)->getName().contains("input_t")) {
                        unsigned elemWidth = getStructWidth((**it), 0, false);
                        primateHeader << "class input_t extends Bundle {\n";
                        primateHeader << "    val data = UInt(" << elemWidth << ".W)\n";
                        primateHeader << "}\n";
                    } else if ((*it)->getName().contains("output_t")) {
                        unsigned elemWidth = getStructWidth((**it), 0, false);
                        primateHeader << "class output_t extends Bundle {\n";
                        primateHeader << "    val data = UInt(" << elemWidth << ".W)\n";
                        primateHeader << "}\n";
                    }
                }
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

            std::vector<Value*>* getBFCOutputs(Instruction *ii) {
                if (isa<llvm::CallInst>(&*ii)) {
                    MDNode *PrimateMetadata = ii->getMetadata("primate");
                    if (PrimateMetadata != NULL) {
                        if (cast<MDString>(PrimateMetadata->getOperand(0))->getString() == "blue") {
                            auto numIn_i = cast<ConstantInt>(cast<ConstantAsMetadata>(PrimateMetadata->getOperand(3))->getValue())->getValue();
                            unsigned numIn = unsigned(numIn_i.getZExtValue());
                            if (numIn < (ii->getNumOperands()-1)) { // last operand is always metadata
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

            std::vector<Value*>* getBFCInputs(Instruction *ii) {
                if (isa<llvm::CallInst>(&*ii)) {
                    MDNode *PrimateMetadata = ii->getMetadata("primate");
                    if (PrimateMetadata != NULL) {
                        if (cast<MDString>(PrimateMetadata->getOperand(0))->getString() == "blue") {
                            auto numIn_i = cast<ConstantInt>(cast<ConstantAsMetadata>(PrimateMetadata->getOperand(3))->getValue())->getValue();
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

            bool checkMemAlias(Value *ptr0, unsigned size0, Value *ptr1, unsigned size1) {
                if (pointerMap->find(ptr0) == pointerMap->end()) {
                    errs() << "pointer0 not initialized\n";
                    ptr0->print(errs());
                    errs() << "\n";
                    exit(1);
                }
                if (pointerMap->find(ptr1) == pointerMap->end()) {
                    errs() << "pointer1 not initialized\n";
                    ptr1->print(errs());
                    errs() << "\n";
                    exit(1);
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

            bool isReachable(Value* src, std::set<Value*> &dst) {
                // simple DFS
                std::vector<Value*> stack;
                for (auto it = dependencyForest[src]->begin(); it != dependencyForest[src]->end(); it++) {
                    stack.push_back(it->first);
                }
                while (!stack.empty()) {
                    Value *inst = stack.back();
                    if (dst.find(inst) != dst.end()) {
                        return true;
                    } else {
                        stack.pop_back();
                        for (auto it = dependencyForest[inst]->begin(); it != dependencyForest[inst]->end(); it++) {
                            stack.push_back(it->first);
                        }
                    }
                }
                return false;
            }

            inline void memInstAddRAWDep(Instruction* inst, Value* srcPtr, unsigned size, ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> &storeInsts) {
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
                        for (auto dep = dependencyForest[&*inst]->begin(); dep != dependencyForest[&*inst]->end(); dep++) {
                            if (dep->second && (isReachable(dep->first, dst))) {
                                immDep = false;
                                break;
                            }
                        }
                        if (immDep) (*dependencyForest[&*inst])[si->first] = true;
                    }
                }
            }

            inline void memInstAddWARDep(Instruction* inst, Value* dstPtr, unsigned size, ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> &loadInsts) {
                for (auto li = loadInsts.begin(); li != loadInsts.end(); li++) {
                    // check all instructions that read memory
                    // li->first->print(errs());
                    // errs() << ":\n";
                    for (auto lp = li->second.begin(); lp != li->second.end(); lp++) {
                        if (checkMemAlias(dstPtr, size, lp->first, lp->second)) {
                            // WAR dependency
                            dependencyForest[&*inst]->insert({li->first, false});
                        }
                    }
                }
            }

            void initializeDependencyForest(Function &F) {
                ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> loadInsts;
                ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> storeInsts;
                dependencyForest.clear();
                loadMergedInst.clear();
                for (Function::iterator bi = F.begin(), be = F.end(); bi != be; bi++) {
                    BasicBlock *bb = &*bi;
                    loadInsts.clear();
                    storeInsts.clear();
                    for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii != ie; ii++) {
                        Instruction *inst = &*ii;
                        // inst->print(errs());
                        // errs() << ":\n";
                        if (isa<LoadInst>(*inst)) {
                            // inst->print(errs());
                            // errs() << ":\n";
                            dependencyForest[&*inst] = new std::map<Value*, bool>();
                            Value* srcPtr = inst->getOperand(0);
                            PointerType *ptrType = dyn_cast<PointerType>(srcPtr->getType());
                            Type *pteType = ptrType->getElementType();
                            unsigned size = getTypeBitWidth(pteType);
                            loadInsts[&*inst].push_back({srcPtr, size});
                            memInstAddRAWDep(inst, srcPtr, size, storeInsts);
                        } else if (isa<StoreInst>(*inst)) {
                            // inst->print(errs());
                            // errs() << ":\n";
                            dependencyForest[&*inst] = new std::map<Value*, bool>();
                            auto *tmp = dyn_cast<llvm::StoreInst>(&*inst);
                            Value *srcOp = tmp->getValueOperand();
                            Value *ptrOp = tmp->getPointerOperand();
                            if (isa<Instruction>(*srcOp)) {
                                Instruction *op_inst = dyn_cast<Instruction>(srcOp);
                                if (op_inst->getParent() == bb && (!isa<PHINode>(*op_inst)))
                                    (*dependencyForest[&*inst])[srcOp] = true;
                            }
                            PointerType *ptrType = dyn_cast<PointerType>(ptrOp->getType());
                            Type *pteType = ptrType->getElementType();
                            unsigned size = getTypeBitWidth(pteType);
                            storeInsts[&*inst].push_back({ptrOp, size});
                            memInstAddWARDep(inst, ptrOp, size, loadInsts);
                        } else if (isa<CallInst>(*inst)) {
                            auto *tmp = dyn_cast<llvm::CallInst>(&*inst);
                            Function* foo = tmp->getCalledFunction();
                            if (foo->getName().contains("memcpy")) {
                                dependencyForest[&*inst] = new std::map<Value*, bool>();
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
                                dependencyForest[&*inst] = new std::map<Value*, bool>();
                                std::vector<std::pair<Value*, unsigned>> inOps;
                                std::vector<std::pair<Value*, unsigned>> outOps;
                                auto inList = getBFCInputs(inst);
                                if (inList != NULL) {
                                    for (auto op = inList->begin(); op != inList->end(); op++) {
                                        Type* op_type = (*op)->getType();
                                        if (op_type->isPointerTy()) {
                                            Value *srcPtr = *op;
                                            PointerType *ptrType = dyn_cast<PointerType>(op_type);
                                            Type *pteType = ptrType->getElementType();
                                            unsigned size = getTypeBitWidth(pteType);
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
                                            PointerType *ptrType = dyn_cast<PointerType>(op_type);
                                            Type *pteType = ptrType->getElementType();
                                            unsigned size = getTypeBitWidth(pteType);
                                            outOps.push_back({dstPtr, size});
                                            memInstAddWARDep(inst, dstPtr, size, loadInsts);
                                        }
                                    }
                                }
                                if (inList != NULL)
                                    loadInsts[&*inst].insert(loadInsts[&*inst].end(), inOps.begin(), inOps.end());
                                if (outList != NULL)
                                    storeInsts[&*inst].insert(storeInsts[&*inst].end(), outOps.begin(), outOps.end());
                            }
                        } else if (!(isa<GetElementPtrInst>(*inst) || isa<BitCastInst>(*inst) || isa<AllocaInst>(*inst) || isa<PHINode>(*inst))) {
                            dependencyForest[&*inst] = new std::map<Value*, bool>();
                            for (auto OI = inst->op_begin(); OI != inst->op_end(); ++OI) {
                                if (isa<Instruction>(*OI)) {
                                    Instruction* op_inst = dyn_cast<Instruction>(OI);
                                    if (op_inst->getParent() == bb && (!isa<PHINode>(*op_inst)))
                                        (*dependencyForest[&*inst])[&*op_inst] = true;
                                }
                            }
                        }
                    }
                }
            }

            void mergeExtInstructions() {
                for (auto it = dependencyForest.begin(); it != dependencyForest.end(); it++) {
                    for (auto dep = it->second->begin(); dep != it->second->end();) {
                        Value* dep_inst = dep->first;
                        if (isa<ZExtInst>(*dep_inst) || isa<SExtInst>(*dep_inst)) {
                            Value* new_dep = dependencyForest[dep_inst]->begin()->first;
                            bool rel = dep->second;
                            // errs() << "start erase\n";
                            // errs() << "erase success\n";
                            if (rel)
                                (*(it->second))[new_dep] = rel;
                            else
                                it->second->insert({new_dep, rel});
                            // errs() << "insert success\n";
                            it->second->erase(dep++);
                        } else {
                            ++dep;
                        }
                    }
                }
                for (auto it = dependencyForest.begin(); it != dependencyForest.end();) {
                    Value *inst = it->first;
                    if (isa<ZExtInst>(*inst) || isa<SExtInst>(*inst)) {
                        dependencyForest.erase(it++);
                    } else {
                        ++it;
                    }
                }
            }

            void mergeLoadInstructions() {
                ValueMap<Value*, std::set<Value*>> loadRAWDependents;
                ValueMap<Value*, std::set<Value*>> loadWARDependents;
                ValueMap<Value*, bool> loadMergeable;
                unmergeableLoad.clear();
                for (auto it = dependencyForest.begin(); it != dependencyForest.end(); it++) {
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
                        auto newDepList = dependencyForest[it->first];
                        for (auto inst_it = loadRAWDependents[it->first].begin(); inst_it != loadRAWDependents[it->first].end(); inst_it++) {
                            loadMergedInst.insert(*inst_it);
                            int erase_count = dependencyForest[*inst_it]->erase(it->first);
                            // (*inst_it)->print(errs());
                            // errs() << ": erase " << erase_count << "\n";
                            for (auto newDep = newDepList->begin(); newDep != newDepList->end(); newDep++) {
                                (*dependencyForest[*inst_it])[newDep->first] = newDep->second;
                            }
                        }
                        for (auto inst_it = loadWARDependents[it->first].begin(); inst_it != loadWARDependents[it->first].end(); inst_it++) {
                            dependencyForest[*inst_it]->erase(it->first);
                        }
                        dependencyForest.erase(it->first);
                    }
                }
            }

            void mergeStoreInstructions() {
                ValueMap<Value*, Value*> storeMap;
                unmergeableStore.clear();
                for (auto it = dependencyForest.begin(); it != dependencyForest.end();) {
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
                        for (auto dep = storeWARDependents.begin(); dep != storeWARDependents.end(); dep++) {
                            if (isReachable(*dep, storeRAWDependents)) {
                                mergeable = false;
                                break;
                            }
                        }
                        if (mergeable) {
                            for (auto dep = storeWARDependents.begin(); dep != storeWARDependents.end(); dep++) {
                                dependencyForest[storeSrc]->insert({*dep, false});
                            }
                            storeMap[it->first] = storeSrc;
                            dependencyForest.erase(it++);
                        } else {
                            unmergeableStore.insert(it->first);
                            ++it;
                        }
                    } else {
                        ++it;
                    }
                }
                for (auto it = dependencyForest.begin(); it != dependencyForest.end(); it++) {
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

            void addControlDependency() {
                for (auto it = dependencyForest.begin(); it != dependencyForest.end(); it++) {
                    Instruction *inst = dyn_cast<Instruction>(it->first);
                    if (inst->isTerminator()) {
                        // terminator instruction has control dependence on all other instructions in the same BB
                        BasicBlock *bb = inst->getParent();
                        for (BasicBlock::iterator dep = bb->begin(); dep != bb->end(); dep++) {
                            if ((dependencyForest.find(&*dep) != dependencyForest.end()) && (&*dep != &*inst))
                                dependencyForest[&*inst]->insert({&*dep, false});
                        }
                    }
                }
            }

            void bruMerge(Instruction* inst, int level, int numALU) {
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
                        // Inst needs to occupy an ALU slot to load operand if any of its operand is not computed in the same BB
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
                        for (auto dep = dependencyForestOp[*it]->begin(); dep != dependencyForestOp[*it]->end(); dep++) {
                            if (dep->second) {
                                Value *RAWDepInst = dep->first;
                                bool mergeable = true;
                                if (!isBlueCall(dyn_cast<Instruction>(RAWDepInst))) {
                                    // Check all Primate instructions in the same BB
                                    for (auto pInst = bb->rbegin(); pInst != bb->rend(); pInst++) {
                                        auto pInstDep = dependencyForestOp.find(&*pInst);
                                        if (pInstDep != dependencyForestOp.end()) {
                                            // Only instructions in the new frontier can depend on the instruction to be merged to
                                            if (newFrontier.find(&*pInst) == newFrontier.end()) {
                                                if (pInstDep->second->find(RAWDepInst) != pInstDep->second->end()) {
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
                            if (isa<CallInst>(*mergeDep) || isa<LoadInst>(*mergeDep) || isa<StoreInst>(*mergeDep)) {
                                frontier->insert(mergeDep);
                            } else {
                                newFrontierNext.insert(mergeDep);
                            }
                            (*dependencyForestOp[*it])[mergeDep] = false;
                            mergeableDep.erase(mergeableDep.begin());
                        }
                        if (erasable) {
                            combinedBranchInst.insert(*it);
                            dependencyForestOp.erase(*it);
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
                    auto pInstDep = dependencyForestOp.find(&*pInst);
                    if (pInstDep != dependencyForestOp.end()) {
                        if (frontier->find(pInstDep->first) == frontier->end()) {
                            for (auto brInst = frontier->begin(); brInst != frontier->end(); brInst++) {
                                dependencyForestOp[*brInst]->insert({pInstDep->first, false});
                            }
                        }
                    }
                }
                frontiers[bb] = frontier;
            }

            void mergeBranchInstructions(int level, int numALU) {
                combinedBranchInst.clear();
                for (auto it = dependencyForest.begin(); it != dependencyForest.end(); it++) {
                    Instruction *inst = dyn_cast<Instruction>(it->first);
                    Value* cond;
                    if (inst->isTerminator()) {
                        if (isa<BranchInst>(*inst)) {
                            BranchInst* brInst = dyn_cast<BranchInst>(inst);
                            if (brInst->isUnconditional()) {
                                dependencyForestOp.erase(it->first);
                                continue;
                            } else {
                                cond = brInst->getCondition();
                            }
                        } else if (isa<SwitchInst>(*inst)) {
                            SwitchInst* swInst = dyn_cast<SwitchInst>(inst);
                            cond = swInst->getCondition();
                        } else if (isa<ReturnInst>(*inst)) {
                            // Primate program must return results through BFU
                            dependencyForestOp.erase(it->first);
                            continue;
                        } else {
                            errs() << "Terminator instruction not supported!\n";
                            exit(1);
                        }

                        if (isa<Instruction>(*cond)) {
                            auto condInst = dyn_cast<Instruction>(cond);
                            // Condition value must be an instruction to merge to
                            if (dependencyForestOp.find(cond) != dependencyForestOp.end()) {
                                bool mergeable = true;
                                BasicBlock *bb = inst->getParent();
                                // Check all Primate instructions in the same BB
                                for (auto pInst = bb->rbegin(); pInst != bb->rend(); pInst++) {
                                    auto pInstDep = dependencyForestOp.find(&*pInst);
                                    if (pInstDep != dependencyForestOp.end()) {
                                        // No instruction other than branch instruction depends on the condition value
                                        if (pInstDep->first != (&*inst)) {
                                            if (pInstDep->second->find(cond) != pInstDep->second->end()) {
                                                mergeable = false;
                                                break;
                                            }
                                        }
                                    }
                                }
                                if (mergeable) {
                                    combinedBranchInst.insert(it->first);
                                    dependencyForestOp.erase(it->first);
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

            void buildDependencyForest(Function &F) {
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

            void optimizeDependencyForest(int bruDepth, int numALU) {
                dependencyForestOp.clear();
                frontiers.clear();
                for (auto it = dependencyForest.begin(); it != dependencyForest.end(); it++) {
                    dependencyForestOp[it->first] = new std::map<Value*, bool>((*it->second));
                }
                mergeBranchInstructions(bruDepth, numALU);
            }

            void printDependencyForest(Function &F) {
                int n = 0;
                for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ii++) {
                    if (dependencyForestOp.find(&*ii) != dependencyForestOp.end()) {
                        ii->print(errs());
                        errs() << "\ndepends on:\n";
                        int i = 1;
                        auto depList = dependencyForestOp[&*ii];
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

            void annotatePriority(Function &F, bool optimized) {
                ValueMap<Value*, std::map<Value*, bool>*>* dag;
                if (optimized) dag = &dependencyForestOp;
                else dag = &dependencyForest;
                instPriority.clear();
                std::vector<Value*> waitlist;
                for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
                    BasicBlock *bb = &*bi;
                    waitlist.clear();
                    for (auto ii = bb->rbegin(); ii != bb->rend(); ++ii) {
                        if (dag->find(&*ii) != dag->end()) {
                            // is a Primate instruction
                            int selfPriority = 0;
                            if (instPriority.find(&*ii) == instPriority.end()) {
                                instPriority[(&*ii)] = 0;
                            } else {
                                selfPriority = instPriority[&*ii];
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
                                if (instPriority.find(dep->first) == instPriority.end()) {
                                    instPriority[dep->first] = newPriority;
                                } else if (instPriority[dep->first] < newPriority) {
                                    instPriority[dep->first] = newPriority;
                                    waitlist.push_back(dep->first);
                                }
                            }
                        }
                    }
                    while (!waitlist.empty()) {
                        Value* pInst = waitlist.front();
                        waitlist.erase(waitlist.begin());
                        for (auto dep = (*dag)[pInst]->begin(); dep != (*dag)[pInst]->end(); dep++) {
                            int newPriority;
                            Instruction *inst = dyn_cast<Instruction>(pInst);
                            if (isBlueCall(inst)) newPriority = instPriority[pInst];
                            else {
                                if (dep->second) {
                                    newPriority = instPriority[pInst] + 2;
                                } else {
                                    newPriority = instPriority[pInst] + 1;
                                }
                            }
                            if (instPriority[dep->first] < newPriority) {
                                instPriority[dep->first] = newPriority;
                                waitlist.push_back(dep->first);
                            }
                        }
                    }
                }
            }

            int estimateNumALUs(Function &F) {
                annotatePriority(F, false);
                int numALU = 0;
                for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
                    BasicBlock *bb = &*bi;
                    int maxPriority = 0;
                    int numALUInst = 0;
                    for (auto ii = bb->begin(); ii != bb->end(); ++ii) {
                        if (dependencyForest.find(&*ii) != dependencyForest.end()) {
                            Instruction *inst = dyn_cast<Instruction>(&*ii);
                            if (!isBlueCall(inst) && !inst->isTerminator()) {
                                // inst->print(errs());
                                // errs() << "\n";
                                numALUInst++;
                            }
                            if (instPriority[&*ii] > maxPriority) {
                                maxPriority = instPriority[&*ii];
                            }
                        }
                    }
                    // errs() << "maxPriority: " << maxPriority << "\n";
                    // errs() << "numALUInst: " << numALUInst << "\n";
                    float numALUBB = numALUInst*2.0/(maxPriority+2.0);
                    if (ceil(numALUBB) > numALU) numALU = ceil(numALUBB);
                }
                // errs() << "estimated ALUs: " << numALU << "\n";
                return numALU;
            }

            void VLIWSim(Function &F, int numALU) {
                annotatePriority(F, true);
                int totalInst = 0;
                int totalVLIWInst = 0;
                for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
                    BasicBlock *bb = &*bi;
                    std::vector<std::pair<int, Value*>> nonFrontier;
                    std::vector<Value*> blueCalls;
                    int frontier_size = 0;
                    auto frontier_it = frontiers.find(bb);
                    if (frontier_it != frontiers.end()) {
                        frontier_size = frontier_it->second->size();
                    }
                    for (auto ii = bb->begin(); ii != bb->end(); ++ii) {
                        if (dependencyForestOp.find(&*ii) != dependencyForestOp.end()) {
                            Instruction *inst = dyn_cast<Instruction>(&*ii);
                            if (frontier_it != frontiers.end()) {
                                if (frontier_it->second->find(&*ii) == frontier_it->second->end()) {
                                    if (isBlueCall(inst)) {
                                        blueCalls.push_back(&*ii);
                                    } else {
                                        nonFrontier.push_back({instPriority[&*ii], &*ii});
                                    }
                                }
                            } else {
                                if (isBlueCall(inst)) {
                                    blueCalls.push_back(&*ii);
                                } else {
                                    nonFrontier.push_back({instPriority[&*ii], &*ii});
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
                            for (auto dep = dependencyForestOp[inst->second]->begin(); dep != dependencyForestOp[inst->second]->end(); dep++) {
                                if (dep->second) {
                                    if (scheduledInst.find(dep->first) == scheduledInst.end()) {
                                        scheduleable = false;
                                        break;
                                    }
                                } else {
                                    if ((scheduledInst.find(dep->first) == scheduledInst.end()) && (toScheduleInst.find(dep->first) == toScheduleInst.end())) {
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
                        while (binst != blueCalls.end()) {
                            bool scheduleable = true;
                            for (auto dep = dependencyForestOp[*binst]->begin(); dep != dependencyForestOp[*binst]->end(); dep++) {
                                Instruction* depInst = dyn_cast<Instruction>(dep->first);
                                if (dep->second && isBlueCall(depInst)) {
                                    if (scheduledInst.find(dep->first) == scheduledInst.end()) {
                                        scheduleable = false;
                                        break;
                                    }
                                } else {
                                    if ((scheduledInst.find(dep->first) == scheduledInst.end()) && (toScheduleInst.find(dep->first) == toScheduleInst.end())) {
                                        scheduleable = false;
                                        break;
                                    }
                                }
                            }
                            if (scheduleable) {
                                toScheduleInst.insert(*binst);
                                binst = blueCalls.erase(binst);
                            } else {
                                binst++;
                            }
                        }
                        // VLIW instruction scheduled
                        numVLIWInst++;
                        errs() << "VLIW Inst " << n << ":\n";
                        for (auto inst = toScheduleInst.begin(); inst != toScheduleInst.end(); inst++) {
                            scheduledInst.insert(*inst);
                            (*inst)->print(errs());
                            errs() << "\n";
                        }
                        n++;
                    }
                    // Schedule frontier instructions
                    if (frontier_size > aluLeft) {
                        errs() << "VLIW Inst " << n << ":\n";
                        numVLIWInst++;
                    } else if (frontier_size != 0) {
                        if (numVLIWInst == 0) {
                            errs() << "VLIW Inst " << n << ":\n";
                            numVLIWInst = 1;
                        }
                        bool newInst = false;
                        // If any instruction in the frontier has RAW dependency on previous instructions, it must be a new VLIW instruction
                        for (auto inst = frontier_it->second->begin(); inst != frontier_it->second->end(); inst++) {
                            for (auto dep = dependencyForestOp[*inst]->begin(); dep != dependencyForestOp[*inst]->end(); dep++) {
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
                            errs() << "VLIW Inst " << n << ":\n";
                            numVLIWInst++;
                        }
                    }

                    numInst += frontier_size;
                    totalVLIWInst += numVLIWInst;
                    totalInst += numInst;
                    if (frontier_size != 0) {
                        for (auto inst = frontier_it->second->begin(); inst != frontier_it->second->end(); inst++) {
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

            void initializeBBWeight(Function &F) {
                int numBB;
                for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
                    BasicBlock *bb = &*bi;
                    bbWeight[bb] = 1.0;
                }
            }

            int evalPerf(Function &F, int numALU, double &perf, double &util) {
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
                errs() << "numALU: " << numALU << ", perf: " << perf << ", utilization: " << util << "\n"; 
                return numInst;
            }

            void numALUDSE(Function &F, int &numALU, int &numInst, int option) {
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
            }

            unsigned getNumThreads(Module &M, unsigned numALU) {
                APInt maxVal(64, 0);
                blueFunction = new std::set<std::string>();
                for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
                    MDNode *metadata = MI->getMetadata("primate");
                    if (metadata) {
                        if (cast<MDString>(metadata->getOperand(0))->getString() == "blue") {
                            blueFunction->insert(cast<MDString>(metadata->getOperand(1))->getString().str());
                            auto *latency = cast<ConstantAsMetadata>(metadata->getOperand(2))->getValue();
                            auto latency_val = cast<ConstantInt>(latency)->getValue();
                            if (latency_val.ugt(maxVal)) {
                                maxVal = latency_val;
                            }
                        }
                    }
                }
                return (5 + (4 + numALU) + unsigned(maxVal.getZExtValue()));
            }

            virtual void InitializeBranchLevel(Function &F) {
                branchLevel = new ValueMap<Value*, int>();
                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    (*branchLevel)[&*instruction] = 0;
                }
            }

            void InitializePointerMap(Function &F) {
                pointerMap = new ValueMap<Value*, ptrInfo_t*>();
                for (Function::arg_iterator arg = F.arg_begin(); arg != F.arg_end(); ++arg){
                    const Type* arg_type = arg->getType();
                    if (arg_type->isPointerTy()) {
                        (*pointerMap)[&*arg] = new ptrInfo_t(&*arg, 0);
                    }
                }
                for (inst_iterator instruction = inst_begin(F), e = inst_end(F); instruction != e; ++instruction) {
                    if (isa<AllocaInst>(*instruction)) {
                        (*pointerMap)[&*instruction] = new ptrInfo_t(&*instruction, 0);
                    } else if (isa<GetElementPtrInst>(*instruction)) {
                        auto *inst = dyn_cast<llvm::GetElementPtrInst>(&*instruction);
                        // inst->print(errs());
                        // errs() << ":\n";
                        unsigned offset = 0;
                        Value *basePtr = inst->getPointerOperand();
                        if ((*pointerMap)[basePtr]->base != basePtr) {
                            offset = (*pointerMap)[basePtr]->offset;
                            basePtr = (*pointerMap)[basePtr]->base;
                        }
                        Type *type = inst->getSourceElementType();
                        int i = 0;
                        for (auto idx = inst->idx_begin(); idx != inst->idx_end(); idx++) {
                            if (isa<ConstantInt>(*idx)) {
                                auto *idx_const = dyn_cast<ConstantInt>(idx);
                                auto idx_val = idx_const->getValue();
                                int idx_u = int(idx_val.getSExtValue());
                                if (isa<StructType>(*type)) {
                                    auto stype = dyn_cast<StructType>(type);
                                    if (i == 0) {
                                        unsigned elemWidth = getStructWidth(*stype, 0, false);
                                        offset += idx_u * elemWidth;
                                    } else {
                                        int j = 0;
                                        Type *tmp;
                                        for (auto elem = stype->element_begin(); elem != stype->element_end(), j <= idx_u; elem++, j++) {
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
                                } else {
                                    errs() << "Error: undefined type: ";
                                    inst->print(errs());
                                    errs() << "\n";
                                    exit(1);
                                }
                            } else {
                                errs() << "Error: pointer is not constant: ";
                                inst->print(errs());
                                errs() << "\n";
                                exit(1);
                            }
                        }
                        (*pointerMap)[&*instruction] = new ptrInfo_t(basePtr, offset);
                        // errs() << offset << "\n";
                    } else if (isa<BitCastInst>(*instruction)) {
                        Value *srcOp = instruction->getOperand(0);
                        (*pointerMap)[&*instruction] = (*pointerMap)[&*srcOp];
                    }
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
                        if (unmergeableLoad.find(&*instruction) == unmergeableLoad.end()) {
                            (*aliasMap)[&*instruction] = (*aliasMap)[&*srcOp];
                        } else {
                            (*aliasMap)[&*instruction] = &*instruction;
                        }
                        // errs() << "load " << srcOp->getName() << "\n";
                    } else if (isa<StoreInst>(*instruction)) {
                        auto *inst = dyn_cast<llvm::StoreInst>(&*instruction);
                        srcOp = inst->getValueOperand();
                        Value *ptrOp = inst->getPointerOperand();
                        if (unmergeableStore.find(&*instruction) == unmergeableStore.end()) {
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

                InitializePointerMap(F);

                buildDependencyForest(F);

                numALU = estimateNumALUs(F);

                // printDependencyForest(F);
                numALUDSE(F, numALU, numInst, BALANCE);

                maxConst = getMaxConst(F);

                InitializeAliasMap(F);

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

                primateCFG.open("primate.cfg");
                primateHeader.open("header.scala");
                assemblerHeader.open("primate_assembler.h");

                assemblerHeader << "#include <iostream>\n#include <map>\n#include <string>\n\n";
                printRegfileKnobs(M);
                generate_header(M);

                int maxNumALU = 0;
                int maxNumInst = 0;
                unsigned maxConst = 0;
            
                for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
                {
                    int numALU = 0, numInst = 0;
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
                unsigned maxLatency = getNumThreads(M, maxNumALU);

                primateCFG << "NUM_THREADS=" << int(pow(2, ceil(log2(maxLatency)))) << "\n";
                errs() << "Number of regs: " << numRegs << "\n";
                primateCFG << "NUM_REGS=" << int(pow(2, ceil(log2(numRegs)))) << "\n";
                assemblerHeader << "#define NUM_REGS " << int(pow(2, ceil(log2(numRegs)))) << "\n";
                assemblerHeader << "#define NUM_REGS_LG int(ceil(log2(NUM_REGS)))\n";

                primateCFG << "NUM_ALUS=" << maxNumALU << "\n";
                primateCFG << "NUM_BFUS=" << blueFunction->size() - 1 << "\n";
                assemblerHeader << "#define NUM_ALUS " << maxNumALU << "\n";
                assemblerHeader << "#define NUM_FUS " << maxNumALU + blueFunction->size() - 1 << "\n";
                assemblerHeader << "#define NUM_FUS_LG int(ceil(log2(NUM_FUS)))\n";

                primateCFG << "IP_WIDTH=" << int(ceil(log2(maxNumInst))) << "\n";
                assemblerHeader << "#define IP_W " << int(ceil(log2(maxNumInst))) << "\n";
                errs() << "Number of instructions: " << maxNumInst << "\n";

                primateCFG << "IMM_WIDTH=" << int(ceil(log2(maxConst))) << "\n";
                assemblerHeader << "#define IMM_W " << int(ceil(log2(maxConst))) << "\n";

                primateCFG.close();
                primateHeader.close();
                assemblerHeader.close();
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

