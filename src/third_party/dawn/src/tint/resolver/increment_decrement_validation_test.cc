// Copyright 2022 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/resolver/resolver.h"

#include "gmock/gmock.h"
#include "src/tint/resolver/resolver_test_helper.h"

namespace tint::resolver {
namespace {

using ResolverIncrementDecrementValidationTest = ResolverTest;

TEST_F(ResolverIncrementDecrementValidationTest, Increment_Signed) {
  // var a : i32 = 2;
  // a++;
  auto* var = Var("a", ty.i32(), ast::StorageClass::kNone, Expr(2));
  WrapInFunction(var, Increment(Source{{12, 34}}, "a"));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, Decrement_Signed) {
  // var a : i32 = 2;
  // a--;
  auto* var = Var("a", ty.i32(), ast::StorageClass::kNone, Expr(2));
  WrapInFunction(var, Decrement(Source{{12, 34}}, "a"));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, Increment_Unsigned) {
  // var a : u32 = 2u;
  // a++;
  auto* var = Var("a", ty.u32(), ast::StorageClass::kNone, Expr(2u));
  WrapInFunction(var, Increment(Source{{12, 34}}, "a"));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, Decrement_Unsigned) {
  // var a : u32 = 2u;
  // a--;
  auto* var = Var("a", ty.u32(), ast::StorageClass::kNone, Expr(2u));
  WrapInFunction(var, Decrement(Source{{12, 34}}, "a"));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, ThroughPointer) {
  // var a : i32;
  // let b : ptr<function,i32> = &a;
  // *b++;
  auto* var_a = Var("a", ty.i32(), ast::StorageClass::kFunction);
  auto* var_b = Const("b", ty.pointer<int>(ast::StorageClass::kFunction),
                      AddressOf(Expr("a")));
  WrapInFunction(var_a, var_b, Increment(Source{{12, 34}}, Deref("b")));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, ThroughArray) {
  // var a : array<i32, 4>;
  // a[1]++;
  auto* var_a = Var("a", ty.array(ty.i32(), 4), ast::StorageClass::kNone);
  WrapInFunction(var_a, Increment(Source{{12, 34}}, IndexAccessor("a", 1)));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, ThroughVector_Index) {
  // var a : vec4<i32>;
  // a.y++;
  auto* var_a = Var("a", ty.vec4(ty.i32()), ast::StorageClass::kNone);
  WrapInFunction(var_a, Increment(Source{{12, 34}}, IndexAccessor("a", 1)));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, ThroughVector_Member) {
  // var a : vec4<i32>;
  // a.y++;
  auto* var_a = Var("a", ty.vec4(ty.i32()), ast::StorageClass::kNone);
  WrapInFunction(var_a, Increment(Source{{12, 34}}, MemberAccessor("a", "y")));

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, Float) {
  // var a : f32 = 2.0;
  // a++;
  auto* var = Var("a", ty.f32(), ast::StorageClass::kNone, Expr(2.f));
  auto* inc = Increment(Expr(Source{{12, 34}}, "a"));
  WrapInFunction(var, inc);

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: increment statement can only be applied to an "
            "integer scalar");
}

TEST_F(ResolverIncrementDecrementValidationTest, Vector) {
  // var a : vec4<f32>;
  // a++;
  auto* var = Var("a", ty.vec4<i32>(), ast::StorageClass::kNone);
  auto* inc = Increment(Expr(Source{{12, 34}}, "a"));
  WrapInFunction(var, inc);

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: increment statement can only be applied to an "
            "integer scalar");
}

TEST_F(ResolverIncrementDecrementValidationTest, Atomic) {
  // var<workgroup> a : atomic<i32>;
  // a++;
  Global(Source{{12, 34}}, "a", ty.atomic(ty.i32()),
         ast::StorageClass::kWorkgroup);
  WrapInFunction(Increment(Expr(Source{{56, 78}}, "a")));

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "56:78 error: increment statement can only be applied to an "
            "integer scalar");
}

TEST_F(ResolverIncrementDecrementValidationTest, Literal) {
  // 1++;
  WrapInFunction(Increment(Expr(Source{{56, 78}}, 1)));

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), "56:78 error: cannot modify value of type 'i32'");
}

TEST_F(ResolverIncrementDecrementValidationTest, Constant) {
  // let a = 1;
  // a++;
  auto* a = Const(Source{{12, 34}}, "a", nullptr, Expr(1));
  WrapInFunction(a, Increment(Expr(Source{{56, 78}}, "a")));

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(56:78 error: cannot modify constant value
12:34 note: 'a' is declared here:)");
}

TEST_F(ResolverIncrementDecrementValidationTest, Parameter) {
  // fn func(a : i32)
  // {
  //   a++;
  // }
  auto* a = Param(Source{{12, 34}}, "a", ty.i32());
  Func("func", {a}, ty.void_(), {Increment(Expr(Source{{56, 78}}, "a"))});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(56:78 error: cannot modify function parameter
12:34 note: 'a' is declared here:)");
}

TEST_F(ResolverIncrementDecrementValidationTest, ReturnValue) {
  // fn func() -> i32 {
  //   return 0;
  // }
  // {
  //   a++;
  // }
  Func("func", {}, ty.i32(), {Return(0)});
  WrapInFunction(Increment(Call(Source{{56, 78}}, "func")));

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(56:78 error: cannot modify value of type 'i32')");
}

TEST_F(ResolverIncrementDecrementValidationTest, ReadOnlyBuffer) {
  // @group(0) @binding(0) var<storage,read> a : i32;
  // {
  //   a++;
  // }
  Global(Source{{12, 34}}, "a", ty.i32(), ast::StorageClass::kStorage,
         ast::Access::kRead, GroupAndBinding(0, 0));
  WrapInFunction(Increment(Source{{56, 78}}, "a"));

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      "56:78 error: cannot modify read-only type 'ref<storage, i32, read>'");
}

TEST_F(ResolverIncrementDecrementValidationTest, Phony) {
  // _++;
  WrapInFunction(Increment(Phony(Source{{56, 78}})));
  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), "56:78 error: cannot modify value of type 'void'");
}

TEST_F(ResolverIncrementDecrementValidationTest, InForLoopInit) {
  // var a : i32 = 2;
  // for (a++; ; ) {
  //   break;
  // }
  auto* a = Var("a", ty.i32(), ast::StorageClass::kNone, Expr(2));
  auto* loop =
      For(Increment(Source{{56, 78}}, "a"), nullptr, nullptr, Block(Break()));
  WrapInFunction(a, loop);

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverIncrementDecrementValidationTest, InForLoopCont) {
  // var a : i32 = 2;
  // for (; ; a++) {
  //   break;
  // }
  auto* a = Var("a", ty.i32(), ast::StorageClass::kNone, Expr(2));
  auto* loop =
      For(nullptr, nullptr, Increment(Source{{56, 78}}, "a"), Block(Break()));
  WrapInFunction(a, loop);

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

}  // namespace
}  // namespace tint::resolver
