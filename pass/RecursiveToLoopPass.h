//
// Created by bytedance on 2021/2/26.
//

#ifndef SYSYPLUS_COMPILER_RECURSIVETOLOOPPASS_H
#define SYSYPLUS_COMPILER_RECURSIVETOLOOPPASS_H

#include "compiler/Global.h"

using namespace llvm;

class RecursiveToLoopPass : public FunctionPass {
    static char ID;
public:
    RecursiveToLoopPass();

    bool runOnFunction(Function &F) override;
};


#endif //SYSYPLUS_COMPILER_RECURSIVETOLOOPPASS_H
