#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <cstddef>
#include <cstdio>
#include <ostream>
#include <vector>

using namespace llvm;

namespace {

typedef struct LoopPair {
  Loop *li = nullptr;
  Loop *lj = nullptr;
} LoopPair;

/*
see leadsTo() for more information
check if a src block is part of a chain of unconditional jumps that lead to a
target bb
*/
bool bbLeadsTo(BasicBlock *src, BasicBlock *target) {
  if (src->size() > 1) {
    return false;
  }
  if (!isa<BranchInst>(src->getTerminator())) {
    return false;
  }

  BranchInst *j = dyn_cast<BranchInst>(src->getTerminator());
  if (j->isUnconditional()) {
    if (j->getSuccessor(0) == target) {
      return true;
    } else {
      return bbLeadsTo(j->getSuccessor(0), target);
    }
  }
  return false;
}

/*
basic idea: if 2 loops are adjacent, both guarded and the first loop have
multiple exits (all leading to loop 2) we can have a situation where we have a
merge inside the guard bb (a phi instruction). in this case we have multiple
exits in loop 1 that are NOT the second loop's guard, they *should* be a bb with
just an unconditional jump to the guard bb.

if we take it a step further we can image a sequence of n times of these "jump
only" bb's that eventually leads to a guard

|l0 exit| -> |jump| -> ... -> |jump| -> |l1 guard|

in this case the loops are still effectively adjacent. this can also work if l0
have multiple exits (if they all just start a chain of jumps that lead to l1).

this is most likely overkill but let's implement this regardless

*/
bool loopLeadsTo(Loop *l, BasicBlock *target) {
  SmallVector<BasicBlock *> exits;
  l->getExitBlocks(exits);
  for (BasicBlock *exit : exits) {
    if (!bbLeadsTo(exit, target)) {
      return false;
    }
  }
  return true;
}

bool areLoopAdjacent(Loop *li, Loop *lj) {

  // no guard
  if (!li->isGuarded() && !lj->isGuarded()) {
    BasicBlock *preheader = lj->getLoopPreheader();

    // if we have more than just the terminator (jump) instruction then the
    // preheader will execute some instruction between the 2 loops
    if (preheader->size() > 1) {
      outs() << "preheader has extra information\n";
      return false;
    }

    // TODO could be improved with lead to but since we do run simplify loop
    // there shouldn't be any need...
    if (li->getExitBlock() == lj->getLoopPreheader()) {
      return true;
    } else {
      outs() << "loop does not lead to preheader\n";
    }
  } else if (li->isGuarded() && lj->isGuarded()) {
    // note that the check for equal condition is done in cfgeq fun

    auto li_guard = li->getLoopGuardBranch();
    auto lj_guard = lj->getLoopGuardBranch();

    // out of loop edge's bb, hopefully lj's guard
    BasicBlock *bypassbb;

    // note that branch instruction only have 2 possible labels. since this is a
    // guard we can safely say that it's not a unconditional jump

    if (li_guard->getSuccessor(0) == li->getLoopPreheader()) {
      bypassbb = li_guard->getSuccessor(1);
    } else {
      bypassbb = li_guard->getSuccessor(0);
    }

    // bypass skips lj guard
    if (bypassbb != lj->getLoopGuardBranch()->getParent()) {
      return false;
    }
    if (loopLeadsTo(li, lj_guard->getParent())) {
      return true;
    }
  }

  // all other cases are false
  // mixed guard cases are always treated as unfusionable
  return false;
}

// TODO fix clang format, this is ugly
bool LoopDominate(Loop *li, Loop *lj, DominatorTree &dt) {
  if (!li->isGuarded() && !lj->isGuarded()) {

    return dt.dominates(li->getHeader(), lj->getHeader());

  } else if (li->isGuarded() && lj->isGuarded()) {

    return dt.dominates(li->getLoopGuardBranch()->getParent(),
                        lj->getLoopPreheader());
  }
  return false;
}

bool LoopPostDominate(Loop *li, Loop *lj, PostDominatorTree &pdt) {
  BasicBlock *li_target;
  if (li->isGuarded() && lj->isGuarded()) {
    li_target = li->getLoopGuardBranch()->getParent();
  } else if (!li->isGuarded() && !lj->isGuarded()) {
    li_target = li->getHeader();
  } else {
    return false;
  }
  if (pdt.dominates(lj->getHeader(), li_target)) {
    return true;
  }
  return false;
}

// li and lj are assumed to be both guarded
bool doGuardsHaveSameCondition(Loop *li, Loop *lj) {
  Instruction *li_cond =
      dyn_cast<Instruction>(li->getLoopGuardBranch()->getCondition());
  Instruction *lj_cond =
      dyn_cast<Instruction>(lj->getLoopGuardBranch()->getCondition());
  if (li_cond->isIdenticalTo(lj_cond)) {
    return true;
  }
  return false;
}

bool areLoopCfgEquivalent(Loop *li, Loop *lj, DominatorTree &dt,
                          PostDominatorTree &pdt) {
  if (!li->isGuarded() && !lj->isGuarded()) {
    return LoopDominate(li, lj, dt) && LoopPostDominate(li, lj, pdt);
  } else if (li->isGuarded() && lj->isGuarded()) {
    return LoopDominate(li, lj, dt) && LoopPostDominate(li, lj, pdt) &&
           doGuardsHaveSameCondition(li, lj);
  }
  return false;
}

bool loopSameIteration(Loop *li, Loop *lj, ScalarEvolution &se) {
  const SCEV *li_back = se.getBackedgeTakenCount(li);
  const SCEV *lj_back = se.getBackedgeTakenCount(lj);

  // possbile output according to getBackedgeTakenCount()
  if (isa<SCEVCouldNotCompute>(li_back) || isa<SCEVCouldNotCompute>(lj_back)) {
    return false;
  }

  if (li_back == lj_back) {
    return true;
  }

  return false;
}

/*
  true value is returned if there is no negative dependency
*/
bool checkNegativity(Loop *li, Loop *lj, DependenceInfo &di,
                     ScalarEvolution &se) {

  std::vector<Instruction *> iwrite, jread;

  for (BasicBlock *bbi : li->blocks()) {
    for (Instruction &iinst : *bbi) {
      if (isa<StoreInst>(iinst)) {
        iwrite.push_back(&iinst);
      }
    }
  }

  for (BasicBlock *bbj : lj->blocks()) {
    for (Instruction &jinst : *bbj) {
      if (isa<LoadInst>(jinst)) {
        jread.push_back(&jinst);
      }
    }
  }

  // no r/w to memory = no problems
  if (iwrite.empty() || jread.empty()) {
    return true;
  }

  // could have probably been one single double loop...
  for (auto iinst : iwrite) {
    for (auto jinst : jread) {

      // first check for dependency
      auto dep = di.depends(iinst, jinst, true);
      if (!dep) {
        continue;
      }

      Value *storepointer = getLoadStorePointerOperand(iinst);
      Value *loadpointer = getLoadStorePointerOperand(jinst);

      const SCEV *storescev = se.getSCEVAtScope(storepointer, li);
      const SCEV *loadscev = se.getSCEVAtScope(loadpointer, lj);

      if (isa<SCEVCouldNotCompute>(storescev) ||
          isa<SCEVCouldNotCompute>(loadscev)) {
        return false;
      }

      // we need to rewrite the store instruction in order to match it with the
      // load
      const SCEVAddRecExpr *storeaddrec = dyn_cast<SCEVAddRecExpr>(storescev);

      // required since the operands gives an arrayref, not a vector
      SmallVector<const SCEV *> ops = to_vector(storeaddrec->operands());
      const SCEV *storerewritten = se.getAddRecExpr(ops, lj, SCEV::FlagAnyWrap);

      // names from minusScev documenation, it's a "subtraction" after all
      // note: apparently it actually does lhs + rhs*-1
      // returns scevcouldnotcompute if they don't have the same base.
      const SCEV *minusscev = se.getMinusSCEV(storerewritten, loadscev);

      // probably redundant since we have the dependency check..
      // minusscev should be not compute only on different bases..
      if (isa<SCEVCouldNotCompute>(minusscev)) {
        outs() << "minus scev is undefined";
        return false;
      }

      // since we found a dependency we must evaluate the known negative as
      // false, otherwise we risk producing an invalid fusion
      if (se.isKnownNonNegative(minusscev)) {
        continue;
      } else {
        return false;
      }
    }
  }

  return true;
}

std::vector<LoopPair> makePairs(std::vector<Loop *> loops) {
  std::vector<LoopPair> ret;
  int size = loops.size();
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      if (i != j) {
        ret.push_back({loops.at(i), loops.at(j)});
      }
    }
  }
  return ret;
}

// will return false if the second loop header as more than 1 node inside the
// loop since we can't say where the tree of bodies should start
bool canFuse(Loop *li, Loop *lj, DominatorTree &dt, PostDominatorTree &pdt,
             ScalarEvolution &se, DependenceInfo &di) {
  int n = 0;
  for (auto bb : successors(lj->getHeader())) {
    if (lj->contains(bb)) {
      n++;
    }
  }
  if (n > 1) {
    outs() << "too many exits node\n";
    return false;
  }
  outs() << "conditions: " << areLoopAdjacent(li, lj)
         << areLoopCfgEquivalent(li, lj, dt, pdt)
         << loopSameIteration(li, lj, se) << checkNegativity(li, lj, di, se)
         << "\n";
  return areLoopAdjacent(li, lj) && areLoopCfgEquivalent(li, lj, dt, pdt) &&
         loopSameIteration(li, lj, se) && checkNegativity(li, lj, di, se);
}

/*
useful rule: all phis in a block must be at the start of the block

deal with the phi instructions and move all non terminator instruction in li
header
*/
void moveheader(Loop *li, Loop *lj) {

  // update induction variables
  PHINode *iinduction = li->getCanonicalInductionVariable();
  PHINode *jinduction = lj->getCanonicalInductionVariable();
  jinduction->replaceAllUsesWith(iinduction);

  // we could leave it to dce but it's annoying to deal with just below...
  jinduction->eraseFromParent();

  SmallVector<Instruction *> ljhead;

  // prevents data invalidation
  for (Instruction &i : *lj->getHeader()) {
    ljhead.push_back(&i);
  }

  for (Instruction *i : ljhead) {

    if (i->isTerminator() || isa<FCmpInst>(i) || isa<FCmpInst>(i)) {
      continue;
    } else if (isa<PHINode>(i)) {
      PHINode *p = dyn_cast<PHINode>(i);
      int num = p->getNumIncomingValues();
      if (num > 2) {
        // most likely impossible due to previous checks but still
        // loop structure has been violated since the header should only have 2
        // prevs: preheader and latch
        errs() << "detected header with more than 2 prevs, loop structure "
                  "violeted, aborting\n";
        exit(1);
      }

      PHINode *newp = PHINode::Create(p->getType(), 0);
      for (int iter = 0; iter < 2; iter++) {
        BasicBlock *bb = p->getIncomingBlock(iter);
        Value *v = p->getIncomingValue(iter);
        if (lj->contains(bb)) {
          // should be an induction variable but you never know
          newp->addIncoming(v, li->getLoopLatch());
        } else {
          newp->addIncoming(v, li->getLoopPreheader());
        }
      }
      newp->insertBefore(li->getHeader()->getFirstInsertionPt());
      outs() << *li->getHeader()->getTerminator();
      p->replaceAllUsesWith(newp);
      outs() << *li->getHeader()->getTerminator();
    } else { // non phi instructions
      i->moveBefore(li->getHeader()->getTerminator());
    }
  }
  outs() << "\nfinal header:\n";
  for (Instruction &x : *li->getHeader()) {
    outs() << x << "\n";
  }
}

/*
moves all the body nodes from loop j to i

specifically we set i's last bodies before the latch to the first latch of j
then we set the last bodies of j to the latch of i

note that this assumes that j body tree starts with just 1 node

NOTE THAT THIS WILL CRASH IF WE FIND A NON BRANCH TERMINATOR
(this should not happen but it's possible)
*/
void movebody(Loop *li, Loop *lj, LoopInfo &ll) {
  BasicBlock *ilatch = li->getLoopLatch();
  BasicBlock *jlatch = lj->getLoopLatch();

  // avoids invalidation
  SmallVector<BasicBlock *> iprevs = to_vector(predecessors(ilatch));

  // we first update the first loop latch's prev to point to the start of the
  // second loop's body
  for (auto bb : iprevs) {
    if (bb == li->getHeader()) {
      // most likely redundant, if this edge were to exist then it should be an
      // inner loop... can't hurt to check
      continue;
    }

    // to be fair this should never be a problem... a complex body structure
    // would most likely trigger a SCEVCouldNotCompute when calculating loop
    // iterations...
    BranchInst *i = dyn_cast<BranchInst>(bb->getTerminator());
    if (i == nullptr) {
      errs() << "expected branch instruction in the loop, too complex\n";
      // harsh but we most likely already modified the loop if we got
      // here, it's safer to throw an error rather than have a
      // corrupted ll file
      exit(1);
    }
    i->getNumSuccessors();
    for (int n = 0; n < i->getNumSuccessors(); n++) {
      if (i->getSuccessor(n) == ilatch) {
        // we need to find the start of lj body
        BasicBlock *bodystart;
        for (auto possibletarget : successors(lj->getHeader())) {
          if (lj->contains(possibletarget)) {
            bodystart = possibletarget;
          }
        }

        i->setSuccessor(n, bodystart);
      }
    }
  }

  SmallVector<BasicBlock *> jprev = to_vector(predecessors(jlatch));

  // set the second loop latch's prev to point to the first loop latch
  for (auto bb : jprev) {
    if (bb == lj->getHeader()) {
      continue;
    }

    // as above
    BranchInst *i = dyn_cast<BranchInst>(bb->getTerminator());
    if (i == nullptr) {
      errs() << "expected branch instruction in the loop, too complex\n";
      exit(1);
    }
    for (int n = 0; n < i->getNumSuccessors(); n++) {
      if (i->getSuccessor(n) == jlatch) {
        i->setSuccessor(n, ilatch);
      }
    }
    // if we don't add this then li won't know it contains bb and more
    // importantly li will think it has multiple exiting block which will break
    // everything
    li->addBasicBlockToLoop(bb, ll);
  }
}

void setnewexit(Loop *li, BasicBlock *ljexit) {
  BasicBlock *exiting = li->getExitingBlock();
  BranchInst *i = dyn_cast<BranchInst>(exiting->getTerminator());
  outs() << "starting exit branch is " << *i << "\n";
  if (i == nullptr) {
    errs() << "expected branch on loop exit not found, aborting\n";
    exit(1);
  }
  for (int n = 0; n < 2; n++) {
    if (!li->contains(i->getSuccessor(n))) {
      i->setSuccessor(n, ljexit);
    }
  }
  outs() << "\n final update branch is: " << *i << "\n";
}

void updateGuard(Loop *li, BasicBlock *jexit) {
  BranchInst *bints = li->getLoopGuardBranch();
  for (int i = 0; i < 2; i++) {
    if (!li->contains(bints->getSuccessor(i))) {
      bints->setSuccessor(i, jexit);
    }
  }
}

// loops are assumed to be in simple form and to not be mixed guarded
void executeFusion(Loop *li, Loop *lj, LoopInfo &ll) {
  BasicBlock *ljexit = lj->getExitBlock();
  if (li->isGuarded() && lj->isGuarded()) {
    updateGuard(li, lj->getExitBlock());
  }
  moveheader(li, lj);
  outs() << "moved header!\n";
  movebody(li, lj, ll);
  outs() << "moved body!\n";
  setnewexit(li, ljexit);
  outs() << "set new exit!\n";
}

void purgePairs(std::vector<LoopPair> &pairs, Loop *li, Loop *lj) {
  for (LoopPair lp : pairs) {
    if (lp.li == li || lp.lj == lj || lp.li == lj || lp.lj == li) {
      lp.li = NULL;
      lp.lj = NULL;
    }
  }
}

bool startingLoopCheck(Loop *l) {
  return l->isLoopSimplifyForm() && l->isInnermost() &&
         (l->getExitingBlock() != nullptr);
}

/* assumptions:
1. each loop is in simple form
2. each loop has 1 exiting block
3. the second loop must have only 1 exit block
4. the first loop can have as many exits as it wants but they must all be an
unconditional branch to the second's loop preheader/guard
4.5 you can also have a chain of bbs with just unconditional jumps to the next
bb until you get to the second loop if you really want to
5. the second loop's headers must have exacly 1 successor inside the loop
6. the second loop's body cannot have terminators that aren't branches
*/
void runLoopFusion(Function &F, FunctionAnalysisManager &AM) {
  LoopInfo &li = AM.getResult<LoopAnalysis>(F);
  DominatorTree &dt = AM.getResult<DominatorTreeAnalysis>(F);
  PostDominatorTree &pdt = AM.getResult<PostDominatorTreeAnalysis>(F);
  ScalarEvolution &se = AM.getResult<ScalarEvolutionAnalysis>(F);
  DependenceInfo &di = AM.getResult<DependenceAnalysis>(F);

  std::vector<Loop *> loops;
  for (Loop *topl : li) {
    for (Loop *l : depth_first(topl)) {
      if (startingLoopCheck(l)) {
        loops.push_back(l);
      }
    }
  }

  std::vector<LoopPair> pairs = makePairs(loops);

  for (LoopPair lp : pairs) {

    outs() << "now testing: " << *lp.li->getHeader()->begin() << " "
           << *lp.lj->getHeader()->begin() << "\n";

    if (canFuse(lp.li, lp.lj, dt, pdt, se, di)) {
      outs() << "LOOP FUSION CANDIDATE FOUND\n";
      executeFusion(lp.li, lp.lj, li);
    }
    purgePairs(pairs, lp.li, lp.lj);
  }
}

struct AlgebraicIdentity : PassInfoMixin<AlgebraicIdentity> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    runLoopFusion(F, AM);
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

} // namespace

llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "lfusion", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "lfusion") {
                    FPM.addPass(AlgebraicIdentity());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize TestPass when added to the pass pipeline on the
// command line, i.e. via '-passes=test-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getTestPassPluginInfo();
}