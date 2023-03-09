#include "compiler/Global.h"

using namespace llvm;

class BasicBlockCounter : public FunctionPass {
  static char ID;

public:
  BasicBlockCounter();

  bool runOnFunction(Function &F) override;
};