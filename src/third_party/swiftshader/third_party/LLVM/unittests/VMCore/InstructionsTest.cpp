//===- llvm/unittest/VMCore/InstructionsTest.cpp - Instructions unit tests ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Instructions.h"
#include "llvm/BasicBlock.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/ADT/STLExtras.h"
#include "gtest/gtest.h"

namespace llvm {
namespace {

TEST(InstructionsTest, ReturnInst) {
  LLVMContext &C(getGlobalContext());

  // test for PR6589
  const ReturnInst* r0 = ReturnInst::Create(C);
  EXPECT_EQ(r0->getNumOperands(), 0U);
  EXPECT_EQ(r0->op_begin(), r0->op_end());

  IntegerType* Int1 = IntegerType::get(C, 1);
  Constant* One = ConstantInt::get(Int1, 1, true);
  const ReturnInst* r1 = ReturnInst::Create(C, One);
  EXPECT_EQ(1U, r1->getNumOperands());
  User::const_op_iterator b(r1->op_begin());
  EXPECT_NE(r1->op_end(), b);
  EXPECT_EQ(One, *b);
  EXPECT_EQ(One, r1->getOperand(0));
  ++b;
  EXPECT_EQ(r1->op_end(), b);

  // clean up
  delete r0;
  delete r1;
}

TEST(InstructionsTest, BranchInst) {
  LLVMContext &C(getGlobalContext());

  // Make a BasicBlocks
  BasicBlock* bb0 = BasicBlock::Create(C);
  BasicBlock* bb1 = BasicBlock::Create(C);

  // Mandatory BranchInst
  const BranchInst* b0 = BranchInst::Create(bb0);

  EXPECT_TRUE(b0->isUnconditional());
  EXPECT_FALSE(b0->isConditional());
  EXPECT_EQ(1U, b0->getNumSuccessors());

  // check num operands
  EXPECT_EQ(1U, b0->getNumOperands());

  EXPECT_NE(b0->op_begin(), b0->op_end());
  EXPECT_EQ(b0->op_end(), llvm::next(b0->op_begin()));

  EXPECT_EQ(b0->op_end(), llvm::next(b0->op_begin()));

  IntegerType* Int1 = IntegerType::get(C, 1);
  Constant* One = ConstantInt::get(Int1, 1, true);

  // Conditional BranchInst
  BranchInst* b1 = BranchInst::Create(bb0, bb1, One);

  EXPECT_FALSE(b1->isUnconditional());
  EXPECT_TRUE(b1->isConditional());
  EXPECT_EQ(2U, b1->getNumSuccessors());

  // check num operands
  EXPECT_EQ(3U, b1->getNumOperands());

  User::const_op_iterator b(b1->op_begin());

  // check COND
  EXPECT_NE(b, b1->op_end());
  EXPECT_EQ(One, *b);
  EXPECT_EQ(One, b1->getOperand(0));
  EXPECT_EQ(One, b1->getCondition());
  ++b;

  // check ELSE
  EXPECT_EQ(bb1, *b);
  EXPECT_EQ(bb1, b1->getOperand(1));
  EXPECT_EQ(bb1, b1->getSuccessor(1));
  ++b;

  // check THEN
  EXPECT_EQ(bb0, *b);
  EXPECT_EQ(bb0, b1->getOperand(2));
  EXPECT_EQ(bb0, b1->getSuccessor(0));
  ++b;

  EXPECT_EQ(b1->op_end(), b);

  // clean up
  delete b0;
  delete b1;

  delete bb0;
  delete bb1;
}

TEST(InstructionsTest, CastInst) {
  LLVMContext &C(getGlobalContext());

  Type* Int8Ty = Type::getInt8Ty(C);
  Type* Int64Ty = Type::getInt64Ty(C);
  Type* V8x8Ty = VectorType::get(Int8Ty, 8);
  Type* V8x64Ty = VectorType::get(Int64Ty, 8);
  Type* X86MMXTy = Type::getX86_MMXTy(C);

  const Constant* c8 = Constant::getNullValue(V8x8Ty);
  const Constant* c64 = Constant::getNullValue(V8x64Ty);

  EXPECT_TRUE(CastInst::isCastable(V8x8Ty, X86MMXTy));
  EXPECT_TRUE(CastInst::isCastable(X86MMXTy, V8x8Ty));
  EXPECT_FALSE(CastInst::isCastable(Int64Ty, X86MMXTy));
  EXPECT_TRUE(CastInst::isCastable(V8x64Ty, V8x8Ty));
  EXPECT_TRUE(CastInst::isCastable(V8x8Ty, V8x64Ty));
  EXPECT_EQ(CastInst::Trunc, CastInst::getCastOpcode(c64, true, V8x8Ty, true));
  EXPECT_EQ(CastInst::SExt, CastInst::getCastOpcode(c8, true, V8x64Ty, true));
}

}  // end anonymous namespace
}  // end namespace llvm
