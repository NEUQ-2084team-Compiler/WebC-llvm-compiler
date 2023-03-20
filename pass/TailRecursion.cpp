#include "TailRecursion.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include <cstddef>
#include <llvm-11/llvm/IR/BasicBlock.h>
#include <llvm-11/llvm/IR/Instruction.h>
#include <llvm-11/llvm/IR/Instructions.h>
#include <llvm-11/llvm/IR/Value.h>
#include <llvm-11/llvm/Pass.h>
#include <llvm-11/llvm/Support/raw_ostream.h>

char TailRecursion::ID = 0;

// Pass入口函数
bool TailRecursion::runOnFunction(Function &F) {
  bool Changed = false;

  // 遍历每个基本块
  for (auto &BB : F) {
    // 获取基本块的最后一条指令
    Instruction *I = BB.getTerminator();
    // 判断是否为调用指令
    if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
      // 转化为callinst
      CallInst *CI = dyn_cast<CallInst>(I);
      Function *Callee = CI->getCalledFunction();
      if (!Callee)
        continue;
      // 判断是否为当前函数自身
      if (Callee == &F) {
        // 获取函数的最后一条指令
        Instruction *Last = F.back().getTerminator();
        // 判断最后一条指令是否为return指令
        if (!isa<ReturnInst>(Last))
          continue;

        // 获取参数列表
        Value *Arg = CI->getArgOperand(0);
        // 获取参数的类型
        Type *ArgType = Arg->getType();
        // 判断参数类型是否为整型
        if (!ArgType->isIntegerTy())
          continue;

        // 获取函数的参数列表
        Function::arg_iterator AI = F.arg_begin();
        Value *Arg1 = AI++;
        Value *Arg2 = AI++;
        // 判断参数个数是否正确
        if (AI != F.arg_end())
          continue;

        // 生成新的基本块
        BasicBlock *NewBB = BasicBlock::Create(F.getContext(), "", &F);
        // 在新的基本块中插入赋值指令
        // Value *n = new AllocaInst()
        Value *NewArg1 = new AllocaInst(ArgType, 0, "", NewBB);
        Value *NewArg2 = new AllocaInst(ArgType, 0, "", NewBB);
        new StoreInst(Arg1, NewArg1, NewBB);
        new StoreInst(Arg2, NewArg2, NewBB);

        // 获取函数的入口基本块
        BasicBlock &EntryBB = F.getEntryBlock();
        // 获取函数的第一个指令
        Instruction *First = EntryBB.getFirstNonPHI();

        // 在函数的入口基本块中插入判断和跳转指令
        ICmpInst *Cond = new ICmpInst(First, CmpInst::ICMP_EQ, Arg1,
                                      ConstantInt::get(ArgType, 1));
        BranchInst::Create(&F.back(), NewBB, Cond, First);
        // 在调用指令处插入赋值指令
        Value *OldArg1 = CI->getArgOperand(0);
        Value *OldArg2 = CI->getArgOperand(1);
        new StoreInst(OldArg1, NewArg1, CI->getNextNode());
        new StoreInst(OldArg2, NewArg2, CI->getNextNode());

        // 修改调用指令的参数
        CI->setArgOperand(0, NewArg1);
        // CI->setArgOperand(1,
        //                   new BinaryOperator(Instruction::Mul, NewArg2, OldArg1,
        //                                      "", CI->getNextNode(), false));

        // 删除原先的return指令
        Last->eraseFromParent();

        Changed = true;
      }
    }
  }

  return Changed;
}

TailRecursion::TailRecursion() : FunctionPass(ID) {}