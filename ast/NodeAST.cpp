#include "NodeAST.hpp"

#include "codegen/CodeGen.h"
#include "compiler/ErrHelper.h"
#include "compiler/stat.h"
#include <iostream>
#include <utility>

long StringExprAST::id = 0;

/**
 * 将右转左类型
 * 强制类型转换
 * @param l
 * @param r
 * @return
 */
llvm::Value *makeMatch(Value *l, Value *r, NodeAST *node = NIL,
                       bool left_is_local_mem = false) {
  if (l == NIL || r == NIL) {
    ABORT_COMPILE;
    return NIL;
  }
  auto this_type = l->getType();
  // 如果左侧Value是局部变量的内存地址，则获取其所指向的类型。
  if (left_is_local_mem) {
    this_type = l->getType()->getContainedType(0);
  }
  if (r->getType() != this_type) {
    // 不同，尝试进行修剪
    if (r->getType()->isIntegerTy() && this_type->isDoubleTy()) {
      // CreateSIToFP函数的作用是将一个有符号整数类型转换为浮点数类型
      r = Builder->CreateSIToFP(r, this_type);
    } else if (r->getType()->isDoubleTy() && this_type->isIntegerTy()) {
      // CreateFPToSI函数的作用是将一个浮点数类型转换为有符号整数类型
      r = Builder->CreateFPToSI(r, this_type);
    } else if (r->getType()->isIntOrIntVectorTy() &&
               this_type->isIntOrIntVectorTy()) {
      // 即使左侧和右侧的值都是整数类型，它们的类型也可能不同。例如，左侧的值可能是i32类型，而右侧的值可能是i64类型
      // CreateZExtOrTrunc函数的作用是将一个整数类型进行零扩展或截断操作，将其转换为另一种整数类型
      r = Builder->CreateZExtOrTrunc(r, this_type);
    }
  }
  return r;
}

llvm::Value *DoubleExprAST::codegen() {
  // 在LLVM IR中，数字常量用ConstantFP类表示
  // ，它在APFloat内部保存数值（APFloat能够保持任意精度的浮点常量）
  // AP = Aribitrary Precision
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

string DoubleExprAST::toString() { return to_string(Val); }

llvm::Value *VariableExprAST::codegen() {
  // 从符号表中取出
  llvm::Value *v = FINDLOCAL(Name);
  if (!v) {
    ABORT_COMPILE;
    LogErrorV((std::string("未知变量名:") + Name).c_str());
  }
  return v;
}

llvm::Value *BinaryExprAST::codegen() {
  // 逻辑值bool
  if (type == BinaryType::AND) {
    // 与
    return this->codeGenAnd(LEA, REA);
  } else if (type == BinaryType::OR) {
    // 或
    return this->codeGenOr(LEA, REA);
  } else {
    llvm::Value *L = LEA->codegen();
    llvm::Value *R = REA->codegen();
    if (!L || !R) {
      ABORT_COMPILE;
      return LogErrorV("二元表达式解析失败");
    }
    // 数值计算 首先判断两个操作数的类型是否相同
    if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      auto l_width = dyn_cast<IntegerType>(L->getType())->getBitWidth();
      auto r_width = dyn_cast<IntegerType>(R->getType())->getBitWidth();
      // 向位数高位扩展
      if (l_width > r_width) {
        R = Builder->CreateZExtOrTrunc(R, L->getType());
      } else if (l_width < r_width) {
        L = Builder->CreateZExtOrTrunc(L, R->getType());
      }
      switch (type) {
        // "add"是这个值的名称，它在生成的IR中将用作标识符
      case BinaryType::add:
        return Builder->CreateAdd(L, R, "add");
      case BinaryType::sub:
        return Builder->CreateSub(L, R, "sub");
      case BinaryType::mul:
        return Builder->CreateMul(L, R, "mul");
      case BinaryType::divi:
        return Builder->CreateSDiv(L, R, "divi");
      case BinaryType::mod:
        return Builder->CreateSRem(L, R, "mod");
      case BinaryType::less:
        // 整数比较指令 Create Integer Compare for Signed Less Than
        return Builder->CreateICmpSLT(L, R, "less_than");
      case BinaryType::greater:
        return Builder->CreateICmpSGT(L, R, "greater_than");
      case BinaryType::equ:
        return Builder->CreateICmpEQ(L, R, "equ");
      case BinaryType::greater_equ:
        return Builder->CreateICmpSGE(L, R, "greater_equ");
      case BinaryType::less_equ:
        return Builder->CreateICmpSLE(L, R, "less_equ");
      case BinaryType::n_equ:
        return Builder->CreateICmpNE(L, R, "not_equ");
      default:
        ABORT_COMPILE;
        return LogErrorV(("invalid binary operator"));
      }
    } else {
      // 类型不相同，做类型转换
      if (L->getType()->isIntegerTy()) {
        // Create Signed Integer to Floating Point
        L = Builder->CreateSIToFP(L, R->getType());
      }
      if (R->getType()->isIntegerTy()) {
        R = Builder->CreateSIToFP(R, L->getType());
      }
      switch (type) {
      case BinaryType::add:
        return Builder->CreateFAdd(L, R, "add");
      case BinaryType::sub:
        return Builder->CreateFSub(L, R, "sub");
      case BinaryType::mul:
        return Builder->CreateFMul(L, R, "mul");
      case BinaryType::divi:
        return Builder->CreateFDiv(L, R, "divi");
      case BinaryType::mod:
        return Builder->CreateFRem(L, R, "mod");
      case BinaryType::less:
        return Builder->CreateFCmpOLT(L, R, "less_than");
      case BinaryType::equ:
        return Builder->CreateFCmpOEQ(L, R, "ordered_fequ");
      case BinaryType::greater:
        return Builder->CreateFCmpOGT(L, R, "greater_than");
      case BinaryType::greater_equ:
        return Builder->CreateFCmpOGE(L, R, "greater_equ");
      case BinaryType::less_equ:
        return Builder->CreateFCmpOLE(L, R, "less_equ");
      case BinaryType::n_equ:
        return Builder->CreateFCmpONE(L, R, "not_equ");
      default:
        ABORT_COMPILE;
        return LogErrorV(("invalid binary operator"));
      }
    }
  }
  ABORT_COMPILE;
  return NIL;
}

BinaryExprAST::BinaryExprAST(BinaryType type, ExpressionAST *lea,
                             ExpressionAST *rea)
    : type(type), LEA(lea), REA(rea) {}

Value *BinaryExprAST::codeGenAnd(NodeAST *l, NodeAST *r) {
  if (Builder->GetInsertBlock() == NIL) {
    ABORT_COMPILE;
    return LogErrorV("当前需要一个Basic Block来开始and环境");
  }
  // Builder->GetInsertBlock()：获取Builder当前所在的基本块（basic block）
  // ->getParent()：获取当前基本块所属的函数（Function）
  auto func = Builder->GetInsertBlock()->getParent();
  if (func == NIL) {
    ABORT_COMPILE;
    return LogErrorV("&&需要在函数环境下使用，建议在函数中使用");
  }
  auto bbCond1 = Builder->GetInsertBlock();
  auto bbCond2 = BasicBlock::Create(*TheContext, "and_right");
  auto bbCondEnd = BasicBlock::Create(*TheContext, "and_end");

  auto cond1V = l->codegen();
  cond1V =
      Builder->CreateICmpNE(cond1V, ConstantInt::get(cond1V->getType(), 0));
  bbCond1 = Builder->GetInsertBlock();
  Builder->CreateCondBr(cond1V, bbCond2,
                        bbCondEnd); // 如果为true的话接着判断cond2，
  func->getBasicBlockList().push_back(bbCond2);
  Builder->SetInsertPoint(bbCond2);
  auto cond2V = r->codegen();
  cond2V =
      Builder->CreateICmpNE(cond2V, ConstantInt::get(cond2V->getType(), 0));
  // 要考虑嵌套
  bbCond2 = Builder->GetInsertBlock();
  Builder->CreateBr(bbCondEnd);
  func->getBasicBlockList().push_back(bbCondEnd);
  /*为左操作数和右操作数各自创建一个基本块（basic block），分别为 bbCond1
    和bbCond2。然后根据左操作数的结果，决定跳转到 bbCond2
    或bbCondEnd。如果跳转到 bbCond2，则继续判断右操作数；否则直接跳转到
    bbCondEnd。在 bbCond2
    中，会生成右操作数的代码，并根据其结果，决定跳转到bbCondEnd。最后，在
    bbCondEnd 中使用 PHI 节点（phi
    node）来合并左右操作数的结果，以便后续代码使用。*/
  Builder->SetInsertPoint(bbCondEnd);
  // 参数 Ty 指定了 Phi 节点的类型，这里是 "bool"。参数 NumReservedValues
  // 指定了该 Phi 节点要合并多少个基本块中的值，这里是 2
  auto phi = Builder->CreatePHI(getTypeFromStr("bool"), 2);
  phi->addIncoming(cond1V, bbCond1);
  phi->addIncoming(cond2V, bbCond2);
  // 如果从bbCond1流入phi节点，说明是false，如果从bbCond2流入phi节点，cond2V就是这个cond表达式的实际值
  return phi;
}

Value *BinaryExprAST::codeGenOr(NodeAST *l, NodeAST *r) {
  if (Builder->GetInsertBlock() == NIL) {
    ABORT_COMPILE;
    return LogErrorV("当前需要一个Basic Block来开始or环境");
  }
  auto func = Builder->GetInsertBlock()->getParent();
  if (func == NIL) {
    ABORT_COMPILE;
    return LogErrorV("||需要在函数环境下使用");
  }

  // 获取当前builder所在的基本块
  auto bbCond1 = Builder->GetInsertBlock();
  auto bbCond2 = BasicBlock::Create(*TheContext, "or_right");
  auto bbCondEnd = BasicBlock::Create(*TheContext, "or_end");
  // TODO
  Builder->SetInsertPoint(bbCond1);
  auto cond1V = l->codegen();
  cond1V =
      Builder->CreateICmpNE(cond1V, ConstantInt::get(cond1V->getType(), 0));
  bbCond1 = Builder->GetInsertBlock();
  Builder->CreateCondBr(cond1V, bbCondEnd, bbCond2);

  func->getBasicBlockList().push_back(bbCond2);
  Builder->SetInsertPoint(bbCond2);
  auto cond2V = r->codegen();
  cond2V =
      Builder->CreateICmpNE(cond2V, ConstantInt::get(cond2V->getType(), 0));
  // 要考虑嵌套，更新当前bb
  bbCond2 = Builder->GetInsertBlock();
  // CreateBr是LLVM
  // IR中的一个函数，表示创建一个无条件跳转指令，将控制权转移到指定的基本块。
  Builder->CreateBr(bbCondEnd);
  func->getBasicBlockList().push_back(bbCondEnd);
  Builder->SetInsertPoint(bbCondEnd);
  auto phi = Builder->CreatePHI(getTypeFromStr("bool"), 2);

  phi->addIncoming(cond1V, bbCond1);
  phi->addIncoming(cond2V, bbCond2);
  return phi;
}

string BinaryExprAST::toString() {
  return string("二元操作结点：") + LEA->toString() + "," + REA->toString();
}

void BinaryExprAST::initChildrenLayers() {
  // 用于初始化子节点的层数
  NodeAST::initChildrenLayers();
  this->LEA->setLayer(getLayer() + 1);
  this->REA->setLayer(getLayer() + 1);
  // 更新最大的层数
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->LEA->initChildrenLayers();
  this->REA->initChildrenLayers();
}

llvm::Value *CallExprAST::codegen() {
  // Look up the name in the global module table.
  llvm::Function *func = TheModule->getFunction(callName);
  std::vector<llvm::Value *> argsV;
  for (unsigned i = 0, e = args.size(); i != e; ++i) {
    Value *av = args[i]->codegen();
    if (av == NIL) {
      ABORT_COMPILE;
      return LogErrorV(("函数实参解析失败：" + args[i]->toString()).c_str());
    }
    argsV.push_back(av);
  }
  // epxr: ident
  if (func == NIL) {
    /* 在局部变量中查找和callName相符的函数指针变量
     如果找到了函数指针变量，那么就可以通过 Builder->CreateLoad 生成一个 load
     指令来加载函数指针变量的值，然后通过 Builder->CreateCall 来进行函数调用。*/
    auto call_func_mem = FINDLOCAL(callName);
    if (call_func_mem != NIL) {
      auto call_mem = Builder->CreateLoad(call_func_mem);
      // 是指向函数类型的指针类型
      // getPointerElementType()获取指针指向的类型
      if (call_mem->getType()->isPointerTy() &&
          call_mem->getType()->getPointerElementType()->getTypeID() ==
              llvm::Type::FunctionTyID) {
        auto function_type = dyn_cast<FunctionType>(
            call_mem->getType()->getPointerElementType());
        return Builder->CreateCall(function_type, call_mem, argsV);
        ;
      }
    }
  }

  if (func == NIL) {
    // 从外部函数中查找 很重要
    auto ev = ExternFunctionLinker::tryHandleFuncCall(*TheContext, *TheModule,
                                                      callName, &argsV);
    // Extern直接处理，就直接返回
    if (ev) {
      return ev;
    }
  }

  if (!func && LOCALSVARS.find(callName) == LOCALSVARS.end()) {
    ABORT_COMPILE;
    return LogErrorV(("使用了未知的函数：" + callName).c_str());
  }
  // If argument mismatch error.
  if (func->arg_size() != args.size()) {
    ABORT_COMPILE;
    return LogErrorV("Incorrect # arguments passed");
  }
  return Builder->CreateCall(func, argsV);
}

string CallExprAST::toString() { return "函数调用：" + callName; }

const string &CallExprAST::getCallName() const { return callName; }

void CallExprAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  if (this->args.empty()) {
    UPDATE_MAX_LAYERS(getLayer() + 1);
  }
  for (const auto &item : this->args) {
    item->setLayer(getLayer() + 1);
    item->initChildrenLayers();
  }
}

llvm::Function *PrototypeAST::codegen() {
  // 形参的类型 int*()
  // 函数类型为int, 无参
  auto type = FunctionType::get(getTypeFromStr("int"), false);

  // Make the function type:  double(double,double) etc.
  std::vector<llvm::Type *> Args;
  for (auto arg : args) {
    // 数组类型
    if (typeid(*arg) == typeid(VariableArrDeclarationAST)) {
      auto arr_arg = dynamic_cast<VariableArrDeclarationAST *>(arg);
      // buildPointerTy()得到指向数组首元素的指针类型
      Args.push_back(arr_arg->buildPointerTy());
    } else if (typeid(*arg) ==
               typeid(FuncPtrAST)) { // 如果这种类型是FunctrAST的话
      auto ptr_arg = dynamic_cast<FuncPtrAST *>(arg);

      std::vector<llvm::Type *> putArgs;
      /*
      从第二个参数开始遍历，并将每个参数的类型（使用标识符字符串来表示）转换为
      LLVM IR 中的类型，并添加到 putArgs 中*/
      for (int i = 1; i < ptr_arg->args.size(); i++) {
        putArgs.push_back(getTypeFromStr(ptr_arg->args[i]->identifier));
      }
      /*
      llvm::ArrayRef 是 LLVM 中的一个类，表示一个指向数组的指针及其长度。它是
      LLVM 中表示数组的常用方式之一。在这里，argsRef 是一个指向 putArgs
      中元素的指针，它表示了函数指针类型的参数列表。*/
      llvm::ArrayRef<llvm::Type *> argsRef(putArgs);
      llvm::FunctionType *funcPtrType = llvm::FunctionType::get(
          getTypeFromStr(ptr_arg->args[0]->identifier), argsRef, false);
      // getPointerTo() 是 LLVM IR 中
      // Type类型的一个成员函数，用于返回指向该类型的指针类型
      Args.push_back(funcPtrType->getPointerTo());
    } else {
      Args.push_back(getTypeFromStr(arg->type));
    }
  }
  llvm::FunctionType *FT =
      llvm::FunctionType::get(getTypeFromStr(returnType), Args, false);
  ;
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, getName(), TheModule.get());

  // Set names for all arguments.
  unsigned Idx = 0;
  for (auto &Arg : F->args()) {
    Arg.setName(args[Idx++]->getName());
  }
  return F;
}

Value *ConditionAST::codegen() {
  webc::parser::condition_num++;
  // 解析if表达式
  Value *ifCondV = if_cond->codegen();
  if (!ifCondV) {
    ABORT_COMPILE;
    return LogErrorV("if parse error");
  }
  // 创建一条 icnp ne指令，用于if的比较，由于是bool比较，此处直接与0比较。
  if (ifCondV->getType()->isIntegerTy()) {
    ifCondV = Builder->CreateICmpNE(
        ifCondV, ConstantInt::get(ifCondV->getType(), 0), "ifcond_integer");
  } else if (ifCondV->getType()->isDoubleTy()) {
    ifCondV = Builder->CreateFCmpUNE(
        ifCondV, ConstantFP::get(ifCondV->getType(), 0), "ifcond_double");
  }
  // 获取if里面的函数体，准备创建基础块
  auto theFunction = Builder->GetInsertBlock()->getParent();
  if (!theFunction) {
    ABORT_COMPILE;
    return LogErrorV("if没在函数体内使用");
  }
  TheCodeGenContext->push_block(Builder->GetInsertBlock());
  // 创建三个basic block，先添加IfTrue至函数体里面
  BasicBlock *bbIfTrue =
      BasicBlock::Create(*TheContext, "neuq_jintao_if_true", theFunction);
  BasicBlock *bbElse =
      else_stmt == NIL ? NIL
                       : BasicBlock::Create(*TheContext, "neuq_jintao_else");
  BasicBlock *bbIfEnd = BasicBlock::Create(*TheContext, "neuq_jintao_if_end");
  LOCALS->setContextType(CodeGenBlockContextType::IF);
  LOCALS->context.ifCodeGenBlockContext = new IfCodeGenBlockContext(
      Builder->GetInsertBlock(), bbIfEnd, bbIfTrue, bbElse);
  // 创建条件分支,createCondBr(条件结果，为true的basic block，为false的basic
  // block)
  if (bbElse == NIL) {
    Builder->CreateCondBr(ifCondV, bbIfTrue, bbIfEnd);
  } else {
    Builder->CreateCondBr(ifCondV, bbIfTrue, bbElse);
  }
  // 开启往Basic Block中塞入代码
  Builder->SetInsertPoint(bbIfTrue);
  Value *ifTrueV = if_stmt->codegen();
  if (!ifTrueV) {
    ABORT_COMPILE;
    return LogErrorV("if里面的内容有误");
  }
  // if
  // 中为true的代码写完了，将if跳转到else后面，也就是IfEnd结点。这符合逻辑，同时也是LLVM的强制要求，每个Basic
  // Block必须以跳转结尾
  if (Builder->GetInsertBlock()->empty() ||
      !(*Builder->GetInsertBlock()->rbegin()).isTerminator()) {
    Builder->CreateBr(bbIfEnd);
  }
  // 将插入if内容时，可能会有循环嵌套，需要更新嵌套最底层的insertBlock
  bbIfTrue = Builder->GetInsertBlock();
  // 塞入else
  if (bbElse != NIL) {
    theFunction->getBasicBlockList().push_back(bbElse);
    Builder->SetInsertPoint(bbElse);
    Value *elseV = nullptr;
    if (else_stmt) {
      elseV = else_stmt->codegen();
      if (!elseV) {
        ABORT_COMPILE;
        return LogErrorV("else里面内容有误");
      }
    }
    if (Builder->GetInsertBlock()->empty() ||
        !(*Builder->GetInsertBlock()->rbegin()).isTerminator()) {
      Builder->CreateBr(bbIfEnd);
    }
    bbElse = Builder->GetInsertBlock();
  }
  // 创建末尾结点
  theFunction->getBasicBlockList().push_back(bbIfEnd);
  Builder->SetInsertPoint(bbIfEnd);
  TheCodeGenContext->pop_block();
  return bbIfEnd;
}

ConditionAST::ConditionAST(ExpressionAST *ifCond, BlockAST *ifStmt,
                           BlockAST *elseStmt)
    : if_cond(ifCond), if_stmt(ifStmt), else_stmt(elseStmt) {
  webc::parser::condition_num++;
}

string ConditionAST::toString() { return "条件结点"; }

void ConditionAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  UPDATE_MAX_LAYERS(getLayer() + 1);
  if_cond->setLayer(getLayer() + 1);
  if_cond->initChildrenLayers();
  if (this->else_stmt != NIL) {
    this->else_stmt->setLayer(getLayer() + 1);
    this->else_stmt->initChildrenLayers();
  }
  this->if_stmt->setLayer(getLayer() + 1);
  this->if_stmt->initChildrenLayers();
}

Value *ForExprAST::codegen() {
  Function *theFuntion = Builder->GetInsertBlock()->getParent();
  if (theFuntion == nullptr) {
    ABORT_COMPILE;
    return LogErrorV("for 需要在函数语境下使用");
  }
  // 先执行for(xxx;;)
  auto tmpForBlock =
      BasicBlock::Create(*TheContext, "bb_for_start", theFuntion);
  TheCodeGenContext->push_block(tmpForBlock);

  auto bbStart = tmpForBlock;
  auto bbCond = BasicBlock::Create(*TheContext, "bb_for_cond", theFuntion);
  auto bbStep = BasicBlock::Create(*TheContext, "bb_for_step", theFuntion);
  auto bbBody = BasicBlock::Create(*TheContext, "bb_for_body", theFuntion);
  auto bbEndFor = BasicBlock::Create(*TheContext, "bb_for_end");
  LOCALS->setContextType(CodeGenBlockContextType::FOR);
  LOCALS->context.forCodeGenBlockContext =
      new ForCodeGenBlockContext(bbStart, bbCond, bbStep, bbBody, bbEndFor);
  Builder->CreateBr(bbStart);
  Builder->SetInsertPoint(bbStart);
  auto startV = Start->codegen();
  if (startV == nullptr) {
    ABORT_COMPILE;
    return LogErrorV("for(x;;){}中x有误");
  }
  Builder->CreateBr(bbCond);

  Builder->SetInsertPoint(bbCond);
  auto condV = Cond->codegen();
  if (condV == nullptr) {
    ABORT_COMPILE;
    return LogErrorV("for(;x;){}中x有误");
  }

  condV = Builder->CreateICmpNE(
      condV, ConstantInt::get(getTypeFromStr("bool"), 0), "neuq_jintao_ifcond");
  Builder->CreateCondBr(condV, bbBody, bbEndFor);

  Builder->SetInsertPoint(bbBody);
  auto body = Body->codegen();
  if (body == nullptr) {
    ABORT_COMPILE;
    return LogErrorV("for执行内容有误");
  }
  auto ins = Builder->GetInsertBlock()->rbegin();
  if (ins == Builder->GetInsertBlock()->rend() || !(*ins).isTerminator()) {
    Builder->CreateBr(bbStep);
  }
  Builder->SetInsertPoint(bbStep);
  auto stepV = Step->codegen();
  if (stepV == nullptr) {
    ABORT_COMPILE;
    return LogErrorV("for(;;x){}中x有误");
  }
  Builder->CreateBr(bbCond);
  // 将循环结束后的基本块 bbEndFor
  // 插入到当前函数的基本块列表中，以确保该基本块能够被正确地执行
  theFuntion->getBasicBlockList().push_back(bbEndFor);
  Builder->SetInsertPoint(bbEndFor);
  TheCodeGenContext->pop_block();
  return bbEndFor;
}

ForExprAST::ForExprAST(NodeAST *start, NodeAST *end, NodeAST *step,
                       BlockAST *body)
    : Start(start), Cond(end), Step(step), Body(body) {
  webc::parser::loop_num++;
}

string ForExprAST::toString() { return "循环结点"; }

void ForExprAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  this->Body->setLayer(getLayer() + 1);
  this->Cond->setLayer(getLayer() + 1);
  this->Start->setLayer(getLayer() + 1);
  this->Step->setLayer(getLayer() + 1);
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->Body->initChildrenLayers();
  this->Cond->initChildrenLayers();
  this->Start->initChildrenLayers();
  this->Step->initChildrenLayers();
}

const std::string &PrototypeAST::getName() const { return name; }

PrototypeAST::PrototypeAST(string returnType, string name,
                           const vector<VariableDeclarationAST *> &args)
    : returnType(std::move(returnType)), name(std::move(name)), args(args) {
  webc::parser::func_params_num += this->args.size();
}

string PrototypeAST::toString() {
  return "函数描述结点：" + this->getName() + " ret:" + returnType +
         " params:" + to_string(args.size());
}

void PrototypeAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  if (!this->args.empty()) {
    UPDATE_MAX_LAYERS(getLayer() + 1);
  }
  for (const auto &item : this->args) {
    item->setLayer(getLayer() + 1);
    item->initChildrenLayers();
  }
}

void FunctionAST::cleanCodeGenContext() {
  // 清除
  TheCodeGenContext->setRetBb(NIL);
  TheCodeGenContext->pop_block();
  TheCodeGenContext->removeFunction();
}

llvm::Function *FunctionAST::codegen() {
  auto name = Proto->getName();
  llvm::Function *function = TheModule->getFunction(name);
  // 设置返回值block
  auto retBB = BasicBlock::Create(*TheContext, "function_ret");
  TheCodeGenContext->setRetBb(retBB);

  if (!function)
    function = Proto->codegen();

  if (!function) {
    ABORT_COMPILE;
    return nullptr;
  }
  // 如果函数不为空，则表示已定义过了
  if (!function->empty())
    return (llvm::Function *)LogErrorV("Function cannot be redefined.");

  // Create a new basic block to start insertion into.
  llvm::BasicBlock *BB =
      llvm::BasicBlock::Create(*TheContext, "alloca", function);
  Builder->SetInsertPoint(BB);
  TheCodeGenContext->push_block(BB);
  TheCodeGenContext->setFunction(function);

  // Record the function argnts in the NamedValues map.
  //    auto funcVars = TheCodeGenContext->get_current_locals()->localVars;
  for (auto &Arg : function->args()) {
    // 分配一片内存
    auto mem = Builder->CreateAlloca(Arg.getType());
    Builder->CreateStore(&Arg, mem);
    // 将该参数的名称和内存地址存储到局部变量映射表中
    LOCALSVARS.insert(make_pair(Arg.getName().data(), mem));
  }
  auto bb_entry = BasicBlock::Create(*TheContext, "neuq_entry", function);
  Builder->CreateBr(bb_entry);
  Builder->SetInsertPoint(bb_entry);
  // 把return值alloc一下，如果不是void
  if (!function->getReturnType()->isVoidTy()) {
    Value *ret = Builder->CreateAlloca(function->getReturnType());
    /*
    如果函数不是void类型，为返回值分配内存，并将空值(Constant::getNullValue)存储在该内存位置上。
将刚刚分配的返回值指针存储在CodeGenContext中的返回值变量(TheCodeGenContext->setRetV)中，以便在函数的其他地方使用。*/
    Builder->CreateStore(Constant::getNullValue(function->getReturnType()),
                         ret);
    TheCodeGenContext->setRetV(ret);
  }
  if (Body->codegen()) {
    // 结束函数，创建return结点

    /*首先判断当前基本块是否为空或者最后一个指令是否为终止指令（如 branch 或
ret），如果不是，则在当前基本块末尾创建一个无条件分支跳转到返回值块（retBB）。
将返回值块（retBB）添加到当前函数的基本块列表中，并将当前插入点（InsertPoint）设置为返回值块。
如果当前函数返回值类型为 void，则在返回值块中创建一个返回 void 的指令
CreateRetVoid。 如果当前函数返回值类型不为 void，则先用 CreateLoad
从返回值的指针（RetV）中读取返回值，然后在返回值块中创建一个返回指定值的指令
CreateRet。*/
    if (Builder->GetInsertBlock()->empty() ||
        !Builder->GetInsertBlock()->rbegin()->isTerminator()) {
      /*判断当前代码生成器（Builder）的指令流是否已经以终止指令（terminator
       * instruction）结尾，如果没有，则创建一个跳转指令（branch
       * instruction）到返回值块（retBB）。因为在LLVM中，每个基本块（basic
       * block）必须以终止指令结尾，这样才能保证代码在执行时正确的跳转和返回。*/
      Builder->CreateBr(retBB);
    }
    function->getBasicBlockList().push_back(retBB);
    Builder->SetInsertPoint(retBB);
    if (function->getReturnType()->isVoidTy()) {
      Builder->CreateRetVoid();
    } else {
      Builder->CreateRet(Builder->CreateLoad(TheCodeGenContext->getRetV()));
    }
    auto isNotValid = verifyFunction(*function, &errs());
    if (isNotValid) {
      LogError(("function '" + Proto->getName() + "' not valid\n").c_str());
      errs() << "function ll codes:\n";
      function->print(errs());
      ABORT_COMPILE;
      goto clean;
    } else {
#ifdef DEBUG_FLAG
      function->print(outs());
#endif
      cleanCodeGenContext();
    }
    return function;
  }
clean:
  cleanCodeGenContext();
  // Error reading body, remove function.
  function->eraseFromParent();
  return nullptr;
}

FunctionAST::FunctionAST(PrototypeAST *proto, BlockAST *body)
    : Proto(proto), Body(body) {
  webc::parser::func_num++;
}

string FunctionAST::toString() {
  return "函数结点：\n" + Proto->toString() + "\n" + Body->toString();
}

void FunctionAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  this->Proto->setLayer(getLayer() + 1);
  this->Body->setLayer(getLayer() + 1);
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->Proto->initChildrenLayers();
  this->Body->initChildrenLayers();
}

Value *BlockAST::codegen() {
  auto func = TheCodeGenContext->getFunc();
  if (func == NIL) {
    auto iterator = statements.begin();
    // 最后一个语句的值
    Value *lastStatementValue;
    for (; iterator != statements.end(); iterator++) {
      if (IS_COMPILE_ABORTED) {
        break;
      }
      auto it = (*iterator);
      if (typeid(*it) == typeid(OutStmtAST)) {
        if (iterator + 1 != statements.end()) {
          LogWarn("警告：作用域内break后方的语句无法到达");
        }
        break;
      } else if (typeid(*it) == typeid(ContinueStmtAST)) {
        if (iterator + 1 != statements.end()) {
          LogWarn("警告：作用域内continue后方的语句无法到达");
        }
        break;
      }
      lastStatementValue = it->codegen();
#ifdef DEBUG_FLAG
      if (lastStatementValue != NIL) {
        lastStatementValue->print(outs());
      }
#endif
    }
    return lastStatementValue;
  } else {
    auto bb = BasicBlock::Create(*TheContext, "blk");
    Builder->CreateBr(bb);
    func->getBasicBlockList().push_back(bb);
    Builder->SetInsertPoint(bb);
    auto iterator = statements.begin();
    Value *lastStatementValue;
    for (; iterator != statements.end(); iterator++) {
      if (IS_COMPILE_ABORTED) {
        break;
      }
      auto it = (*iterator);
      lastStatementValue = (*iterator)->codegen();
#ifdef DEBUG_FLAG
      if (lastStatementValue != NIL) {
        //                lastStatementValue->print(outs());
      }
#endif
      if (typeid(*it) == typeid(OutStmtAST)) {
        if (iterator + 1 != statements.end()) {
          LogWarn("警告：作用域内break后方的语句无法到达");
        }
        break;
      } else if (typeid(*it) == typeid(ContinueStmtAST)) {
        if (iterator + 1 != statements.end()) {
          LogWarn("警告：作用域内continue后方的语句无法到达");
        }
        break;
      }
    }
    return lastStatementValue;
  }
}

Value *ExpressionStatementAST::codegen() { return expr->codegen(); }

ExpressionStatementAST::ExpressionStatementAST(ExpressionAST *expr)
    : expr(expr) {}

VariableAssignmentAST::VariableAssignmentAST(const string &identifier,
                                             ExpressionAST *expr)
    : identifier(identifier), expr(expr) {}

llvm::Value *VariableAssignmentAST::codegen() {
  Value *result = nullptr;
  auto ori_v = FINDLOCAL(identifier);
  if (ori_v != nullptr) {
    auto v = expr == nullptr ? nullptr : expr->codegen();
    v = makeMatch(ori_v, v, this, true);
    result = Builder->CreateStore(v, ori_v);
  } else {
    auto gv = TheModule->getNamedGlobal(identifier);
    if (gv) {
      auto v = makeMatch(ori_v, gv, this);
      // 全局变量创建时必须初始化
      result = Builder->CreateStore(expr->codegen(), v);
    } else {
      // 局部和全局都找不到了
      ABORT_COMPILE;
      return LogErrorV((identifier + " is not defined!").c_str());
    }
  }
  return result;
}

string VariableAssignmentAST::toString() { return StatementAST::toString(); }

void VariableAssignmentAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  this->expr->setLayer(getLayer() + 1);
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->expr->initChildrenLayers();
}

llvm::Value *VariableDeclarationAST::codegen() {
  Value *ret = nullptr;
  if (this->getName() == "ctotal") {
    ret = NIL;
  }
  if (!HASLOCALS) {
    // 全局变量
    auto global_variable = TheModule->getNamedGlobal(identifier->identifier);
    if (global_variable) {
      // 全局变量重定义
      fprintf(stderr, "variable %s redefined", identifier->identifier.c_str());
      ABORT_COMPILE;
      return nullptr;
    } else {
      auto *gv = new GlobalVariable(
          *TheModule, getTypeFromStr(type), isConst,
          GlobalVariable::LinkageTypes::ExternalLinkage,
          Constant::getNullValue(getTypeFromStr(type)), identifier->identifier);
      if (expr != nullptr) {
        auto v = expr->codegen();
        if (IS_COMPILE_ABORTED || v == NIL) {
          return NIL;
        }
        v = makeMatch(gv, v, this);
        gv->setInitializer(dyn_cast<Constant>(v));
      }
      ret = gv;
    }
  } else {
    // 局部变量
    auto this_type = getTypeFromStr(type);
    if (LOCALSVARS.find(identifier->identifier) == LOCALSVARS.end()) {
      ret = Builder->CreateAlloca(this_type);
      //            identifier.
      //            auto mem = Builder->CreateAlloca(getTypeFromStr(type),)
      if (expr != nullptr) {
        auto v = expr->codegen();
        if (IS_COMPILE_ABORTED) {
          return NIL;
        }
        v = makeMatch(ret, v, this, true);
        Builder->CreateStore(v, ret);
      }
      INSERTLOCAL(identifier->identifier, ret);
    }
  }
  return ret;
}

VariableDeclarationAST::VariableDeclarationAST(const string &type,
                                               IdentifierExprAST *identifier,
                                               ExpressionAST *expr,
                                               bool isConst)
    : type(type), identifier(identifier), expr(expr), isConst(isConst) {}

void VariableDeclarationAST::setIsConst(bool isConst) {
  VariableDeclarationAST::isConst = isConst;
}

std::string VariableDeclarationAST::getName() { return identifier->identifier; }

string VariableDeclarationAST::toString() {
  string s = "变量声明：";
  return s + this->getName();
}

void VariableDeclarationAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->identifier->setLayer(getLayer() + 1);
  this->identifier->initChildrenLayers();
  if (this->expr != NIL) {
    this->expr->setLayer(getLayer() + 1);
    this->expr->initChildrenLayers();
  }
}

llvm::Value *FuncPtrAST::codegen() {
  Value *ret = nullptr;
  if (this->getName() == "ctotal") {
    ret = NIL;
  }
  if (!HASLOCALS) {
    // 全局变量
    // 找全局变量，并返回一个全局变量的指针
    auto global_variable = TheModule->getNamedGlobal(identifier->identifier);
    // 如果已经存在，说明重定义了
    if (global_variable) {
      fprintf(stderr, "variable %s redefined", identifier->identifier.c_str());
      ABORT_COMPILE;
      return nullptr;
    } else {
      auto *gv = new GlobalVariable(
          *TheModule, getTypeFromStr(type), isConst,
          GlobalVariable::LinkageTypes::ExternalLinkage,
          Constant::getNullValue(getTypeFromStr(type)), identifier->identifier);
      if (expr != nullptr) {
        auto v = expr->codegen();
        if (IS_COMPILE_ABORTED) {
          return NIL;
        }
        auto val = dyn_cast<Constant>(expr->codegen());
        gv->setInitializer(val);
      }
      ret = gv;
    }
  } else {
    if (LOCALSVARS.find(identifier->identifier) == LOCALSVARS.end()) {
      std::vector<llvm::Type *> putArgs;
      for (int i = 1; i < args.size(); i++) {
        putArgs.push_back(getTypeFromStr(args[i]->identifier));
      }
      llvm::ArrayRef<llvm::Type *> argsRef(putArgs);
      llvm::FunctionType *funcPtrType = llvm::FunctionType::get(
          getTypeFromStr(args[0]->identifier), argsRef, false);

      ret = Builder->CreateAlloca(funcPtrType->getPointerTo());
      //            identifier.
      //           auto mem = Builder->CreateAlloca(getTypeFromStr(type),)
      INSERTLOCAL(identifier->identifier, ret);
    }
  }
  return ret;
}

llvm::Value *IntegerExprAST::codegen() {
  return ConstantInt::get(*TheContext, APInt(32, Val, true));
}

string IntegerExprAST::toString() { return string("整型：" + to_string(Val)); }

llvm::Value *IdentifierExprAST::codegen() {
  if (HASLOCALS) {
    // 局部变量
    auto local_mem = FINDLOCAL(identifier);
    if (local_mem != nullptr) {
      auto v = Builder->CreateLoad(local_mem);
      // 如果是数组，返回数组第一个元素的地址
      if (v->getType()->isArrayTy()) {
        // 数组兜底
        v->eraseFromParent();
        // 0取数组，后0为取第一个元素所在地址
        /*CreateInBoundsGEP
         * 指令获取数组第一个元素的地址，作为这个数组的值返回。这个指令的作用是对指针进行偏移，生成一个新的指针，这个新指针指向原指针所指向的类型中的一个元素。这里使用偏移值
         * {ConstantInt::get(getTypeFromStr("int"), 0),
         * ConstantInt::get(getTypeFromStr("int"), 0)}
         * 来获取第一个元素所在地址，即偏移量为 0。*/
        return Builder->CreateInBoundsGEP(
            local_mem, {ConstantInt::get(getTypeFromStr("int"), 0),
                        ConstantInt::get(getTypeFromStr("int"), 0)});
      }
      // 否则直接返回该变量的值
      return v;
    }
  }
  // 可能是全局变量
  auto gv = TheModule->getNamedGlobal(identifier);
  if (gv) {
    return Builder->CreateLoad(gv);
  }
  // 可能是函数指针
  auto func = TheModule->getFunction(identifier);
  if (func != NIL) {
    webc::parser::func_pointers_num++;
    return func;
  }
  ABORT_COMPILE;
  return LogErrorV((identifier + " is not defined!!!").c_str());
}

IdentifierExprAST::IdentifierExprAST() {}

ReturnStmtAST::ReturnStmtAST(ExpressionAST *expr) : expr(expr) {}

ReturnStmtAST::ReturnStmtAST() { expr = nullptr; }

llvm::Value *ReturnStmtAST::codegen() {
  // 检查是否在函数内(在函数声明的codegen中创建了一个ret_bb)
  auto ret_bb = TheCodeGenContext->getRetBb();
  if (ret_bb == NIL) {
    ABORT_COMPILE;
    return LogErrorV("return是否用在了非函数环境下？");
  }
  if (expr == nullptr) {
    return Builder->CreateBr(ret_bb);
  } else {
    auto ret_addr = TheCodeGenContext->getRetV();
    if (ret_addr == NIL) {
      ABORT_COMPILE;
      return LogErrorV("无返回值分配域？请联系作者");
    }
    auto v = expr->codegen();
    // 可能会返回一个表达式或者是一个函数调用
    if (v == NIL) {
      ABORT_COMPILE;
      return LogErrorV("函数返回的值为空，可能是值解析失败，或变量不存在");
    } else {
      // 当返回递归函数
      /* 在存储返回值之前，代码还检查了当前的表达式是否是递归调用的函数。
       * 如果是，则设置尾调用优化标志，这可以告诉
       * LLVM，这是一个尾调用，并且 LLVM
       * 将尝试将当前堆栈帧重用于新的函数调用。*/
      if (typeid(*expr) == typeid(CallExprAST)) {
        auto call_expr = dynamic_cast<CallExprAST *>(expr);
        if (call_expr->getCallName() ==
            TheCodeGenContext->getFunc()->getName()) {
          dyn_cast<CallInst>(v)->setTailCall(true);
        }
      }
      if (!v->getType()->isVoidTy()) {
        Builder->CreateStore(v, ret_addr);
      }
      Builder->CreateBr(ret_bb);
      return v;
    }
  }
}

string ReturnStmtAST::toString() {
  return string("ret：" + string(expr == NIL ? "void" : expr->toString()));
}

void ReturnStmtAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  if (expr != NIL) {
    expr->setLayer(getLayer() + 1);
    UPDATE_MAX_LAYERS(getLayer() + 1);
    expr->initChildrenLayers();
  }
}

Type *getTypeFromStr(const std::string &type) {
  if (type == "int") {
    return Type::getInt32Ty(*TheContext);
  } else if (type == "long") {
    return Type::getInt64Ty(*TheContext);
  } else if (type == "short") {
    // 这里的short应该是16位的
    return Type::getInt8Ty(*TheContext);
  } else if (type == "bool") {
    return Type::getInt1Ty(*TheContext); // 本质上就是1字节的int
  } else if (type == "void") {
    return Type::getVoidTy(*TheContext);
  } else if (type == "double") {
    return Type::getDoubleTy(*TheContext);
  } else if (type == "float") {
    return Type::getFloatTy(*TheContext);
  } else if (type == "char") {
    // 8b
    return Type::getInt8Ty(*TheContext);
  } else if (type == "str") {
    // 指向i8类型的指针
    return Type::getInt8Ty(*TheContext)->getPointerTo();
  } else if (type.compare(0, 3, "int") == 0) {
    auto int_num = type.substr(3);
    int widths = stoi(int_num);
    return Type::getIntNTy(*TheContext, widths);
  }
  return NIL;
}

// int a[5][6][7] -> vector<Expr>(7,6,5)
ArrayType *buildArrayType(vector<ExpressionAST *> *vec, Type *arrayElemType) {
  ArrayType *arr_type = NIL;
  // 从后往前遍历
  auto it = vec->rbegin();
  for (; it < vec->rend(); it++) {
    auto v = (*it)->codegen();
    if (isa<ConstantInt>(v)) {
      auto constant_int = dyn_cast<ConstantInt>(v);
      uint64_t size = 0;
      /*如果 constant_int 的位宽小于等于 32
       * 位，那么它的无符号整数值将被转换为一个 uint64_t 类型的值，并保存到 size
       * 变量中；如果 constant_int 的位宽大于 32
       * 位，那么它的有符号整数值将被转换为一个 uint64_t 类型的值，并保存到 size
       * 变量中。这个 size 变量最终用于创建 LLVM 的 ArrayType 类型。*/
      if (constant_int->getBitWidth() <= 32) {
        // size就是当前数组的维度
        size = constant_int->getZExtValue();
      } else {
        size = constant_int->getSExtValue();
      }
      arr_type = arr_type == nullptr ? ArrayType::get(arrayElemType, size)
                                     : ArrayType::get(arr_type, size);
    } else {
      ABORT_COMPILE;
      LogError("数组声明[]内必须为常量，请检查");
    }
  }
  return arr_type;
}

// 用于获取数组元素的类型
Type *getArrayElemType(Value *value) {
  if (value == nullptr ||
      // 数组首元素的地址以及数组本身
      (!value->getType()->isPointerTy() && !value->getType()->isArrayTy())) {
    return nullptr;
  }
  Type *tmp = value->getType();
  while (tmp->isPointerTy()) {
    // getContainedType(0)返回的是指针指向的类型
    tmp = tmp->getContainedType(0);
  }
  while (tmp->isArrayTy()) {
    // 获取其元素类型
    tmp = tmp->getArrayElementType();
  }
  return tmp;
}

// 获取一个指针或数组类型的数组类型的
Type *getArrayType(Value *value) {
  if (value == nullptr ||
      (!value->getType()->isPointerTy() && !value->getType()->isArrayTy())) {
    return nullptr;
  }
  Type *tmp = value->getType();
  while (tmp->isPointerTy()) {
    tmp = tmp->getContainedType(0);
  }
  if (tmp->isArrayTy()) {
    return tmp;
  } else {
    return nullptr;
  }
}

// 获取最大层数
int getMaxLayer(BlockAST *ast) { return 0; }

// arr[i][j][k]
llvm::Value *IdentifierArrExprAST::codegen() {
  auto local = FINDLOCAL(identifier);
  if (local == NIL) {
    return LogErrorV(("未找到" + identifier + "\n").c_str());
  }
  auto it = arrIndex->begin();
  auto v_vec = vector<Value *>();
  // 添加0取指针地址
  // FIXME
  // 此处特判了如果是连续的两个pointerType，则判断可能传入的是一个CreateStore后的值，此时进行Load（该方案拓展性差，后期改良方案）
  /*判断要访问的数组元素是否是多维数组，如果是，则通过CreateLoad将其转化为指向数组首元素的指针。如果不是多维数组，则将0作为偏移量压入v_vec。*/
  if (local->getType()->isPointerTy() &&
      local->getType()->getContainedType(0)->isPointerTy()) {
    local = Builder->CreateLoad(local);
  } else {
    /*在 LLVM IR
     * 中，指向数组的指针和指向数组元素的指针类型是不同的。指向数组的指针是
     * ArrayTy** 类型，而指向数组元素的指针是 ArrayTy* 类型。在这个函数中，如果
     * local 不是指向数组元素的指针，那么就需要将一个 0 偏移量压入 v_vec，将
     * local 转换成指向数组元素的指针类型。因为一个指向数组元素的指针，加上 0
     * 偏移量得到的仍然是指向数组元素的指针，不需要进行偏移。如果 local
     * 已经是指向数组元素的指针类型，那么不需要进行转换，直接将 local
     * 作为返回值。*/
    v_vec.push_back(ConstantInt::get(getTypeFromStr("int"), 0));
  }
  Value *ret = local;
  //    if (!(local->getType()->isPointerTy())) {
  //        v_vec.push_back(ConstantInt::get(getTypeFromStr("int"), 0));
  //    }
  // 兜底，目前放入local内的数组
  for (; it < arrIndex->end(); it++) {
    auto v = (*it)->codegen();
    if (v == nullptr) {
      ABORT_COMPILE;
      return LogErrorV("数组解析失败");
    }
    v_vec.push_back(v);
  }
  /*Builder->CreateInBoundsGEP 实际上是创建了一个 getelementptr 的指令，它的参数
   * ret 是一个数组或结构体的指针， v_vec 是一个包含所有索引的值的向量。*/
  ret = Builder->CreateInBoundsGEP(ret, ArrayRef<Value *>(v_vec));
  return Builder->CreateLoad(ret);
}

IdentifierArrExprAST::IdentifierArrExprAST(const string &identifier,
                                           vector<ExpressionAST *> *arrIndex)
    : IdentifierExprAST(identifier), arrIndex(arrIndex) {}

string IdentifierArrExprAST::toString() {
  return string("数组标识符：") + identifier;
}

void IdentifierArrExprAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  if (!arrIndex->empty()) {
    UPDATE_MAX_LAYERS(getLayer() + 1);
  }
  for (auto const &arri : *arrIndex) {
    if (arri != NIL) {
      arri->setLayer(getLayer() + 1);
      arri->initChildrenLayers();
    }
  }
}

llvm::Value *VariableArrDeclarationAST::codegen() {
  Value *ret = nullptr;
  ArrayType *arr_type =
      buildArrayType(identifier->arrIndex, getTypeFromStr(type));
  // 如果是全局变量
  if (!HASLOCALS) {
    vector<Constant *> *ca = genGlobalExprs();
    auto constant_arr_value =
        ConstantArray::get(arr_type, ArrayRef<Constant *>(*ca));
    // 全局变量
    auto global_variable = TheModule->getNamedGlobal(identifier->identifier);
    if (global_variable) {
      fprintf(stderr, "variable %s redefined", identifier->identifier.c_str());
      return nullptr;
    } else {
      auto gv = new GlobalVariable(
          *TheModule, arr_type, true,
          GlobalVariable::LinkageTypes::ExternalLinkage,
          Constant::getNullValue(arr_type), identifier->identifier);
      auto vs = ArrayRef<Constant *>(*ca);
      gv->setInitializer(constant_arr_value);
      //            gv->print(outs(), true);
      ret = gv;
    }
  } else {
    if (LOCALSVARS.find(identifier->identifier) == LOCALSVARS.end()) {
      auto mem = Builder->CreateAlloca(arr_type);
      genLocalStoreExprs(mem);
      INSERTLOCAL(identifier->identifier, mem);
      ret = mem;
    }
  }
  return ret;
}

VariableArrDeclarationAST::VariableArrDeclarationAST(
    const string &type, IdentifierArrExprAST *identifier,
    vector<NodeAST *> *exprs, bool isConst)
    : type(type), identifier(identifier), exprs(exprs), isConst(isConst) {}

// 返回的vector表示数组的index
vector<Constant *> *VariableArrDeclarationAST::genGlobalExprs() {
  //    vector<uint64_t> vmaxindex = getIndexVal();
  //    vector<uint64_t> vindex(vmaxindex.size(),0);
  if (exprs == NIL && exprs->empty()) {
    return NIL;
  }
  // 常量数组表示数组的index
  auto arr_index_value_vector = new vector<Constant *>();
  //    for (auto exp = exprs->begin(); exp != exprs->end() ; exp++) {
  //
  //    }
  for_each(exprs->begin(), exprs->end(), [&](NodeAST *e) -> void {
    auto v = e->codegen();
    // 全局数组的初始化必须为常量声明
    if (!isa<Constant>(v)) {
      if (v->getType()->isIntegerTy()) {
        //                auto id = typeid(v);
        auto ty = dyn_cast<IntegerType>(v->getType());
        if (ty == NIL) {
          ABORT_COMPILE;
          LogErrorV("全局数组初始化必须为常量声明");
          return;
        } else {
          // TODO FixME!
          auto info = typeid(v).name();
          v = ConstantInt::get(getTypeFromStr("int"), 0);
        }
      } else {
        ABORT_COMPILE;
        LogErrorV("全局数组初始化必须为常量声明");
        return;
      }
    }
    arr_index_value_vector->push_back(dyn_cast<Constant>(v));
  });
  return arr_index_value_vector;
}
/*函数首先检查数组是否有初始化表达式，如果没有，则直接返回。然后，它创建了一个存储每个维度的索引的向量vindex，其初始值为零。然后，函数遍历每个初始化表达式，并为每个表达式生成内存地址。生成内存地址的过程中，它使用vindex中的每个维度索引，并将它们添加到gepindex_vec中，以便在内存中正确地定位该元素。然后，函数使用CreateInBoundsGEP()函数生成该元素的内存地址，并使用CreateStore()函数将其存储在内存中。

最后，函数使用incrementVectorIndex()函数来增加vindex向量的值，以便在下一次循环中正确地定位下一个元素。如果incrementVectorIndex()函数返回1，则表示数组初始化已经完成，超过了定义的长度，那么函数退出循环。

incrementVectorIndex()函数用于增加向量vindex的值，以便在下一次循环中正确地定位下一个元素。它首先检查vindex是否已经达到了vmaxindex的最大值。如果是，则返回1，表示数组初始化已经完成，超过了定义的长度。否则，它从向量的末尾开始逐个检查维度的值。如果当前维度还没有达到最大值，则将其增加1并返回。否则，将当前维度的值设置为0，并检查前一个维度，以便递增其值。如果所有维度的值都已达到最大值，则返回1。*/
void VariableArrDeclarationAST::genLocalStoreExprs(Value *mem) {
  if (exprs == NIL && exprs->empty()) {
    return;
  }
  vector<uint64_t> vmaxindex = getIndexVal();
  // 每一个维度的长度都是0
  vector<uint64_t> vindex(vmaxindex.size(), 0);
  for (auto e : *exprs) {
    if (e != NIL) {
      // 存储 LLVM IR 中的 GEP 指令的索引操作数
      vector<Value *> gepindex_vec;
      // 垫一个0
      gepindex_vec.push_back(ConstantInt::get(getTypeFromStr("int"), 0));
      for (auto index : vindex) {
        auto intv = ConstantInt::get(Type::getInt32Ty(*TheContext), index);
        gepindex_vec.push_back(intv);
      }
      auto ret =
          Builder->CreateInBoundsGEP(mem, ArrayRef<Value *>(gepindex_vec));
      Builder->CreateStore(e->codegen(), ret);
      // 调用向量索引增量计算函数
      if (incrementVectorIndex(vindex, vmaxindex)) {
        //                LogWarn("数组声明超过定义，超过定义的值将被忽略");
        break;
      }
    } else {
      // TODO Fixme
    }
  }
}

// 向量索引的增量计算
int VariableArrDeclarationAST::incrementVectorIndex(vector<uint64_t> &indexVec,
                                                    vector<uint64_t> &maxVec) {
  // 先判断维度是否相同
  assert(indexVec.size() == maxVec.size());
  int sz = indexVec.size() - 1;
  bool is_max = true;
  for (int tsz = 0; tsz <= sz; ++tsz) {
    if (indexVec.at(tsz) != maxVec.at(tsz) - 1) {
      is_max = false;
      break;
    }
  }
  int carry = 1;
  if (is_max) {
    return carry;
  }
  while (carry > 0 && sz >= 0) {
    if (indexVec.at(sz) + 1 < maxVec.at(sz)) {
      indexVec[sz]++;
      carry = 0;
      break;
    } else {
      indexVec[sz] = 0;
      sz--;
    }
  }
  return carry;
}

// 获取变量数组声明语句中的每个维度的大小
vector<uint64_t> VariableArrDeclarationAST::getIndexVal() {
  vector<uint64_t> vmaxindex;
  // 获取index的vector
  for_each(identifier->arrIndex->begin(), identifier->arrIndex->end(),
           [&](ExpressionAST *e) {
             if (e == NIL) {
               if (vmaxindex.empty()) {
                 vmaxindex.push_back(INT_MAX);
               } else {
                 ABORT_COMPILE;
                 LogError(
                     "只允许数组index首部为空值，要不然无法编译，我就罢工了");
                 return;
               }
             } else {
               auto value_index = dyn_cast<ConstantInt>(e->codegen());
               if (value_index == NIL) {
                 ABORT_COMPILE;
                 LogError("声明一个数组长度不能使用非常量");
                 return;
               } else {
                 if (value_index->getBitWidth() <= 32) {
                   vmaxindex.push_back(value_index->getSExtValue());
                 } else {
                   vmaxindex.push_back(value_index->getZExtValue());
                 }
               }
             }
           });
  return vmaxindex;
}

// 创建一个指向某个类型的数组的指针
Type *VariableArrDeclarationAST::buildPointerTy() {
  auto idx_nums = getIndexVal();
  if (idx_nums.empty()) {
    return NIL;
  }
  Type *type = getTypeFromStr(this->type);
  int length = idx_nums.size();
  for (int i = length - 1; i > 0; i--) {
    if (idx_nums[i] == INT_MAX) {
      ABORT_COMPILE;
      return LogErrorTy(
          "数组索引值只有第一个可以为空!(第一个索引我不管，你随便)");
    }
    type = ArrayType::get(type, idx_nums[i]);
  }
  return type->getPointerTo();
}

string VariableArrDeclarationAST::getName() { return identifier->identifier; }

string VariableArrDeclarationAST::toString() { return identifier->identifier; }

void VariableArrDeclarationAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->identifier->setLayer(getLayer() + 1);
  this->identifier->initChildrenLayers();
  if (this->exprs != NIL) {
    for (const auto &item : *this->exprs) {
      item->setLayer(getLayer() + 1);
      item->initChildrenLayers();
    }
  }
}

string FuncPtrAST::getName() { return identifier->identifier; }

FuncPtrAST::FuncPtrAST(std::vector<IdentifierExprAST *> args,
                       IdentifierExprAST *identifier)
    : args(std::move(args)), identifier(identifier) {
  webc::parser::func_pointers_num++;
}

void FuncPtrAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->identifier->setLayer(getLayer() + 1);
  this->identifier->initChildrenLayers();
  for (const auto &item : this->args) {
    if (item != NIL) {
      item->setLayer(getLayer() + 1);
      item->initChildrenLayers();
    }
  }
}

VariableArrAssignmentAST::VariableArrAssignmentAST(
    IdentifierArrExprAST *identifier, ExpressionAST *expr)
    : identifier(identifier), expr(expr) {}

llvm::Value *VariableArrAssignmentAST::codegen() {
  // 读数组的地址
  auto arr_addr = FINDLOCAL(identifier->identifier);
  // 如果是i8*
  /*如果数组地址类型是指向数组类型的指针，则直接使用该地址，否则需要使用 load
   * 指令将该地址所指向的数组元素地址取出来*/
  if (arr_addr->getType()->isPointerTy() &&
      arr_addr->getType()->getContainedType(0)->isArrayTy()) {
    // ignore
  } else {
    arr_addr = Builder->CreateLoad(arr_addr);
  }
  if (arr_addr == nullptr) {
    ABORT_COMPILE;
    return LogErrorV((identifier->identifier + "is not defined").c_str());
  }
  auto st = identifier->arrIndex->begin();
  Value *ret = arr_addr;
  vector<Value *> vec;
  // 0取地址，如果是i8*这种就不用，如果是[i8 x n]*就得加0
  // FIXME: 不优雅的特判
  if (arr_addr->getType()->isPointerTy() &&
      arr_addr->getType()->getContainedType(0)->isArrayTy()) {
    /*当要访问的数组是一个指向数组的指针时，我们需要在 GEP
     * 指令的索引值数组中提供一个额外的
     * 0，表示要访问的是该指针指向的整个数组，而不是数组的某个元素。如果不提供这个额外的
     * 0，LLVM
     * 编译器会认为我们要访问指针本身所指向的内存区域，而不是数组本身，这就会导致计算出来的地址偏移量错误。*/
    vec.push_back(ConstantInt::get(getTypeFromStr("int"), 0));
  }
  for (; st != identifier->arrIndex->end(); st++) {
    auto v = (*st)->codegen();
    vec.push_back(v);
  }
  ret = Builder->CreateInBoundsGEP(arr_addr, ArrayRef<Value *>(vec));
  auto newV = expr->codegen();
  newV = makeMatch(ret, newV, this);
  ret = Builder->CreateStore(newV, ret);
  return ret;
}

string VariableArrAssignmentAST::toString() {
  return string("数组变量赋值") + identifier->toString();
}

void VariableArrAssignmentAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  this->identifier->setLayer(getLayer() + 1);
  this->expr->setLayer(getLayer() + 1);
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->identifier->initChildrenLayers();
  this->expr->initChildrenLayers();
}

OutStmtAST::OutStmtAST() = default;

Value *OutStmtAST::codegen() {
  auto blk = TheCodeGenContext->findTopLoopCodeGenBlockTypeBlock();
  if (blk == NIL) {
    ABORT_COMPILE;
    return LogErrorV("break 不能用作于当前的代码上下文中，请检查");
  }
  switch (blk->contextType) {
  case NONE:
    return LogErrorV("程序匹配的break环境与当前域不符(NONE)，请联系作者");
  case FOR:
    if (blk->context.forCodeGenBlockContext == NIL ||
        blk->context.forCodeGenBlockContext->bbEndFor == NIL) {
      ABORT_COMPILE;
      return LogErrorV("程序匹配的上下文context为NULL，请联系作者");
    }
    return Builder->CreateBr(blk->context.forCodeGenBlockContext->bbEndFor);
  case IF:
    return LogErrorV("程序匹配的break环境与当前域不符(IF)，请联系作者");
  case WHILE:
    if (blk->context.whileCodeGenBlockContext == NIL ||
        blk->context.whileCodeGenBlockContext->bbEndWhile == NIL) {
      ABORT_COMPILE;
      return LogErrorV(
          "程序匹配的上下文context为NULL(while continue;)，请联系作者");
    }
    return Builder->CreateBr(blk->context.whileCodeGenBlockContext->bbEndWhile);
  default:
    ABORT_COMPILE;
    return LogErrorV("Break在未知的代码域中，或编译器目前无法处理的关键字");
  }
  return NIL;
}

string OutStmtAST::toString() { return "跳出语句"; }

ContinueStmtAST::ContinueStmtAST() = default;

Value *ContinueStmtAST::codegen() {
  auto blk = TheCodeGenContext->findTopLoopCodeGenBlockTypeBlock();
  if (blk == NIL) {
    ABORT_COMPILE;
    return LogErrorV("break 不能用作于当前的代码上下文中，请检查");
  }
  switch (blk->contextType) {
  case NONE:
    ABORT_COMPILE;
    return LogErrorV("程序匹配的break环境与当前域不符(NONE)，请联系作者");
  case FOR:
    if (blk->context.forCodeGenBlockContext == NIL ||
        blk->context.forCodeGenBlockContext->bbEndFor == NIL) {
      ABORT_COMPILE;
      return LogErrorV("程序匹配的上下文context为NULL，请联系作者");
    }
    return Builder->CreateBr(blk->context.forCodeGenBlockContext->bbStep);
  case IF:
    ABORT_COMPILE;
    return LogErrorV(
        "程序匹配的break环境与当前域不符(IF)，这可能是个bug，请联系作者");
  case WHILE:
    if (blk->context.whileCodeGenBlockContext == NIL ||
        blk->context.whileCodeGenBlockContext->bbCond == NIL) {
      ABORT_COMPILE;
      return LogErrorV(
          "程序匹配的上下文context为NULL(while continue;)，请联系作者");
    }
    return Builder->CreateBr(blk->context.whileCodeGenBlockContext->bbCond);
  default:
    ABORT_COMPILE;
    return LogErrorV("Break在未知的代码域中，或编译器目前无法处理的关键字");
  }
  return NIL;
}

string ContinueStmtAST::toString() { return "继续"; }

WhileStmtAST::WhileStmtAST(NodeAST *cond, BlockAST *body)
    : Cond(cond), Body(body) {
  webc::parser::loop_num++;
}

llvm::Value *WhileStmtAST::codegen() {
  auto function = Builder->GetInsertBlock()->getParent();
  if (function == NIL) {
    ABORT_COMPILE;
    return LogErrorV("while 不能用在非函数体中");
  }
  BasicBlock *bbCond = BasicBlock::Create(*TheContext, "while_cond", function);
  TheCodeGenContext->push_block(bbCond);
  BasicBlock *bbBody = BasicBlock::Create(*TheContext, "while_body");
  BasicBlock *bbEndWhile = BasicBlock::Create(*TheContext, "while_end");
  LOCALS->setContextType(CodeGenBlockContextType::WHILE);
  LOCALS->context.whileCodeGenBlockContext =
      new WhileCodeGenBlockContext(bbCond, bbBody, bbEndWhile);
  // 创建条件分支
  Builder->CreateBr(bbCond);
  Builder->SetInsertPoint(bbCond);
  auto condV = Cond->codegen();
  if (condV == NIL) {
    ABORT_COMPILE;
    return LogErrorV("while中的判断语句解析失败");
  }
  if (condV->getType()->isIntegerTy()) {
    condV = Builder->CreateICmpNE(condV, ConstantInt::get(condV->getType(), 0),
                                  "while_cond_integer");
  } else if (condV->getType()->isDoubleTy()) {
    condV = Builder->CreateFCmpUNE(condV, ConstantFP::get(condV->getType(), 0),
                                   "while_cond_double");
  }
  Builder->CreateCondBr(condV, bbBody, bbEndWhile);
  function->getBasicBlockList().push_back(bbBody);
  Builder->SetInsertPoint(bbBody);
  auto bodyV = Body->codegen();
  if (bodyV == NIL) {
    ABORT_COMPILE;
    return LogErrorV("while中的判断语句解析失败");
  }
  if (Builder->GetInsertBlock()->empty() ||
      !Builder->GetInsertBlock()->rbegin()->isTerminator()) {
    Builder->CreateBr(bbCond);
  }
  function->getBasicBlockList().push_back(bbEndWhile);
  Builder->SetInsertPoint(bbEndWhile);
  TheCodeGenContext->pop_block();
  return bbEndWhile;
}

string WhileStmtAST::toString() {
  string s = "循环：\n条件:\n";
  s.append(this->Cond->toString());
  s.append("\n执行域：" + this->Body->toString());
  return s;
}

void WhileStmtAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  this->Cond->setLayer(getLayer() + 1);
  this->Body->setLayer(getLayer() + 1);
  UPDATE_MAX_LAYERS(getLayer() + 1);
  this->Cond->initChildrenLayers();
  this->Body->initChildrenLayers();
}

string ExpressionAST::toString() { return std::string(); }

string StatementAST::toString() { return std::string(); }

string ExpressionStatementAST::toString() {
  return std::string("变量声明：" + expr->toString());
}

void ExpressionStatementAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  if (expr != NIL) {
    expr->setLayer(getLayer() + 1);
    UPDATE_MAX_LAYERS(getLayer() + 1);
    expr->initChildrenLayers();
  }
}

string BlockAST::toString() {
  string s = "block域内容：\n";
  for (auto st : statements) {
    s.append(st->toString() + "\n");
  }
  return s;
}

BlockAST::BlockAST() {}

void BlockAST::addStatement(StatementAST *stmt) {
  this->statements.push_back(stmt);
}

void BlockAST::initChildrenLayers() {
  NodeAST::initChildrenLayers();
  if (!statements.empty()) {
    UPDATE_MAX_LAYERS(getLayer() + 1);
  }
  for (const auto &stmt : statements) {
    if (stmt != NIL) {
      stmt->setLayer(getLayer() + 1);
      stmt->initChildrenLayers();
    }
  }
}

string IdentifierExprAST::toString() { return string("标识符：") + identifier; }

// 字符串表达式 重点
llvm::Value *StringExprAST::codegen() {
  GlobalVariable *gv;
  if (TheCodeGenContext->getFunc() == NIL) {
    auto sz = str->size();
    // 存储字符串的每个字符和字符串的结束标志 '\0'
    auto arr_type = ArrayType::get(getTypeFromStr("char"), sz + 1);

    gv = new GlobalVariable(
        *TheModule, arr_type, true, GlobalValue::PrivateLinkage,
        ConstantDataArray::getString(*TheContext, *str), getUniqueId());
    // 将gv的地址类型设置为全局的未命名地址，这意味着可以在LLVM
    // IR中更容易地访问这个全局变量，同时又能减小可执行文件的大小。
    gv->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
    // 转换为i8*类型然后返回
    return ConstantExpr::getBitCast(gv, getTypeFromStr("str"));
    // return Builder->CreateInBoundsGEP(
    //     gv, {ConstantInt::get(getTypeFromStr("int"), 0),
    //          ConstantInt::get(getTypeFromStr("int"), 0)});  
  } else {
    /*如果字符串定义在函数内部，调用 Builder->CreateGlobalString
     * 方法，将该字符串存储在一个 GlobalVariable
     * 类型的全局变量中，并返回一个指向该全局变量的指针*/
    gv = Builder->CreateGlobalString(*str, StringExprAST::getUniqueId());
  }
  /*最后，通过调用 Builder->CreateInBoundsGEP
   * 方法来生成一个指向字符串的指针。这个指针是通过将全局变量的指针作为第一个元素，并将整数
   * 0 作为第二个元素的索引来计算得到的*/
  return Builder->CreateInBoundsGEP(
      gv, {ConstantInt::get(getTypeFromStr("int"), 0),
           ConstantInt::get(getTypeFromStr("int"), 0)});
}

string StringExprAST::getUniqueId() {
  return std::string("_str_") + to_string(StringExprAST::id++);
}

void StringExprAST::setUniqueId(long id) { StringExprAST::id = id; }

StringExprAST::StringExprAST(string *str) : str(str) {}

string StringExprAST::toString() { return str == NIL ? "" : *str; }

string NullExprAST::toString() { return "空值"; }

llvm::Value *NullExprAST::codegen() {
  return ConstantExpr::getNullValue(getTypeFromStr("char")->getPointerTo());
}

llvm::Value *BoolExprAST::codegen() {
  return is_true ? ConstantInt::get(getTypeFromStr("bool"), 1)
                 : ConstantInt::get(getTypeFromStr("bool"), 0);
}

string BoolExprAST::toString() { return std::to_string(is_true); }

int NodeAST::getLayer() const { return layer; }

void NodeAST::setLayer(int layer) { this->layer = layer; }

void NodeAST::initChildrenLayers() {
  // empty impl
  webc::ast::nodes_num++;
}