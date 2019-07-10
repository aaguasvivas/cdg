// Author: Markus Kusano
//
// Finds the data dependences of a given function. This requires both
// AliasAnalysis and MemoryDependenceAnalysis pass information
//
// The main entry point for this class is getDependenceInfo(). This function
// takes an llvm::Function and the MemoryDependenceAnalysis and AliasAnalysis
// information for the function. For example to use this in a module pass:
//
//  bool runOnModule(Module &M) {
//    AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
//
//    for (auto mi = M.begin(); mi != M.end(); ++mi) {
//      // skip external functions
//      if (mi->isDeclaration())
//        continue;
//
//      // Since MemoryDependenceAnalysis is a function pass, we need to pass the
//      // current function we are examining to the getAnalysis() call.
//      MemoryDependenceAnalysis &MDA = getAnalysis<MemoryDependenceAnalysis>(*mi);
//
//      DataDep.getDataDependencies(*mi, MDA, AA);
//
//    } // end for (module::iterator)
//  }
//
//
//  Two maps LocalDeps_ and NonLocalDeps_ contain the dependency information.
//  You can call getDataDependencies() on all the functions in a module to
//  build all the dependency information in these two maps.

#include "llvm/IR/Module.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"

using namespace llvm;
class DataDependence {
  public:
    // Dependence type (see MemoryDependenceAnalysis.{cpp,h}).
    //
    // From MemoryDependenceAnalysis.h
    //
    // Clobber - This is a dependence on the specified instruction which
    // clobbers the desired value.  The pointer member of the MemDepResult
    // pair holds the instruction that clobbers the memory.  For example,
    // this occurs when we see a may-aliased store to the memory location we
    // care about.
    //
    // There are several cases that may be interesting here:
    //   1. Loads are clobbered by may-alias stores.
    //   2. Loads are considered clobbered by partially-aliased loads.  The
    //      client may choose to analyze deeper into these cases.
    //
    //      
    // Def - This is a dependence on the specified instruction which
    // defines/produces the desired memory location.  The pointer member of
    // the MemDepResult pair holds the instruction that defines the memory.
    // Cases of interest:
    //   1. This could be a load or store for dependence queries on
    //      load/store.  The value loaded or stored is the produced value.
    //      Note that the pointer operand may be different than that of the
    //      queried pointer due to must aliases and phi translation.  Note
    //      that the def may not be the same type as the query, the pointers
    //      may just be must aliases.
    //   2. For loads and stores, this could be an allocation instruction. In
    //      this case, the load is loading an undef value or a store is the
    //      first store to (that part of) the allocation.
    //   3. Dependence queries on calls return Def only when they are
    //      readonly calls or memory use intrinsics with identical callees
    //      and no intervening clobbers.  No validation is done that the
    //      operands to the calls are the same.
    //
    //
    // The following indicate that the query has no known dependency
    // in the specified block.  More detailed state info is encoded in the
    // upper part of the pair (i.e. the Instruction*)
    //
    // NonLocal - This marker indicates that the query has no dependency in
    // the specified block.  To find out more, the client should query other
    // predecessor blocks.
    //
    // NonFuncLocal - This marker indicates that the query has no
    // dependency in the specified function.
    //
    // Unknown - This marker indicates that the query dependency
    // is unknown.
    enum DepType {
      Clobber = 0,
      Def = 1,
      NonFuncLocal = 2,
      NonLocal = 3,
      Unknown = 4,
      Invalid = 99 // represents an invalid dep info
    };

    // Print out the enum DepType to a string
    static const char *depTypeToString(DepType d);

    // Get data dependencies. This function stores its results in class
    // internal data structures.
    //
    // MDA is assumed to be the MemoryDependenceAnalysis information of the
    // function F. AA is the alias analysis information for the current module.
    void getDataDependencies(Function &F, MemoryDependenceAnalysis &MDA, AliasAnalysis &AA);

    // Dependence Information. Used in a map of Instruction -> DepInfo.
    // Depinfo contains the instruction that is depended on (depInst) and the
    // DepType (clobber, def, nonfunclocal, nonlocal, unknown)
    //
    // In this pass, this is only used for dependence types that are not
    // NonLocal. NonLocal dependencies have some additional information and are
    // stored in a different data-structure and map
    struct DepInfo {
      DepInfo();
      DepInfo(Instruction *i, DepType d); 
      bool valid();

      Instruction *DepInst_; // inst that is depended on
      DepType Type_;

    };
    // The two following maps, when examined together, hold all the dependence
    // information obtained from the MemoryDependenceAnalysis pass
    
    // Map of Instructions to DepInfo. DepInfo is the dependence information
    // of the instruction, ie the instruction (template argument 1) depends
    // on another instruction contained in DepInfo (template argument 2)
    std::map<Instruction *, DepInfo> LocalDeps_;

    // Map of instructions to vectors of NonLocalDepResults. This stores the
    // data for non local dependencies
    std::map<Instruction *, std::vector<NonLocalDepResult> > NonLocalDeps_;

  private:
    // Helper functions
    // Processes MemoryDependenceAnalysis result for the given
    // instruction and stores the information in class internal data structures
    void processDepResult(Instruction *inst, MemoryDependenceAnalysis &MDA, AliasAnalysis &AA);

    // Apated from MemDepPrinter(). This interprets the dependency result and
    // returns a pair of the instruction that is depended on 
    static DepInfo getDepInfo(MemDepResult dep);

};
