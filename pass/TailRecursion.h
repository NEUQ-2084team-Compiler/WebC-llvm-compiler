#include "compiler/Global.h"
// #include <llvm-11/llvm/IR/Function.h>
// #include <llvm-11/llvm/Pass.h>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

class TailRecursion : public FunctionPass {
  static char ID;

public:
  TailRecursion();

  bool runOnFunction(Function &F) override;
};