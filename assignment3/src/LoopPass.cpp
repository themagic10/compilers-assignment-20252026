#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
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
#include <cstdio>
#include <vector>

using namespace llvm;

namespace {

bool IsInstructionInvariant(Instruction *i, Loop *l) {
  /*
    some notes on this:

    1. reading mayHaveSideEffects reveals that it does not consider the alloca
    instruction to have any side effects. mayReadOrWriteMemory source code does
    the same thing despite the fact that in the official language reference
    documentation alloca is listed under the memory instruction..

    despite this alloca needs to be excluded as according to the docs the stack
    frame is released when the function returns to the caller. moving it out of
    the loop will cause problems: the resulting pointer *should* point to newly
    allocated stack addresses with each iteration of the loop where an
    out-of-loop alloca will produce only one pointer stuck at the same
    address...

    2. the isTerminator function covers jump instruction (even calls and
    returns) since they are always at the end of a bb. we assume functions to
    NOT be loop invariant. note that we can use a function inlining optimization
    pass before the loop pass to try and optimize them...

    3. phi operation have to be accounted for
  */

  if (i->mayHaveSideEffects() || i->mayReadOrWriteMemory() || isa<PHINode>(i) ||
      i->isTerminator() || isa<AllocaInst>(*i)) {
    return false;
  }

  for (auto op = i->op_begin(); op != i->op_end(); ++op) {
    if (isa<Instruction>(op)) {
      Instruction *iop = dyn_cast<Instruction>(op);
      if (l->contains(iop)) {
        return IsInstructionInvariant(iop, l);
      }
    }
  }
  return true;
}

// TODO: does it guarantee that the instruction are fetched in order?
// TODO: maybe use l->hasLoopInvariantOperands(const Instruction *I)??
std::vector<Instruction *> GetLoopInvariantInst(Loop *l) {
  std::vector<Instruction *> r;
  for (auto bbiter : l->blocks()) {
    for (auto institer = bbiter->begin(); institer != bbiter->end();
         ++institer) {
      Instruction *i = &*institer;
      if (IsInstructionInvariant(i, l)) {
        r.push_back(i);
      }
    }
  }

  return r;
}

bool isDeadAfterLoop(Instruction *i, Loop *l) {
  for (auto u = i->use_begin(); u != i->use_end(); ++u) {
    if (!l->contains(dyn_cast<Instruction>(u->get()))) {
      return false;
    }
  }
  return true;
}

bool doesDominateAllExits(BasicBlock *bb, Loop *l, DominatorTree &d) {
  SmallVector<BasicBlock *> exits;
  l->getExitingBlocks(exits);
  for (auto e = exits.begin(); e != exits.end(); ++e) {
    if (!d.dominates(bb, *e)) {
      return false;
    }
  }
  return true;
}

bool isMoveSafe(Instruction *i, Loop *l, DominatorTree &dt) {
  /*

  in order to evaluate if an instruction can be moved we have to check if:
  1. it's loop invariant
  2. it dominates all of its users
  3. it dominates all of its exits or if the instruction is dead outside of the
  loop


  since llvm uses ssa for its virtual registers we effectively just need to
  check condition number 3

  */
  return doesDominateAllExits(i->getParent(), l, dt) || isDeadAfterLoop(i, l);
}

void RunLicm(Loop *l, DominatorTree &dt) {
  std::vector<Instruction *> invariants = GetLoopInvariantInst(l);
  std::vector<Instruction *> final;
  for (auto i : invariants) {
    if (isMoveSafe(i, l, dt)) {
      final.push_back(i);
    }
  }
  // moving...
  BasicBlock *preheader = l->getLoopPreheader();
  // doing this keeps the same order too
  for (Instruction *i : final) {
    outs() << "currently moving" << *i << "\n";
    i->moveBefore(&*preheader->getTerminator());
  }
}

struct AlgebraicIdentity : PassInfoMixin<AlgebraicIdentity> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &li = AM.getResult<LoopAnalysis>(F);
    DominatorTree &dt = AM.getResult<DominatorTreeAnalysis>(F);
    for (auto l : li) {
      RunLicm(l, dt);
    }

    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

} // namespace

llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MyLicm", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-licm") {
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