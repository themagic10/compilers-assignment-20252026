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
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <math.h>
#include <sys/types.h>
#include <vector>

#define MAXSUM 64 // effectively disable maxsum

using namespace llvm;

namespace {

enum ShiftOperation { UNDEFINED, ADDITION, SUBTRACTION, TERMINATOR };

typedef struct shift {
  int value;
  ShiftOperation op;
} shift;

typedef struct reduction {
  std::vector<shift> shifts;
  int addition = 0;
  bool istooexpensive = false;
} reduction;

/*

here's the idea: suppose we have a number x that is evaluated to be between two
powers of 2

i=2^a > x > 2^b=j

we calculate |x-i| and |x-j|, the smallest is the match, we set shift.value=a(or
b). let n be min(|x-i|,|x-j|) if the bigger power wins (i) we set operation sub
since we know that x = i-|x-i|. we then execute the function recursively on
|x-i|.

the base cases are as follow:
- 0, we're finished
- 1, set addition = 1
- if x is a power of 2 we just need to do a single shift, best case scenario

note: if the distance is the same (this should only happen with x=3...) we
arbitrarily choose to use the lower power

*/
reduction evaluate_reduction(reduction r, int val) {

  if (val == 0) {
    return r;
  } else if (val == 1) {
    r.addition = 1;
    return r;
  } else {

    // strenght reduction is evaluated to be too expensive, give up and use a
    // mul to save cpu cycles
    if (r.shifts.size() >= MAXSUM) {
      r.istooexpensive = true;
      return r;
    }

    int upper_pow = ceil(log2(val));
    int lower_pow = floor(log2(val));

    // val = 2^pow scenario
    if (upper_pow == lower_pow) {
      shift s;
      s.value = upper_pow;
      s.op = TERMINATOR; // kinda useless..
      r.shifts.push_back(s);
      return r;
    }

    int upper_diff = abs(pow(2, upper_pow) - val);
    int lower_diff = abs(pow(2, lower_pow) - val);

    shift s;
    int power, target;
    if (lower_diff <= upper_diff) {
      s.op = ADDITION;
      power = lower_pow;
      target = lower_diff;
    } else {
      s.op = SUBTRACTION;
      power = upper_pow;
      target = upper_diff;
    }
    s.value = power;
    r.shifts.push_back(s);

    // TODO: we should pass r by reference, not by copy, it's a waste of time...
    return evaluate_reduction(r, target);
  }
}

// debug
void printReduction(reduction r) {
  outs() << "printing reduction: \n";
  if (r.istooexpensive) {
    outs() << "reduction is too expensive!\n";
    return;
  }
  for (shift elem : r.shifts) {
    outs() << elem.value << " " << elem.op << "\n";
  }
  outs() << "addition? ";
  outs() << r.addition << "\ndone\n";
}

// do note that we do not care about <constant> x <constant> scenarios
// strenght reduction cares about <variable> x <constant> scenarios
// we also ignore x*0 and x*1 scenarios, that's algebraic identity
void MultiplicationReduction(Instruction *i) {

  Value *target_var;
  Value *target_const;

  bool assignedtarget = false; // used to check if both are constants

  Value *op0 = i->getOperand(0);
  Value *op1 = i->getOperand(1);

  /*
  we work with positive numbers. at the end if the value is negative
  we multiply the final result by -1, note that this can be optimized since
  bitwise we have that: -x = !x + 1
  note that we will NOT implement that optimization in the strenght reduction
  optimization pass (but it should be trivial)
  */
  bool is_negative = false; // TODO: maybe add this in algebriac identity?
  int val = 0;

  if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
    target_const = op0;
    val = c->getSExtValue();
    if (val < 0) {
      is_negative = true;
      val = -val;
    }
    target_var = op1;
    assignedtarget = true;
  }
  if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
    if (assignedtarget) {
      // both are constants, no strenght reduction
      return;
    }
    target_const = op1;
    val = c->getSExtValue();
    if (val < 0) {
      is_negative = true;
      val = -val;
    }
    target_var = op0;
  }

  if (val != 1 || val != 0) { // x*0 and x*1 are algebriac identity
    reduction res = evaluate_reduction({{}, 0}, val);
    printReduction(res);
    if (res.istooexpensive) {
      return;
    }

    Instruction *latest_instruction = i;

    // we traverse it in reverse: if we have x=a-(b+c) we can say it as
    // k = c+b; x = a-k
    for (auto s = res.shifts.rbegin(); s != res.shifts.rend(); ++s) {
      /*
      for each shift:
      %shift = lshr %x value
      %add = add %shift %prev (if is not the first shift)
      (then %add becomes the new prev)

      at the end if addition=1: add %prev 1
      */

      Value *c =
          ConstantInt::get(Type::getInt32Ty(i->getFunction()->getContext()),
                           s->value); // note: type refers to integer type,
                                      // llvm context grabbed from function
      Instruction *ishift =
          BinaryOperator::Create(Instruction::LShr, target_var, c);
      ishift->insertAfter(latest_instruction);

      //"base case" is the first shift, we don't add that to anything
      // else
      if (latest_instruction != i) {
        Instruction *joininstr;
        if (s->op == ADDITION) {
          joininstr = BinaryOperator::Create(Instruction::Add, ishift,
                                             latest_instruction);
        } else if (s->op == SUBTRACTION) {
          joininstr = BinaryOperator::Create(Instruction::Sub, ishift,
                                             latest_instruction);
        }
        joininstr->insertAfter(ishift);
        latest_instruction = joininstr;
      } else {
        latest_instruction = ishift;
      }
    } // end of shifts loop

    if (res.addition == 1) {
      Value *c = ConstantInt::get(
          Type::getInt32Ty(i->getFunction()->getContext()), res.addition);
      Instruction *i =
          BinaryOperator::Create(Instruction::Add, latest_instruction, c);
      i->insertAfter(latest_instruction);
      latest_instruction = i;
    }

    i->replaceAllUsesWith(latest_instruction);
  }
}

void DivisionReduction(Instruction *i, bool issigned) {
  Value *op0 = i->getOperand(0);
  Value *op1 = i->getOperand(1);
  uint64_t val = 0;
  // strenght reduction with division is only possible if the costant is
  // op1
  if (ConstantInt *c = dyn_cast<ConstantInt>(op0)) {
    return;
  }

  if (ConstantInt *c = dyn_cast<ConstantInt>(op1)) {
    val = c->getZExtValue();
  } else {
    return;
  }

  int lg = log2(val);
  if (pow(2, lg) != val) {
    return;
  }

  Value *c =
      ConstantInt::get(Type::getInt32Ty(i->getFunction()->getContext()), lg);
  // logical cause it's unsigned division
  Instruction *newinstr;
  if (issigned) {
    newinstr = BinaryOperator::Create(Instruction::AShr, op1, c);
  } else {
    newinstr = BinaryOperator::Create(Instruction::LShr, op1, c);
  }

  i->replaceAllUsesWith(newinstr);
  newinstr->insertAfter(i);
}

struct StrenghtReduction : PassInfoMixin<StrenghtReduction> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    for (auto bbiter = F.begin(); bbiter != F.end(); ++bbiter) {
      for (auto institer = bbiter->begin(); institer != bbiter->end();
           ++institer) {

        switch (institer->getOpcode()) {
        case (Instruction::Mul):
          MultiplicationReduction(&*institer);
          break;
        case (Instruction::UDiv):
          DivisionReduction(&*institer, false);
          break;
        case (Instruction::SDiv): {
          DivisionReduction(&*institer, true);
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