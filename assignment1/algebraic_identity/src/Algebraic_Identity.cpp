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

  struct AlgebraicIdentity : PassInfoMixin<AlgebraicIdentity>{

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    for (auto bbiter = F.begin(); bbiter != F.end(); ++bbiter){
      for (auto institer = bbiter->begin(); institer != bbiter->end(); ++institer){

        switch (institer->getOpcode()){

          case Instruction::Add: { // {} for local scope or the compiler WILL complain

          Value* op0 = institer->getOperand(0);
          Value* op1 = institer->getOperand(1);

          bool is_op0_zero, is_op1_zero = false;

          if (ConstantInt *c = dyn_cast<ConstantInt>(op0)){
            if (c->isZero()){
              is_op0_zero = true;
            }
          }

          if (ConstantInt *c = dyn_cast<ConstantInt>(op1)){
            if (c->isZero()){
              is_op1_zero = true;
            }
          }


          if (is_op0_zero || is_op1_zero){
            //non-zero value. in case both operands are zero then the target will be 0..
            Value *target;
            if (is_op0_zero) {
                target = op1;
            }
            else{
                target = op0;
            }
            institer->replaceAllUsesWith(target);

		  }

          

          break;
        }

          case Instruction::Sub: {
            /*
            a - 0 -> a
            in case of 0-a we leave it at that
            */

            Value* op0 = institer->getOperand(0);
            Value* op1 = institer->getOperand(1);

            bool is_op1_zero = false;

            if (ConstantInt *c = dyn_cast<ConstantInt>(op1)){
                if (c->isZero()){
                    is_op1_zero = true;
                }
            }

                if (is_op1_zero){ //works even if both are 0
                    institer->replaceAllUsesWith(op0);
                }
                
            break;
          }
          

          case Instruction::Mul:{
            Value* op0 = institer->getOperand(0);
            Value* op1 = institer->getOperand(1);

            bool is_op0_zero, is_op1_zero = false;
            bool is_op0_one, is_op1_one = false;

            if (ConstantInt *c = dyn_cast<ConstantInt>(op0)){
                if (c->isZero()){
                    is_op0_zero = true;
                }
                else if(c->isOne()){
                    is_op0_one = true;
                }
            }

            if (ConstantInt *c = dyn_cast<ConstantInt>(op1)){
                if (c->isZero()){
                    is_op1_zero = true;
                }
                else if (c->isOne()){
                    is_op1_one = true;
                }
            }

            if (is_op0_zero || is_op1_zero){ //zero is the target
                Value* target;
                if (is_op0_zero){
                    target = op0;
                }
                else{
                    target = op1;
                }
                institer->replaceAllUsesWith(target);
            }
            else if (is_op0_one || is_op1_one){ //non-one is the target
                Value* target;
                if (is_op0_one){
                    target = op1;
                }
                else{
                    target = op0;
                }
                institer->replaceAllUsesWith(target);
            }

            break;
          }
            

          case Instruction::SDiv: { //is it correct? S for signed integer

            /*
            0/a -> 0
            a/1 -> a
            note that we do not care about a/0, the compiler should throw a warning for that
            and its not really something that can be optimized..

            we still need to check if op1 is zero to avoid 0/0 inconsistency
            */

            Value* op0 = institer->getOperand(0);
            Value* op1 = institer->getOperand(1);

            bool is_op0_zero, is_op1_zero = false;
            bool is_op1_one = false;

            if (ConstantInt *c = dyn_cast<ConstantInt>(op0)){
                if (c->isZero()){
                is_op0_zero = true;
                }
            }

            if (ConstantInt *c = dyn_cast<ConstantInt>(op1)){
                if (c->isZero()){
                  is_op1_zero = true;
                }
                else if (c->isOne()){
                  is_op1_one = true;
                }
            }

            //small optimization, in both case op0 replaces the instruction..
            if ((is_op0_zero || is_op1_one) && !is_op1_zero){
                institer->replaceAllUsesWith(op0);
            }


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

}

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