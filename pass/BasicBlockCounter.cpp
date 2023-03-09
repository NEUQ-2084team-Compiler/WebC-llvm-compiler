#include "BasicBlockCounter.h"

char BasicBlockCounter::ID = 0;

bool BasicBlockCounter::runOnFunction(Function &F) {
  unsigned int bbCount = 0;
  for (auto &BB : F) {
    bbCount++;
  }
  errs() << "Function " << F.getName() << " has " << bbCount
         << " basic blocks.\n";
  return false;
}

BasicBlockCounter::BasicBlockCounter() : FunctionPass(ID) {}