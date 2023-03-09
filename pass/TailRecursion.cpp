#include "TailRecursion.h"
#include "llvm/IR/Instructions.h"
#include <llvm-11/llvm/IR/Instruction.h>
#include <llvm-11/llvm/Pass.h>

char TailRecursion::ID = 0;

bool TailRecursion::runOnFunction(Function &F) {
  bool changed = false;
  for (auto &BB : F) {
    Instruction *inst = BB.getTerminator();
    CallInst *call = dyn_cast<CallInst>(inst);
    if (!call||!call->isTailCall()) {
      continue;
    }
    Function *callee = call->getCalledFunction();
    if (!callee) {
      continue;
    }
    if (callee == &F) {
      changed = true;
      //todo
    }
  }
  return false;
}

TailRecursion::TailRecursion() : FunctionPass(ID) {}    