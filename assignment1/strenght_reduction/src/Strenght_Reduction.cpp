#include "llvm/IR/BasicBlock.h"
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
#include <cstdint>
#include <math.h>
#include <sys/types.h>
#include <vector>

using namespace llvm;

namespace {

typedef struct reduction {
  std::vector<int> shifts;
  int addition = 0;
} reduction;

reduction evaluate_reduction(reduction r, int val) {
  if (val == 0) {
    return r;
  } else if (val == 1) {
    r.addition = 1;
    return r;
  } else {
    int shifting = floor(log2(val));
    r.shifts.push_back(shifting);
    int carry = val - pow(2, shifting);
    return evaluate_reduction(r, carry);
  }
}

struct StrenghtReduction : PassInfoMixin<StrenghtReduction> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    for (auto bbiter = F.begin(); bbiter != F.end(); ++bbiter) {
      for (auto institer = bbiter->begin(); institer != bbiter->end();
           ++institer) {

        switch (institer->getOpcode()) {
        case (Instruction::Mul): {

          // do note that we do not care about constant x constant scenarios
          // strenght reduction cares about variable x constant scenarios
          // we also ignore x*0 and x*1 scenarios, that's algebraic identity
          Value *target_var;
          Value *target_const;
          uint target_amt = 0;

          Value *op0 = institer->getOperand(0);
          Value *op1 = institer->getOperand(1);

          // we work with positive numbers. at the end if the value is negative
          // we multiply the final result by -1
          bool is_negative = false;
          int val = 0;

          if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
            target_const = op0;
            val = c->getSExtValue();
            if (val < 0) {
              is_negative = true;
              val = -val;
            }
            target_var = op1;
            ++target_amt;
          }
          if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
            target_const = op1;
            val = c->getSExtValue();
            if (val < 0) {
              is_negative = true;
              val = -val;
            }
            target_var = op0;
            ++target_amt;
          }

          if (target_amt == 1 || val != 1 || val != 0) {
            reduction res =
                evaluate_reduction({{}, 0}, val); // anon reduction struct

            Instruction *latest_instruction = &*institer;

            for (int elem : res.shifts) {
              /*  for each elem ->  %ishift = lshr prev elem
                                    %iadd = add %a prev
                  then prev <- %iadd and elem++
              */

              Value *c =
                  ConstantInt::get(Type::getInt32Ty(F.getContext()),
                                   elem); // note: type refers to integer type,
                                          // llvm context grabbed from function
              Instruction *ishift =
                  BinaryOperator::Create(Instruction::LShr, target_var, c);
              ishift->insertAfter(latest_instruction);

              //"base case" is the first shift, we don't add that to anything
              // else
              if (latest_instruction != &*institer) {
                Instruction *iadd = BinaryOperator::Create(
                    Instruction::Add, ishift, latest_instruction);
                iadd->insertAfter(ishift);
                latest_instruction = iadd;
              } else {
                latest_instruction = ishift;
              }
            }
            if (res.addition == 1) {
              Value *c = ConstantInt::get(Type::getInt32Ty(F.getContext()),
                                          res.addition);
              Instruction *i = BinaryOperator::Create(Instruction::Add,
                                                      latest_instruction, c);
              i->insertAfter(latest_instruction);
              latest_instruction = i;
            }
            institer->replaceAllUsesWith(latest_instruction);
          }
          break;
        }
        case (Instruction::UDiv): {
          Value *op0 = institer->getOperand(0);
          Value *op1 = institer->getOperand(1);
          uint64_t val = 0;
          // strenght reduction with division is only possible if the costant is
          // op1
          if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
            break;
          }

          if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
            val = c->getZExtValue();
          } else {
            break;
          }

          int lg = log2(val);
          if (pow(2, lg) != val) {
            break;
          }

          Value *c = ConstantInt::get(Type::getInt32Ty(F.getContext()), lg);
          // logical cause it's unsigned division
          Instruction *i = BinaryOperator::Create(Instruction::LShr, op1, c);

          institer->replaceAllUsesWith(i);
          i->insertAfter(&*institer);

          break;
        }
        // TODO finish this and test
        case (Instruction::SDiv): {
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
  return {LLVM_PLUGIN_API_VERSION, "strenghtreduction", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "str-red") {
                    FPM.addPass(StrenghtReduction());
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