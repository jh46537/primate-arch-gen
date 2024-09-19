#ifndef LLVM_TRANSFORMS_PRIMATE_PRIMATEARCHGEN_H
#define LLVM_TRANSFORMS_PRIMATE_PRIMATEARCHGEN_H

#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <map>
#include <math.h>
#include <set>
#include <stdlib.h>

#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Primate/dataflow.h"

#define MAX_BR_LEVEL 2
#define MAX_PERF 0
#define BALANCE 1

#define DEBUG_TYPE "primate-arch-gen"

namespace llvm {

class PrimateArchGen : public PassInfoMixin<PrimateArchGen>, 
                       public DataFlow<BitVector>, 
                       public AssemblyAnnotationWriter {
public:
    // set forward false in the constructor DataFlow()
    PrimateArchGen() : DataFlow<BitVector>(false) {
        // bvIndexToInstrArg = new std::vector<Value*>();
        // valueToBitVectorIndex = new ValueMap<Value*, int>();
        // instrInSet = new ValueMap<const Instruction*, BitVector*>();
        aliasMap = new ValueMap<Value*, Value*>();
        branchLevel = new ValueMap<Value*, int>();

        dependencyForest = new ValueMap<Value*, std::map<Value*, bool>*>();
        dependencyForestOp = new ValueMap<Value*, std::map<Value*, bool>*>();
        instPriority = new ValueMap<Value*, int>();
        bfIdx = new ValueMap<Value*, int>();
    }

    // ~PrimateArchGen() {
    //     dbgs() << "1\n";
    //     delete bvIndexToInstrArg;
    //     dbgs() << "2\n";
    //     delete valueToBitVectorIndex;
    //     dbgs() << "3\n";
    //     delete instrInSet;
    //     dbgs() << "4\n";
    //     delete aliasMap;
    //     dbgs() << "5\n";
    //     delete branchLevel;
    //     dbgs() << "6\n";

    //     delete pointerMap;
    //     dbgs() << "7\n";
    //     delete dependencyForest;
    //     dbgs() << "8\n";
    //     delete dependencyForestOp;
    //     dbgs() << "9\n";
    //     delete instPriority;
    //     dbgs() << "10\n";
    //     delete bfIdx;
    //     dbgs() << "11\n";
    // }
    
    PreservedAnalyses run(Module &M, ModuleAnalysisManager& AM);
    void getAnalysisUsage(AnalysisUsage &AU) const;
    
private:
    struct ptrInfo_t {
        Value *base;
        int offset;
        ptrInfo_t(Value *base = NULL, 
                  int offset = 0) : base(base), offset(offset) {}
    };

    // domain vector to store all definitions and function arguments
    std::vector<Value*> domain;
    std::vector<Value*> *bvIndexToInstrArg; 	  // map values to their bitvector
    ValueMap<Value*, int> *valueToBitVectorIndex; // map values (args and variables) to their bit vector index
    ValueMap<const Instruction*, BitVector*> *instrInSet; // IN set for an instruction inside basic block
    ValueMap<Value*, Value*> *aliasMap;
    ValueMap<Value*, int> *branchLevel;
    std::set<unsigned> *gatherModes;
    std::map<unsigned, std::set<unsigned>*> *fieldIndex;
    ValueMap<Value*, ptrInfo_t*> *pointerMap;
    ValueMap<Value*, std::map<Value*, bool>*> *dependencyForest;
    ValueMap<Value*, std::map<Value*, bool>*> *dependencyForestOp;
    ValueMap<Value*, int> *instPriority;
    std::set<Value*> loadMergedInst;
    std::set<Value*> unmergeableLoad;
    std::set<Value*> unmergeableStore;
    std::set<Value*> combinedBranchInst;
    std::map<BasicBlock*, std::set<Value*>*> frontiers;
    std::map<BasicBlock*, double> bbWeight;
    std::map<BasicBlock*, int> bbNumInst;
    std::map<BasicBlock*, int> bbNumVLIWInst;

    int numBFs;
    std::map<std::string, std::set<Value*>*> bfu2bf;
    std::map<std::string, int> bfuNumInputs;
    std::vector<Value*> blueFunctions;
    ValueMap<Value*, int> *bfIdx;
    int **bfConflictMap;
    int **bfConflictMap_tmp;

    int domainSize;
    int numArgs;
    int numInstr;
    int numALU_min;
    
    int live[50];
    unsigned int n = 0;
    
public:
    static char ID;

    // print live variables before each basic block
    virtual void 
    emitBasicBlockStartAnnot(const BasicBlock *bb, 
                             formatted_raw_ostream &os) override;
    // print live variables before each instruction
    // (used for computing histogram)
    virtual void 
    emitInstructionAnnot(const Instruction *i, 
                         formatted_raw_ostream &os) override;

private:
    virtual void setBoundaryCondition(BitVector *BlkBoundry) override;
    virtual void meetOp(BitVector* lhs, const BitVector* rhs) override;
    virtual BitVector* 
    initializeFlowValue(BasicBlock& BB, SetType setType) override; 
    virtual BitVector* transferFn(BasicBlock& BB) override;
    
    bool isLifetimeCall(Instruction *i);
    bool isDefinition(Instruction *i);
    void calculate(const Instruction *ii);
    bool isBlueCall(Instruction *ii);
    unsigned getArrayWidth(ArrayType &a, unsigned start);
    unsigned getArrayWidthArcGen(ArrayType &a, unsigned start);
    unsigned getStructWidth(StructType &s, unsigned start, const bool arcGen);
    unsigned getTypeBitWidth(Type *ty, bool trackSizes = false);
    void printRegfileKnobs(Module &M, raw_fd_stream &primateCFG);
    void generate_header(Module &M, raw_fd_stream &primateHeader);
    unsigned getMaxConst(Function &F);
    std::vector<Value*>* getBFCOutputs(Instruction *ii);
    std::vector<Value*>* getBFCInputs(Instruction *ii);
    bool 
    checkMemAlias(Value *ptr0, unsigned size0, Value *ptr1, unsigned size1);
    bool isReachable(Value* src, std::set<Value*> &dst);
    
    inline void 
    memInstAddRAWDep(Instruction* inst, Value* srcPtr, unsigned size, 
                     ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> &storeInsts);
    inline void 
    memInstAddWARDep(Instruction* inst, Value* dstPtr, unsigned size, 
                     ValueMap<Value*, std::vector<std::pair<Value*, unsigned>>> &loadInsts);
    
    void initializeDependencyForest(Function &F);
    void mergeExtInstructions();
    void mergeLoadInstructions();
    void mergeStoreInstructions();
    void addControlDependency();
    void bruMerge(Instruction* inst, int level, int numALU);
    void mergeBranchInstructions(int level, int numALU);
    void buildDependencyForest(Function &F);
    void optimizeDependencyForest(int bruDepth, int numALU);
    void printDependencyForest(Function &F);
    void annotatePriority(Function &F, bool optimized);
    
    int estimateNumALUs(Function &F);
    
    void addBFCDependency(Value* bfc, std::map<Value*, 
                          std::set<Value*>> &bfcConflict, 
                          std::set<Value*> &toScheduleInst);
    void addBFDependency(std::map<Value*, 
                         std::set<Value*>> &bfcConflict, 
                         std::set<Value*> &bfcSet);
    
    void VLIWSim(Function &F, int numALU);
    void initializeBBWeight(Function &F);
    int evalPerf(Function &F, int numALU, double &perf, double &util);
    void numALUDSE(Function &F, int &numALU, int &numInst, int option);
    void initializeBFCMeta(Module &M);
    void generateInterconnect(int numALU, raw_fd_stream &interconnectCFG);
    unsigned getNumThreads(Module &M, unsigned numALU);

    virtual void InitializeBranchLevel(Function &F);
    virtual void InitializePointerMap(Function &F);
    virtual void InitializeAliasMap(Function &F);
    virtual bool evalFunc(Function &F, int &numALU, 
                          int &numInst, unsigned &maxConst);
    
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_PRIMATE_PRIMATEARCHGEN_H
