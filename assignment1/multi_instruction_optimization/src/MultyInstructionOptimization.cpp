#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
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
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <sys/types.h>
#include <vector>

using namespace llvm;

namespace {

typedef struct binopinfo {
  unsigned int opcode;
  Value *op0 = nullptr;
  Value *op1 = nullptr;
  bool op0_constant = false;
  bool op1_constant = false;
  int op0_int = 0;
  int op1_int = 0;

} binopinfo;

// returns ret.opcode = unreachable if the opcode is not add or sub
binopinfo getInstructionInfo(Instruction *i) {
  binopinfo ret;

  ret.opcode = i->getOpcode();
  if (ret.opcode != Instruction::Add && ret.opcode != Instruction::Sub) {
    ret.opcode = Instruction::Unreachable;
    return ret;
  }

  ret.op0 = i->getOperand(0);
  ret.op1 = i->getOperand(1);

  if (ConstantInt *c = dyn_cast<ConstantInt>(ret.op0)) {
    ret.op0_constant = true;
    ret.op0_int = c->getSExtValue();
  }
  if (ConstantInt *c = dyn_cast<ConstantInt>(ret.op1)) {
    ret.op1_constant = true;
    ret.op1_int = c->getSExtValue();
  }

  return ret;
}

/*
  (x + k) + j -> x + h[=j+k]
  (x + k) - x -> k
  same for -
  (x * k)*j = x*h; h = k*j

  CONSTRAINT: OPTIMIZATIONS ARE ONLY LOCAL (INSIDE BB)


*/
/*
ONLY CALL THIS FUNCTION IF INSTRUCTION IS ADD OR SUB

let k,j constant

a = x + k
b = a + j -> b = x + (k+j)
---
a = k - x
b = a - j -> b = k-x-j = (k-j)-x
---
a = x - k
b = a - j -> b = x - k - j

a = x + k
b = a - j -> b = x + k - j

note that even though it seems like we're replacing instruction b we're actually
modifying the constant operand in instruction a, dce will hit instruction b

this is convinient but leads to a problem: let's say we have

a = x+2;
b = a + 5;
c = b - 10;

our code will correctly update a = x + 7 but c will be wrong since it became a
user of a only afterwards

we create a worklist of users and visited users for that

*/
void MultiInstOpt(Instruction *i) {
  outs() << "currently optimizing instruction " << *i << "\n";
  binopinfo bi = getInstructionInfo(i);

  // we need <variable> <op> <const>
  if (bi.op0_constant & bi.op1_constant) {
    outs() << "invalid structure\n";
    return;
  }

  SmallVector<User *> allusers = to_vector(i->users());
  SmallVector<User *> visitedusers;

  for (auto x = allusers.begin(); x != allusers.end(); ++x) {
    if (llvm::is_contained(visitedusers, *x)) {
      continue;
    }
    visitedusers.push_back(*x);
    if (Instruction *newi = dyn_cast<Instruction>(*x)) {
      if (newi->getOpcode() == Instruction::Add ||
          newi->getOpcode() == Instruction::Sub) {
        bi = getInstructionInfo(i); // we need to recalculate this...
        int constoperand;
        int constval;

        if (bi.op0_constant) {
          constval = bi.op0_int;
          constoperand = 0;
        } else {
          constval = bi.op1_int;
          constoperand = 1;
        }
        binopinfo bni = getInstructionInfo(newi);

        // no need to check if both operands are constant since they're a user
        // of i. we need to check if both are NOT constant, in this case we
        // cannot do anything
        if (!bni.op0_constant && !bni.op1_constant) {
          outs() << "user invalid structure\n";
          continue;
        }

        int constop;
        int newconstval;
        if (bni.op0_constant) {
          constop = 0;
          newconstval = bni.op0_int;
        } else {
          constop = 1;
          newconstval = bni.op1_int;
        }
        int result;
        if (bni.opcode == Instruction::Add) {
          result = constval + newconstval;
        } else {
          result = constval - newconstval;
        }
        auto newconstoperand = ConstantInt::get(i->getType(), result, true);
        for (auto futureusers : newi->users()) {
          allusers.push_back(futureusers);
        }
        i->setOperand(constoperand, newconstoperand);
        newi->replaceAllUsesWith(i);
      }
    }
  }
  return;
}
struct MultyInstructionOptimization
    : PassInfoMixin<MultyInstructionOptimization> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    for (auto bbiter = F.begin(); bbiter != F.end(); ++bbiter) {
      for (auto institer = bbiter->begin(); institer != bbiter->end();
           ++institer) {
        switch (institer->getOpcode()) {
        case Instruction::Add: {
          MultiInstOpt(&*institer);
          break;
        }
        case Instruction::Sub: {
          MultiInstOpt(&*institer);
          break;
        }
        default:
          break;
        }
      }
    }

    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

} // namespace

llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "miop", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "miop") {
                    FPM.addPass(MultyInstructionOptimization());
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