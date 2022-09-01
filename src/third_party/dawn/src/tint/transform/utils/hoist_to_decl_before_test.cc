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

#include <utility>

#include "gtest/gtest-spi.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/if_statement.h"
#include "src/tint/sem/statement.h"
#include "src/tint/transform/test_helper.h"
#include "src/tint/transform/utils/hoist_to_decl_before.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::transform {
namespace {

using HoistToDeclBeforeTest = ::testing::Test;

TEST_F(HoistToDeclBeforeTest, VarInit) {
    // fn f() {
    //     var a = 1;
    // }
    ProgramBuilder b;
    auto* expr = b.Expr(1_i);
    auto* var = b.Decl(b.Var("a", nullptr, expr));
    b.Func("f", {}, b.ty.void_(), {var});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  let tint_symbol = 1i;
  var a = tint_symbol;
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, ForLoopInit) {
    // fn f() {
    //     for(var a = 1i; true; ) {
    //     }
    // }
    ProgramBuilder b;
    auto* expr = b.Expr(1_i);
    auto* s = b.For(b.Decl(b.Var("a", nullptr, expr)), b.Expr(true), {}, b.Block());
    b.Func("f", {}, b.ty.void_(), {s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  let tint_symbol = 1i;
  for(var a = tint_symbol; true; ) {
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, ForLoopCond) {
    // fn f() {
    //     var a : bool;
    //     for(; a; ) {
    //     }
    // }
    ProgramBuilder b;
    auto* var = b.Decl(b.Var("a", b.ty.bool_()));
    auto* expr = b.Expr("a");
    auto* s = b.For({}, expr, {}, b.Block());
    b.Func("f", {}, b.ty.void_(), {var, s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  var a : bool;
  loop {
    let tint_symbol = a;
    if (!(tint_symbol)) {
      break;
    }
    {
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, ForLoopCont) {
    // fn f() {
    //     for(; true; var a = 1i) {
    //     }
    // }
    ProgramBuilder b;
    auto* expr = b.Expr(1_i);
    auto* s = b.For({}, b.Expr(true), b.Decl(b.Var("a", nullptr, expr)), b.Block());
    b.Func("f", {}, b.ty.void_(), {s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  loop {
    if (!(true)) {
      break;
    }
    {
    }

    continuing {
      let tint_symbol = 1i;
      var a = tint_symbol;
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, ElseIf) {
    // fn f() {
    //     var a : bool;
    //     if (true) {
    //     } else if (a) {
    //     } else {
    //     }
    // }
    ProgramBuilder b;
    auto* var = b.Decl(b.Var("a", b.ty.bool_()));
    auto* expr = b.Expr("a");
    auto* s = b.If(b.Expr(true), b.Block(),      //
                   b.Else(b.If(expr, b.Block(),  //
                               b.Else(b.Block()))));
    b.Func("f", {}, b.ty.void_(), {var, s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  var a : bool;
  if (true) {
  } else {
    let tint_symbol = a;
    if (tint_symbol) {
    } else {
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, Array1D) {
    // fn f() {
    //     var a : array<i32, 10>;
    //     var b = a[0];
    // }
    ProgramBuilder b;
    auto* var1 = b.Decl(b.Var("a", b.ty.array<i32, 10>()));
    auto* expr = b.IndexAccessor("a", 0_i);
    auto* var2 = b.Decl(b.Var("b", nullptr, expr));
    b.Func("f", {}, b.ty.void_(), {var1, var2});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  var a : array<i32, 10u>;
  let tint_symbol = a[0i];
  var b = tint_symbol;
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, Array2D) {
    // fn f() {
    //     var a : array<array<i32, 10>, 10>;
    //     var b = a[0][0];
    // }
    ProgramBuilder b;

    auto* var1 = b.Decl(b.Var("a", b.ty.array(b.ty.array<i32, 10>(), 10_i)));
    auto* expr = b.IndexAccessor(b.IndexAccessor("a", 0_i), 0_i);
    auto* var2 = b.Decl(b.Var("b", nullptr, expr));
    b.Func("f", {}, b.ty.void_(), {var1, var2});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Add(sem_expr, expr, true);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  var a : array<array<i32, 10u>, 10i>;
  let tint_symbol = a[0i][0i];
  var b = tint_symbol;
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, Prepare_ForLoopCond) {
    // fn f() {
    //     var a : bool;
    //     for(; a; ) {
    //     }
    // }
    ProgramBuilder b;
    auto* var = b.Decl(b.Var("a", b.ty.bool_()));
    auto* expr = b.Expr("a");
    auto* s = b.For({}, expr, {}, b.Block());
    b.Func("f", {}, b.ty.void_(), {var, s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Prepare(sem_expr);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  var a : bool;
  loop {
    if (!(a)) {
      break;
    }
    {
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, Prepare_ForLoopCont) {
    // fn f() {
    //     for(; true; var a = 1i) {
    //     }
    // }
    ProgramBuilder b;
    auto* expr = b.Expr(1_i);
    auto* s = b.For({}, b.Expr(true), b.Decl(b.Var("a", nullptr, expr)), b.Block());
    b.Func("f", {}, b.ty.void_(), {s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Prepare(sem_expr);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  loop {
    if (!(true)) {
      break;
    }
    {
    }

    continuing {
      var a = 1i;
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, Prepare_ElseIf) {
    // fn f() {
    //     var a : bool;
    //     if (true) {
    //     } else if (a) {
    //     } else {
    //     }
    // }
    ProgramBuilder b;
    auto* var = b.Decl(b.Var("a", b.ty.bool_()));
    auto* expr = b.Expr("a");
    auto* s = b.If(b.Expr(true), b.Block(),      //
                   b.Else(b.If(expr, b.Block(),  //
                               b.Else(b.Block()))));
    b.Func("f", {}, b.ty.void_(), {var, s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* sem_expr = ctx.src->Sem().Get(expr);
    hoistToDeclBefore.Prepare(sem_expr);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn f() {
  var a : bool;
  if (true) {
  } else {
    if (a) {
    } else {
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, InsertBefore_Block) {
    // fn foo() {
    // }
    // fn f() {
    //     var a = 1i;
    // }
    ProgramBuilder b;
    b.Func("foo", {}, b.ty.void_(), {});
    auto* var = b.Decl(b.Var("a", nullptr, b.Expr(1_i)));
    b.Func("f", {}, b.ty.void_(), {var});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* before_stmt = ctx.src->Sem().Get(var);
    auto* new_stmt = ctx.dst->CallStmt(ctx.dst->Call("foo"));
    hoistToDeclBefore.InsertBefore(before_stmt, new_stmt);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn foo() {
}

fn f() {
  foo();
  var a = 1i;
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, InsertBefore_ForLoopInit) {
    // fn foo() {
    // }
    // fn f() {
    //     for(var a = 1i; true;) {
    //     }
    // }
    ProgramBuilder b;
    b.Func("foo", {}, b.ty.void_(), {});
    auto* var = b.Decl(b.Var("a", nullptr, b.Expr(1_i)));
    auto* s = b.For(var, b.Expr(true), {}, b.Block());
    b.Func("f", {}, b.ty.void_(), {s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* before_stmt = ctx.src->Sem().Get(var);
    auto* new_stmt = ctx.dst->CallStmt(ctx.dst->Call("foo"));
    hoistToDeclBefore.InsertBefore(before_stmt, new_stmt);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn foo() {
}

fn f() {
  foo();
  for(var a = 1i; true; ) {
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, InsertBefore_ForLoopCont) {
    // fn foo() {
    // }
    // fn f() {
    //     var a = 1i;
    //     for(; true; a+=1i) {
    //     }
    // }
    ProgramBuilder b;
    b.Func("foo", {}, b.ty.void_(), {});
    auto* var = b.Decl(b.Var("a", nullptr, b.Expr(1_i)));
    auto* cont = b.CompoundAssign("a", b.Expr(1_i), ast::BinaryOp::kAdd);
    auto* s = b.For({}, b.Expr(true), cont, b.Block());
    b.Func("f", {}, b.ty.void_(), {var, s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* before_stmt = ctx.src->Sem().Get(cont->As<ast::Statement>());
    auto* new_stmt = ctx.dst->CallStmt(ctx.dst->Call("foo"));
    hoistToDeclBefore.InsertBefore(before_stmt, new_stmt);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn foo() {
}

fn f() {
  var a = 1i;
  loop {
    if (!(true)) {
      break;
    }
    {
    }

    continuing {
      foo();
      a += 1i;
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

TEST_F(HoistToDeclBeforeTest, InsertBefore_ElseIf) {
    // fn foo() {
    // }
    // fn f() {
    //     var a : bool;
    //     if (true) {
    //     } else if (a) {
    //     } else {
    //     }
    // }
    ProgramBuilder b;
    b.Func("foo", {}, b.ty.void_(), {});
    auto* var = b.Decl(b.Var("a", b.ty.bool_()));
    auto* elseif = b.If(b.Expr("a"), b.Block(), b.Else(b.Block()));
    auto* s = b.If(b.Expr(true), b.Block(),  //
                   b.Else(elseif));
    b.Func("f", {}, b.ty.void_(), {var, s});

    Program original(std::move(b));
    ProgramBuilder cloned_b;
    CloneContext ctx(&cloned_b, &original);

    HoistToDeclBefore hoistToDeclBefore(ctx);
    auto* before_stmt = ctx.src->Sem().Get(elseif);
    auto* new_stmt = ctx.dst->CallStmt(ctx.dst->Call("foo"));
    hoistToDeclBefore.InsertBefore(before_stmt, new_stmt);
    hoistToDeclBefore.Apply();

    ctx.Clone();
    Program cloned(std::move(cloned_b));

    auto* expect = R"(
fn foo() {
}

fn f() {
  var a : bool;
  if (true) {
  } else {
    foo();
    if (a) {
    } else {
    }
  }
}
)";

    EXPECT_EQ(expect, str(cloned));
}

}  // namespace
}  // namespace tint::transform
