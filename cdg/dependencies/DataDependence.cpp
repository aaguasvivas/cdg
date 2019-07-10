// See DataDependencies.h for more information

#include "DataDependence.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

// Enable debugging output to stderr
//#define MK_DEBUG

void DataDependence::getDataDependencies(Function &F, MemoryDependenceAnalysis &MDA,
    AliasAnalysis &AA) {
  // Since MemoryDependenceAnalysis is a function pass, we need to pass the
  // function we are looking at to the pass
  //MemoryDependenceAnalysis &MDA = getAnalysis<MemoryDependenceAnalysis>(F);

  for (inst_iterator i = inst_begin(F); i != inst_end(F); ++i) {
    Instruction *inst;
    inst = &*i;

    // skip non memory accesses
    if (!inst->mayReadFromMemory() && !inst->mayWriteToMemory())
      continue;

    processDepResult(inst, MDA, AA);

  } // end for (inst_iterator)
}

void DataDependence::processDepResult(Instruction *inst, 
    MemoryDependenceAnalysis &MDA, AliasAnalysis &AA) {
  // TODO: This is probably a good place to check of the dependency
  // information is calculated on-demand
  MemDepResult Res = MDA.getDependency(inst);


  if (!Res.isNonLocal()) {
    // local results (not-non-local) can be simply handled. They are just
    // a pair of insturctions and a dependency type

    // Get dependency information
    DepInfo newInfo;
    newInfo = getDepInfo(Res);

#ifdef MK_DEBUG
    //errs() << "[DEBUG] newInfo depInst == " << Res.getInst() << '\n';
    if (Res.getInst() == NULL) {
      errs() << "[DEBUG] NULL dependency found, dep type: "
             << depTypeToString(newInfo.Type_) << '\n';
    }
#endif

    // Save into map
    assert(newInfo.valid());
    LocalDeps_[inst] = newInfo;
  }
  else {
    // Handle NonLocal dependencies. The function call
    // getNonLocalPointerDependency() assumes that a result of NonLocal
    // has already been encountered
    
    // Get dependency information
    DepInfo newInfo;
    newInfo = getDepInfo(Res);

    assert(newInfo.Type_ == NonLocal);
    assert(Res.isNonLocal());

    SmallVector<NonLocalDepResult, 4> NLDep;
    if (LoadInst *LI = dyn_cast<LoadInst>(inst)) {
      if (!LI->isUnordered()) {
        // FIXME: Handle atomic/volatile loads.
        errs() << "[WARNING] atomic/volatile loads are not handled\n";
        assert(false && "atomic/volatile loads not handled");
        //Deps[Inst].insert(std::make_pair(getInstTypePair(0, Unknown),
                                         //static_cast<BasicBlock *>(0)));
        return;
      }
      AliasAnalysis::Location Loc = AA.getLocation(LI);
      MDA.getNonLocalPointerDependency(Loc, true, LI->getParent(), NLDep);
    } 
    else if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
      if (!SI->isUnordered()) {
        // FIXME: Handle atomic/volatile stores.
        errs() << "[WARNING] atomic/volatile stores are not handled\n";
        assert(false && "atomic/volatile stores not handled");
        //Deps[Inst].insert(std::make_pair(getInstTypePair(0, Unknown),
                                         //static_cast<BasicBlock *>(0)));
        return;
      }
      AliasAnalysis::Location Loc = AA.getLocation(SI);
      MDA.getNonLocalPointerDependency(Loc, false, SI->getParent(), 
          NLDep);
    } 
    else if (VAArgInst *VI = dyn_cast<VAArgInst>(inst)) {
      AliasAnalysis::Location Loc = AA.getLocation(VI);
      MDA.getNonLocalPointerDependency(Loc, false, VI->getParent(), 
          NLDep);
    } 
    else {
      llvm_unreachable("Unknown memory instruction!");
    }

#ifdef MK_DEBUG
    errs() << "[DEBUG] NLDep.size() == " << NLDep.size() << '\n';
#endif
    for (auto I = NLDep.begin(), E = NLDep.end(); I != E; ++I) {
      NonLocalDeps_[inst].push_back(*I);
    }
  } // end else
}

const char *DataDependence::depTypeToString(DepType d) {
  // NOTE: Change this if you add more stuff to DepType
  switch (d) {
    case Clobber:
      return "Clobber";
      break;
    case Def:
      return "Def";
      break;
    case NonFuncLocal:
      return "NonFuncLocal";
      break;
    case NonLocal:
      return "NonLocal";
      break;
    case Unknown:
      return "Unknown";
      break;
    case Invalid:
      return "Invalid";
      break;
  }
  llvm_unreachable("unknown DepType encountered");
}

DataDependence::DepInfo::DepInfo() {
  DepInst_ = NULL;
  Type_ = Invalid;
}

bool DataDependence::DepInfo::valid() {
  if (Type_ == Invalid)
    return false;
  // For some dependencies, the dependent instruction is NULL (ie, for non
  // function local dependencencies)
  //if (DepInst_ == NULL)
    //return false;
  return true;
}

DataDependence::DepInfo::DepInfo(Instruction *i, DepType d) {
  Type_ = d;
  DepInst_ = i;
}

DataDependence::DepInfo DataDependence::getDepInfo(MemDepResult dep) {
  if (dep.isClobber())
    return DepInfo(dep.getInst(), Clobber);
  if (dep.isDef())
    return DepInfo(dep.getInst(), Def);
  if (dep.isNonFuncLocal())
    return DepInfo(dep.getInst(), NonFuncLocal);
  if (dep.isUnknown())
    return DepInfo(dep.getInst(), Unknown);
  if (dep.isNonLocal())
    return DepInfo(dep.getInst(), NonLocal);
  llvm_unreachable("unknown dependence type");
}
