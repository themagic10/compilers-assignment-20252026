#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

void OptimizeAddition(Instruction *i) {
  Value *op0 = i->getOperand(0);
  Value *op1 = i->getOperand(1);

  bool is_op0_zero, is_op1_zero = false;

  if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
    if (c->isZero()) {
      is_op0_zero = true;
    }
  }

  if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
    if (c->isZero()) {
      is_op1_zero = true;
    }
  }

  if (is_op0_zero || is_op1_zero) {
    // non-zero value. in case both operands are zero then the target
    // will be 0..
    Value *target;
    if (is_op0_zero) {
      target = op1;
    } else {
      target = op0;
    }
    i->replaceAllUsesWith(target);
  }
}

void OptimizeSubtraction(Instruction *i) {
  /*
    a - 0 -> a
    in case of 0-a we leave it at that
  */

  Value *op0 = i->getOperand(0);
  Value *op1 = i->getOperand(1);

  bool is_op1_zero = false;

  if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
    if (c->isZero()) {
      is_op1_zero = true;
    }
  }

  if (is_op1_zero) { // works even if both are 0
    i->replaceAllUsesWith(op0);
  }
}

void OptimizeMultiplication(Instruction *i) {
  Value *op0 = i->getOperand(0);
  Value *op1 = i->getOperand(1);

  bool is_op0_zero, is_op1_zero = false;
  bool is_op0_one, is_op1_one = false;

  if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
    if (c->isZero()) {
      is_op0_zero = true;
    } else if (c->isOne()) {
      is_op0_one = true;
    }
  }

  if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
    if (c->isZero()) {
      is_op1_zero = true;
    } else if (c->isOne()) {
      is_op1_one = true;
    }
  }

  if (is_op0_zero || is_op1_zero) { // zero is the target
    Value *target;
    if (is_op0_zero) {
      target = op0;
    } else {
      target = op1;
    }
    i->replaceAllUsesWith(target);
  } else if (is_op0_one || is_op1_one) { // non-one is the target
    Value *target;
    if (is_op0_one) {
      target = op1;
    } else {
      target = op0;
    }
    i->replaceAllUsesWith(target);
  }
}

void OptimizeDivision(Instruction *i) {
  /*
    0/a -> 0
    a/1 -> a
    note that we do not care about a/0, the compiler should throw a
    warning for that and its not really something that can be optimized..

    we still need to check if op1 is zero to avoid 0/0 inconsistency
  */

  Value *op0 = i->getOperand(0);
  Value *op1 = i->getOperand(1);

  bool is_op0_zero, is_op1_zero = false;
  bool is_op1_one = false;

  if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
    if (c->isZero()) {
      is_op0_zero = true;
    }
  }

  if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
    if (c->isZero()) {
      is_op1_zero = true;
    } else if (c->isOne()) {
      is_op1_one = true;
    }
  }

  // small optimization, in both case op0 replaces the instruction..
  if ((is_op0_zero || is_op1_one) && !is_op1_zero) {
    i->replaceAllUsesWith(op0);
  }
}

struct AlgebraicIdentity : PassInfoMixin<AlgebraicIdentity> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    for (auto bbiter = F.begin(); bbiter != F.end(); ++bbiter) {
      for (auto institer = bbiter->begin(); institer != bbiter->end();
           ++institer) {

        Instruction *targetinst = &*institer;
        switch (targetinst->getOpcode()) {

        case Instruction::Add:
          OptimizeAddition(targetinst);
          break;

        case Instruction::Sub:
          OptimizeSubtraction(targetinst);
          break;

        case Instruction::Mul:
          OptimizeMultiplication(targetinst);
          break;

        case Instruction::SDiv:
          OptimizeDivision(targetinst);
          break;
        case Instruction::UDiv:
          OptimizeDivision(targetinst);
          break;

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
  return {LLVM_PLUGIN_API_VERSION, "localopts", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "alg-id") {
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