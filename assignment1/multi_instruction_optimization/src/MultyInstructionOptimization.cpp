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

/*
  (x + k) + j -> x + h[=j+k]
  (x + k) - x -> k
  same for -
  (x * k)*j = x*h; h = k*j

  CONSTRAINT: OPTIMIZATIONS ARE ONLY LOCAL (INSIDE BB)


*/
struct MultyInstructionOptimization
    : PassInfoMixin<MultyInstructionOptimization> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    for (auto bbiter = F.begin(); bbiter != F.end(); ++bbiter) {
      for (auto institer = bbiter->begin(); institer != bbiter->end();
           ++institer) {
        switch (institer->getOpcode()) {
        case Instruction::Add: {
          Value *op0 = institer->getOperand(0);
          Value *op1 = institer->getOperand(1);
          bool is_op0_const, is_op1_const = false;

          if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
            is_op0_const = true;
          }
          if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
            is_op1_const = true;
          }

          if (is_op0_const && is_op1_const) {
            ConstantInt *c0 = dyn_cast<ConstantInt>(op0);
            ConstantInt *c1 = dyn_cast<ConstantInt>(op0);
            if (c0->isNegative() || c1->isNegative()) {
              // signed
              int64_t v0 = c0->getSExtValue();
              int64_t v1 = c1->getSExtValue();
              int64_t res = v0 + v1;
              Value *vres =
                  ConstantInt::get(Type::getInt64Ty(F.getContext()), res);
              institer->replaceAllUsesWith(vres);
            } else {
              // unsigned
              uint64_t v0 = c0->getZExtValue();
              uint64_t v1 = c1->getZExtValue();
              uint64_t res = v0 + v1;
              Value *vres =
                  ConstantInt::get(Type::getInt64Ty(F.getContext()), res);
              institer->replaceAllUsesWith(vres);
            }
          } else { // y = x + k, look for y + j or y - x
            // non constant operand
            Value *target_var = (is_op1_const) ? op0 : op1;
            for (auto futureinst : institer->users()) {
              Instruction *i = dyn_cast<Instruction>(futureinst);
              if (i != nullptr && i->getOpcode() == Instruction::Add) {
                Value *otherop;
                uint8_t otherop_pos = 0;
                // we check the pointers
                if (futureinst->getOperand(0) == target_var) {
                  otherop_pos = 1;
                } else {
                  otherop_pos = 0;
                }
                otherop = futureinst->getOperand(otherop_pos);

                // we only care if otherop is a constant or is -x
              }
            }
          }

          break;
        }
        case Instruction::Sub: {
          break;
        }
        case Instruction::Mul: {
          break;
        }
        case Instruction::UDiv: {
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