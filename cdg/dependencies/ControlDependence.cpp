 * See ControlDependences.h for more information
 */

#include "ControlDependence.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

// Enable debugging to stderr
#define MK_DEBUG

// From Ferrante et al. the algorithm for obtaining control dependency
// information is:
//
// Let S consist of all edges (A,B) in the control flow graph (A->B) such that
// B is not an ancestor of A in the post-dominator tree (ie B does not
// post-dominate A).
//
// Each pair (A,B) in S is then examined. Let L denote the least common
// ancestor of A and B in the post-dominator tree. L can be two things (see the
// paper for proofs)
//
// Case 1. L = parent of A. All nodes in the post-dominator tree on the path
// from L to B, including B but not L, should be made control dependent on A. 
//
// Case 2. L = A. All nodes in the post-dominator tree on the path from A to B,
// including A and B, should be made control dependent on A. (This case
// captures loop dependence.)
//
// This can be done by simply traversing backwards from B in the post-dominator
// tree until we reach A's parent (if it exists). We mark every node as control
// dependent on A.

void ControlDependence::getControlDependencies(Function &F, PostDominatorTree &PDT) {
  // All edges in the CFG (A->B) such that B does not post-dominate A
  vector<CFGEdge> S;

  S = getNonPDomEdges(F, PDT);

#ifdef MK_DEBUG
  errs() << "[DEBUG] Size of set S: " << S.size() << '\n';
#endif

  updateControlDependencies(S, PDT);

}

vector<ControlDependence::CFGEdge> ControlDependence::getNonPDomEdges(
    Function &F, const PostDominatorTree &PDT) const {
  std::vector<CFGEdge> S;
  for (auto BBi = F.begin(), BBe = F.end(); BBi != BBe; ++BBi) {
    BasicBlock *A = &(*BBi);
    // Get the edge A->B, ie get the successors of A
    for (succ_iterator Si = succ_begin(A), Se = succ_end(A); 
        Si != Se; ++ Si) {
      BasicBlock *B = *Si;
      // properlyDominates - Returns true iff A dominates B and A != B.
      // See the base class file Dominators.h for some more documentation than
      // what is given in PostDominators.h
      //
      // Note: Ferrante et al. refer to dominance/post-dominance a little bit
      // differently than others. They do not consider A to dominate B is A ==
      // B. This is referred to as strictly dominates, or properly dominantes
      // in LLVM.
      if (!PDT.properlyDominates(B, A)) {
        // B does not post-dominate A in the edge (A->B), this is the criteria
        // for being in the set S
        CFGEdge e;
        e.head = B;
        e.tail = A; // head refers to the head of the arrow
        S.push_back(e);
      }
    }
  }

  return S;
}

void ControlDependence::updateControlDependencies(const vector<ControlDependence::CFGEdge> &S, 
    PostDominatorTree &PDT) {
  BasicBlock *L;

  for (vector<CFGEdge>::size_type i = 0; i < S.size(); ++i) {
    CFGEdge curEdge;
    curEdge = S[i];

    BasicBlock *A;
    BasicBlock *B;

    A = curEdge.tail;
    B = curEdge.head; // (A->B)

    // Again, from Dominator.h documentation (rather than PostDominators.h):
    // findNearestCommonDominator - Find nearest common dominator basic block
    // for basic block A and B. If there is no such block then return NULL.
    L = PDT.findNearestCommonDominator(A, B);
    if (L == NULL) {
      continue;
    }
    // it is implied from this point on that L != NULL
    
    // Case 1. L = parent of A. All nodes in the post-dominator tree on the path
    // from L to B, including B but not L, should be made control dependent on A. 
    //
    // Case 2. L = A. All nodes in the post-dominator tree on the path from A to B,
    // including A and B, should be made control dependent on A. (This case
    // captures loop dependence.)
    //
    // This can be done by simply traversing backwards from B in the post-dominator
    // tree until we reach A's parent (if it exists). We mark every node as control
    // dependent on A.

    DomTreeNode *domNodeA;
    domNodeA = PDT.getNode(A);

    DomTreeNode *parentA;
    parentA = domNodeA->getIDom();  // could be NULL?

#ifdef MK_DEBUG
    if (parentA == NULL) {
      errs () << "[DEBUG] Found NULL parent of A\n";
    }
#endif

    DomTreeNode *domNodeB;
    domNodeB = PDT.getNode(B);

    DomTreeNode *curNode;
    curNode = domNodeB;

    // For an edge (A->B) we are building up the nodes that are control
    // dependent on A. We only need to do one query of the map to get the set
    // of nodes dependent on A, add more nodes to it, and then re-insert it
    // into the map
    std::set<BasicBlock *> &depSet = controlDeps_[A];

    // Operator[] should return a reference to the value referred to by the key
    // A. If there is no value then it should insert one and use the default
    // ctor (emtpy set).
    //depSet = controlDeps_[A];

    while (curNode != parentA) {
#ifdef MK_DEBUG
      errs() << "[DEBUG] Iterating up dom tree\n";
#endif
      // Mark each node visited on our way to the parent of A, but not A's
      // parent, as control dependent on A
      depSet.insert(curNode->getBlock());

      // Update cur
      curNode = curNode->getIDom();
    } // end while (cur != parentA)
    // Because std::map::operator[] returns a reference to the set then I
    // believe we do not need to do any insertions
#ifdef MK_DEBUG
    errs() << "[DEBUG] size of depSet: " << depSet.size() << '\n';
    errs() << "[DEBUG] size of map value: " << controlDeps_[A].size() << '\n';
    assert(depSet.size() == controlDeps_[A].size() && "map size mis-match");
#endif

  } // end for(vector<>)
}

void ControlDependence::toDot(std::string name) const {
  if (name.empty()) {
#ifdef MK_DEBUG
    errs() << "[DEBUG] dot file name is empty\n";
#endif
    name = "controldeps.dot";
  }

  // The set of nodes that have already been inserted into the dot file. This
  // is used so we don't define the same node twice in the file
  std::set<BasicBlock *> insertedNodes;

  // attempt to open output stream
 // std::ofstream out;
  //out.open(name, std::ios::out);

  std::string errInfo;
  raw_fd_ostream out(name.c_str(), errInfo);

  // check for errors
  if (!errInfo.empty()) {
    errs() <<  "[Warning] Error opening output file: " << errInfo << '\n';
    return;
  }

  // header for dot file
  out << "digraph \"CDG for " << name << " module\" {\n";

#ifdef MK_DEBUG
  errs() << "[DEBUG] making dot file controlDeps_.size(): " << controlDeps_.size() << '\n';
#endif

  // create an edge for each dependency
  for (auto i = controlDeps_.begin(), 
      ei = controlDeps_.end(); i != ei; ++i) {
    BasicBlock *tail;
    tail = i->first;


    std::set<BasicBlock *> depSet;
    depSet = i->second;

    for (auto j = depSet.begin(), ej = depSet.end(); j != ej; ++j) {
      // In a CDG, Y is a descendent of X iff Y is control dependent on X
      // everything in the depset in control dependent up tail. So in this case
      // depset is Y and tail is X. Edges should go from tail -> depset[j]
      
      // insert the node if necessary
      if (insertedNodes.find(*j) == insertedNodes.end()) {
        // We've never encountered this basicblock before so create a node with
        // it in the file
        insertedNodes.insert(*j);
        insertDotNode(out, *j);
      }

      // Insert the edge: TODO: I'm not sure if there is a chance the same edge
      // will be added twice
      insertDotEdge(out, tail, *j);
    }
  }

  // closing brace
  out << "}";

  // close the file
  out.close();
}

void ControlDependence::insertDotNode(raw_fd_ostream &out, BasicBlock *node) const {
  // Use the pointer value to make each node name unique. The label for the
  // node is the contents of the basicblock
  out << "Node" << node << " [shape=record, label=\""; 

  // change newlines to \l for left alrignment in graphviz node
  std::string bbstr;
  raw_string_ostream bbo(bbstr);
  bbo << *node;

  std::size_t hit;
  hit = bbstr.find('\n');

  while (hit != std::string::npos) {
#ifdef MK_DEBUG
    errs() << "[DEBUG} \\n found at pos: " << hit << '\n';
#endif
    bbstr.erase(hit, 1);
    bbstr.insert(hit, "\\l");
    hit = bbstr.find('\n');
  }

  // label body to dot file
  out << bbstr;

  // Close the label and the node
  out << "\"];\n";
}


void ControlDependence::insertDotEdge(raw_fd_ostream &out, BasicBlock *A, BasicBlock *B) const {
  // Nodes are named by their address
  out << "Node" << A << "->" << "Node" << B << '\n';
}

ControlDependence::CFGEdge::CFGEdge() {
  tail = NULL;
  head = NULL;
}
