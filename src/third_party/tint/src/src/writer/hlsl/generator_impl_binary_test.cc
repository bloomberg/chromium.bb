// Copyright 2020 The Tint Authors.
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

#include "src/ast/call_statement.h"
#include "src/ast/variable_decl_statement.h"
#include "src/writer/hlsl/test_helper.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

using HlslGeneratorImplTest_Binary = TestHelper;

struct BinaryData {
  const char* result;
  ast::BinaryOp op;
};
inline std::ostream& operator<<(std::ostream& out, BinaryData data) {
  out << data.op;
  return out;
}

using HlslBinaryTest = TestParamHelper<BinaryData>;
TEST_P(HlslBinaryTest, Emit_f32) {
  auto params = GetParam();

  // Skip ops that are illegal for this type
  if (params.op == ast::BinaryOp::kAnd || params.op == ast::BinaryOp::kOr ||
      params.op == ast::BinaryOp::kXor ||
      params.op == ast::BinaryOp::kShiftLeft ||
      params.op == ast::BinaryOp::kShiftRight) {
    return;
  }

  Global("left", ty.f32(), ast::StorageClass::kFunction);
  Global("right", ty.f32(), ast::StorageClass::kFunction);

  auto* left = Expr("left");
  auto* right = Expr("right");

  auto* expr = create<ast::BinaryExpression>(params.op, left, right);

  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), params.result);
}
TEST_P(HlslBinaryTest, Emit_u32) {
  auto params = GetParam();

  Global("left", ty.u32(), ast::StorageClass::kFunction);
  Global("right", ty.u32(), ast::StorageClass::kFunction);

  auto* left = Expr("left");
  auto* right = Expr("right");

  auto* expr = create<ast::BinaryExpression>(params.op, left, right);

  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), params.result);
}
TEST_P(HlslBinaryTest, Emit_i32) {
  auto params = GetParam();

  // Skip ops that are illegal for this type
  if (params.op == ast::BinaryOp::kShiftLeft ||
      params.op == ast::BinaryOp::kShiftRight) {
    return;
  }

  Global("left", ty.i32(), ast::StorageClass::kFunction);
  Global("right", ty.i32(), ast::StorageClass::kFunction);

  auto* left = Expr("left");
  auto* right = Expr("right");

  auto* expr = create<ast::BinaryExpression>(params.op, left, right);

  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest,
    HlslBinaryTest,
    testing::Values(
        BinaryData{"(left & right)", ast::BinaryOp::kAnd},
        BinaryData{"(left | right)", ast::BinaryOp::kOr},
        BinaryData{"(left ^ right)", ast::BinaryOp::kXor},
        BinaryData{"(left == right)", ast::BinaryOp::kEqual},
        BinaryData{"(left != right)", ast::BinaryOp::kNotEqual},
        BinaryData{"(left < right)", ast::BinaryOp::kLessThan},
        BinaryData{"(left > right)", ast::BinaryOp::kGreaterThan},
        BinaryData{"(left <= right)", ast::BinaryOp::kLessThanEqual},
        BinaryData{"(left >= right)", ast::BinaryOp::kGreaterThanEqual},
        BinaryData{"(left << right)", ast::BinaryOp::kShiftLeft},
        BinaryData{"(left >> right)", ast::BinaryOp::kShiftRight},
        BinaryData{"(left + right)", ast::BinaryOp::kAdd},
        BinaryData{"(left - right)", ast::BinaryOp::kSubtract},
        BinaryData{"(left * right)", ast::BinaryOp::kMultiply},
        BinaryData{"(left / right)", ast::BinaryOp::kDivide},
        BinaryData{"(left % right)", ast::BinaryOp::kModulo}));

TEST_F(HlslGeneratorImplTest_Binary, Multiply_VectorScalar) {
  auto* lhs = vec3<f32>(1.f, 1.f, 1.f);
  auto* rhs = Expr(1.f);

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);

  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(),
            "(float3(1.0f, 1.0f, 1.0f) * "
            "1.0f)");
}

TEST_F(HlslGeneratorImplTest_Binary, Multiply_ScalarVector) {
  auto* lhs = Expr(1.f);
  auto* rhs = vec3<f32>(1.f, 1.f, 1.f);

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);

  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(),
            "(1.0f * float3(1.0f, 1.0f, "
            "1.0f))");
}

TEST_F(HlslGeneratorImplTest_Binary, Multiply_MatrixScalar) {
  Global("mat", ty.mat3x3<f32>(), ast::StorageClass::kFunction);
  auto* lhs = Expr("mat");
  auto* rhs = Expr(1.f);

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "(mat * 1.0f)");
}

TEST_F(HlslGeneratorImplTest_Binary, Multiply_ScalarMatrix) {
  Global("mat", ty.mat3x3<f32>(), ast::StorageClass::kFunction);
  auto* lhs = Expr(1.f);
  auto* rhs = Expr("mat");

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "(1.0f * mat)");
}

TEST_F(HlslGeneratorImplTest_Binary, Multiply_MatrixVector) {
  Global("mat", ty.mat3x3<f32>(), ast::StorageClass::kFunction);
  auto* lhs = Expr("mat");
  auto* rhs = vec3<f32>(1.f, 1.f, 1.f);

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "mul(float3(1.0f, 1.0f, 1.0f), mat)");
}

TEST_F(HlslGeneratorImplTest_Binary, Multiply_VectorMatrix) {
  Global("mat", ty.mat3x3<f32>(), ast::StorageClass::kFunction);
  auto* lhs = vec3<f32>(1.f, 1.f, 1.f);
  auto* rhs = Expr("mat");

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "mul(mat, float3(1.0f, 1.0f, 1.0f))");
}

TEST_F(HlslGeneratorImplTest_Binary, Multiply_MatrixMatrix) {
  Global("mat", ty.mat3x3<f32>(), ast::StorageClass::kFunction);
  auto* lhs = Expr("mat");
  auto* rhs = Expr("mat");

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kMultiply, lhs, rhs);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  EXPECT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "mul(mat, mat)");
}

TEST_F(HlslGeneratorImplTest_Binary, Logical_And) {
  auto* left = Expr("left");
  auto* right = Expr("right");

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalAnd, left, right);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "(_tint_tmp)");
  EXPECT_EQ(pre_result(), R"(bool _tint_tmp = left;
if (_tint_tmp) {
  _tint_tmp = right;
}
)");
}

TEST_F(HlslGeneratorImplTest_Binary, Logical_Multi) {
  // (a && b) || (c || d)
  auto* a = Expr("a");
  auto* b = Expr("b");
  auto* c = Expr("c");
  auto* d = Expr("d");

  auto* expr = create<ast::BinaryExpression>(
      ast::BinaryOp::kLogicalOr,
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalAnd, a, b),
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, c, d));

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "(_tint_tmp_0)");
  EXPECT_EQ(pre_result(), R"(bool _tint_tmp = a;
if (_tint_tmp) {
  _tint_tmp = b;
}
bool _tint_tmp_0 = (_tint_tmp);
if (!_tint_tmp_0) {
  bool _tint_tmp_1 = c;
  if (!_tint_tmp_1) {
    _tint_tmp_1 = d;
  }
  _tint_tmp_0 = (_tint_tmp_1);
}
)");
}

TEST_F(HlslGeneratorImplTest_Binary, Logical_Or) {
  auto* left = Expr("left");
  auto* right = Expr("right");

  auto* expr =
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, left, right);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(result(), "(_tint_tmp)");
  EXPECT_EQ(pre_result(), R"(bool _tint_tmp = left;
if (!_tint_tmp) {
  _tint_tmp = right;
}
)");
}

TEST_F(HlslGeneratorImplTest_Binary, If_WithLogical) {
  // if (a && b) {
  //   return 1;
  // } else if (b || c) {
  //   return 2;
  // } else {
  //   return 3;
  // }

  auto* body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(Expr(3)),
  });
  auto* else_stmt = create<ast::ElseStatement>(nullptr, body);

  body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(Expr(2)),
  });
  auto* else_if_stmt = create<ast::ElseStatement>(
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, Expr("b"),
                                    Expr("c")),
      body);

  body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(Expr(1)),
  });

  auto* expr = create<ast::IfStatement>(
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalAnd, Expr("a"),
                                    Expr("b")),
      body,
      ast::ElseStatementList{
          else_if_stmt,
          else_stmt,
      });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(out, expr)) << gen.error();
  EXPECT_EQ(result(), R"(bool _tint_tmp = a;
if (_tint_tmp) {
  _tint_tmp = b;
}
if ((_tint_tmp)) {
  return 1;
} else {
  bool _tint_tmp_0 = b;
  if (!_tint_tmp_0) {
    _tint_tmp_0 = c;
  }
  if ((_tint_tmp_0)) {
    return 2;
  } else {
    return 3;
  }
}
)");
}

TEST_F(HlslGeneratorImplTest_Binary, Return_WithLogical) {
  // return (a && b) || c;
  auto* a = Expr("a");
  auto* b = Expr("b");
  auto* c = Expr("c");

  auto* expr = create<ast::ReturnStatement>(create<ast::BinaryExpression>(
      ast::BinaryOp::kLogicalOr,
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalAnd, a, b), c));

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(out, expr)) << gen.error();
  EXPECT_EQ(result(), R"(bool _tint_tmp = a;
if (_tint_tmp) {
  _tint_tmp = b;
}
bool _tint_tmp_0 = (_tint_tmp);
if (!_tint_tmp_0) {
  _tint_tmp_0 = c;
}
return (_tint_tmp_0);
)");
}

TEST_F(HlslGeneratorImplTest_Binary, Assign_WithLogical) {
  // a = (b || c) && d;
  auto* a = Expr("a");
  auto* b = Expr("b");
  auto* c = Expr("c");
  auto* d = Expr("d");

  auto* expr = create<ast::AssignmentStatement>(
      a,
      create<ast::BinaryExpression>(
          ast::BinaryOp::kLogicalAnd,
          create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, b, c), d));

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(out, expr)) << gen.error();
  EXPECT_EQ(result(), R"(bool _tint_tmp = b;
if (!_tint_tmp) {
  _tint_tmp = c;
}
bool _tint_tmp_0 = (_tint_tmp);
if (_tint_tmp_0) {
  _tint_tmp_0 = d;
}
a = (_tint_tmp_0);
)");
}

TEST_F(HlslGeneratorImplTest_Binary, Decl_WithLogical) {
  // var a : bool = (b && c) || d;

  auto* b_decl = Decl(Var("b", ty.bool_(), ast::StorageClass::kFunction));
  auto* c_decl = Decl(Var("c", ty.bool_(), ast::StorageClass::kFunction));
  auto* d_decl = Decl(Var("d", ty.bool_(), ast::StorageClass::kFunction));

  auto* b = Expr("b");
  auto* c = Expr("c");
  auto* d = Expr("d");

  auto* var = Var(
      "a", ty.bool_(), ast::StorageClass::kFunction,
      create<ast::BinaryExpression>(
          ast::BinaryOp::kLogicalOr,
          create<ast::BinaryExpression>(ast::BinaryOp::kLogicalAnd, b, c), d));

  auto* decl = Decl(var);
  WrapInFunction(b_decl, c_decl, d_decl, Decl(var));

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(out, decl)) << gen.error();
  EXPECT_EQ(result(), R"(bool _tint_tmp = b;
if (_tint_tmp) {
  _tint_tmp = c;
}
bool _tint_tmp_0 = (_tint_tmp);
if (!_tint_tmp_0) {
  _tint_tmp_0 = d;
}
bool a = (_tint_tmp_0);
)");
}

TEST_F(HlslGeneratorImplTest_Binary, Bitcast_WithLogical) {
  // as<i32>(a && (b || c))

  auto* a = Expr("a");
  auto* b = Expr("b");
  auto* c = Expr("c");

  auto* expr = create<ast::BitcastExpression>(
      ty.i32(),
      create<ast::BinaryExpression>(
          ast::BinaryOp::kLogicalAnd, a,
          create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, b, c)));

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, expr)) << gen.error();
  EXPECT_EQ(pre_result(), R"(bool _tint_tmp = a;
if (_tint_tmp) {
  bool _tint_tmp_0 = b;
  if (!_tint_tmp_0) {
    _tint_tmp_0 = c;
  }
  _tint_tmp = (_tint_tmp_0);
}
)");
  EXPECT_EQ(result(), R"(asint((_tint_tmp)))");
}

TEST_F(HlslGeneratorImplTest_Binary, Call_WithLogical) {
  // foo(a && b, c || d, (a || c) && (b || d))

  Func("foo", ast::VariableList{}, ty.void_(), ast::StatementList{},
       ast::DecorationList{});
  Global("a", ty.bool_(), ast::StorageClass::kInput);
  Global("b", ty.bool_(), ast::StorageClass::kInput);
  Global("c", ty.bool_(), ast::StorageClass::kInput);
  Global("d", ty.bool_(), ast::StorageClass::kInput);

  ast::ExpressionList params;
  params.push_back(create<ast::BinaryExpression>(ast::BinaryOp::kLogicalAnd,
                                                 Expr("a"), Expr("b")));
  params.push_back(create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr,
                                                 Expr("c"), Expr("d")));
  params.push_back(create<ast::BinaryExpression>(
      ast::BinaryOp::kLogicalAnd,
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, Expr("a"),
                                    Expr("c")),
      create<ast::BinaryExpression>(ast::BinaryOp::kLogicalOr, Expr("b"),
                                    Expr("d"))));

  auto* expr = create<ast::CallStatement>(Call("foo", params));
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(out, expr)) << gen.error();
  EXPECT_EQ(result(), R"(bool _tint_tmp = a;
if (_tint_tmp) {
  _tint_tmp = b;
}
bool _tint_tmp_0 = c;
if (!_tint_tmp_0) {
  _tint_tmp_0 = d;
}
bool _tint_tmp_1 = a;
if (!_tint_tmp_1) {
  _tint_tmp_1 = c;
}
bool _tint_tmp_2 = (_tint_tmp_1);
if (_tint_tmp_2) {
  bool _tint_tmp_3 = b;
  if (!_tint_tmp_3) {
    _tint_tmp_3 = d;
  }
  _tint_tmp_2 = (_tint_tmp_3);
}
foo((_tint_tmp), (_tint_tmp_0), (_tint_tmp_2));
)");
}

}  // namespace
}  // namespace hlsl
}  // namespace writer
}  // namespace tint
