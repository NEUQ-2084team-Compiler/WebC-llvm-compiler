#include "DeadCodeEliminatePass.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include <llvm-11/llvm/ADT/SmallPtrSet.h>
#include <llvm-11/llvm/IR/Use.h>
#include <llvm-11/llvm/IR/Value.h>
#include <llvm-11/llvm/Support/Casting.h>
#include <llvm-11/llvm/Support/raw_ostream.h>

char DeadCodeEliminatePass::ID = 0;

DeadCodeEliminatePass::DeadCodeEliminatePass() : FunctionPass(ID) {}

bool DeadCodeEliminatePass::runOnFunction(Function &F) {
  bool changed = false;
  // 决定 Pass 不会对哪些函数生效
  if (skipFunction(F)) {
    changed = true;
  }
  return removeDeadInstructions(F);
}

bool DeadCodeEliminatePass::removeDeadInstructions(Function &F) {
  // 该局部变量用于存储被判定为 Live 的 LLVM IR 指令
  SmallPtrSet<Instruction *, 32> Alive;
  /*该局部变量作为工作集，用于存储需要对哪些 LLVM IR 指令进行是否为
  Live的判定。当工作集为空时，表示已经完成了判定工作。*/
  SmallVector<Instruction *, 128> Worklist;

  // 收集Live指令
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (I.isSafeToRemove()) {
        Alive.insert(&I);
        Worklist.push_back(&I);
      }
    }
  }
  errs() << "收集Live指令\n";

  while (!Worklist.empty()) {
    auto *LiveInst = Worklist.pop_back_val();
    for (Use &OI : LiveInst->operands()) {
      if (Instruction *Inst = dyn_cast<Instruction>(OI)) {
        if (Alive.insert(Inst).second) {
          Worklist.push_back(Inst);
        }
      }
    }
  }
  errs() << "收集Dead指令\n";

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (!Alive.count(&I)) {
        errs() << I << "\n";
        Worklist.push_back(&I);
        I.dropAllReferences();
      }
    }
  }
  errs() << "删除Dead指令\n";

  for (Instruction *I : Worklist) {
    I->eraseFromParent();
  }

  errs() << "删除Dead指令完成\n";

  return !Worklist.empty();
}