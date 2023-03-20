#include "compiler/Global.h"

using namespace llvm;

class DeadCodeEliminatePass : public FunctionPass {
private:
  static char ID;

  bool removeDeadInstructions(Function &F);

public:
  DeadCodeEliminatePass();

  bool runOnFunction(Function &F) override;
};