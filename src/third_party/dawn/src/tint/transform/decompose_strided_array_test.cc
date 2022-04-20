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

#include "src/tint/transform/decompose_strided_array.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/tint/program_builder.h"
#include "src/tint/transform/simplify_pointers.h"
#include "src/tint/transform/test_helper.h"
#include "src/tint/transform/unshadow.h"

namespace tint::transform {
namespace {

using DecomposeStridedArrayTest = TransformTest;
using f32 = ProgramBuilder::f32;

TEST_F(DecomposeStridedArrayTest, ShouldRunEmptyModule) {
  ProgramBuilder b;
  EXPECT_FALSE(ShouldRun<DecomposeStridedArray>(Program(std::move(b))));
}

TEST_F(DecomposeStridedArrayTest, ShouldRunNonStridedArray) {
  // var<private> arr : array<f32, 4>

  ProgramBuilder b;
  b.Global("arr", b.ty.array<f32, 4>(), ast::StorageClass::kPrivate);
  EXPECT_FALSE(ShouldRun<DecomposeStridedArray>(Program(std::move(b))));
}

TEST_F(DecomposeStridedArrayTest, ShouldRunDefaultStridedArray) {
  // var<private> arr : @stride(4) array<f32, 4>

  ProgramBuilder b;
  b.Global("arr", b.ty.array<f32, 4>(4), ast::StorageClass::kPrivate);
  EXPECT_TRUE(ShouldRun<DecomposeStridedArray>(Program(std::move(b))));
}

TEST_F(DecomposeStridedArrayTest, ShouldRunExplicitStridedArray) {
  // var<private> arr : @stride(16) array<f32, 4>

  ProgramBuilder b;
  b.Global("arr", b.ty.array<f32, 4>(16), ast::StorageClass::kPrivate);
  EXPECT_TRUE(ShouldRun<DecomposeStridedArray>(Program(std::move(b))));
}

TEST_F(DecomposeStridedArrayTest, Empty) {
  auto* src = R"()";
  auto* expect = src;

  auto got = Run<DecomposeStridedArray>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, PrivateDefaultStridedArray) {
  // var<private> arr : @stride(4) array<f32, 4>
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : @stride(4) array<f32, 4> = a;
  //   let b : f32 = arr[1];
  // }

  ProgramBuilder b;
  b.Global("arr", b.ty.array<f32, 4>(4), ast::StorageClass::kPrivate);
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.array<f32, 4>(4), b.Expr("arr"))),
             b.Decl(b.Const("b", b.ty.f32(), b.IndexAccessor("arr", 1))),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect = R"(
var<private> arr : array<f32, 4>;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : array<f32, 4> = arr;
  let b : f32 = arr[1];
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, PrivateStridedArray) {
  // var<private> arr : @stride(32) array<f32, 4>
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : @stride(32) array<f32, 4> = a;
  //   let b : f32 = arr[1];
  // }

  ProgramBuilder b;
  b.Global("arr", b.ty.array<f32, 4>(32), ast::StorageClass::kPrivate);
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.array<f32, 4>(32), b.Expr("arr"))),
             b.Decl(b.Const("b", b.ty.f32(), b.IndexAccessor("arr", 1))),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect = R"(
struct strided_arr {
  @size(32)
  el : f32,
}

var<private> arr : array<strided_arr, 4>;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : array<strided_arr, 4> = arr;
  let b : f32 = arr[1].el;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, ReadUniformStridedArray) {
  // struct S {
  //   a : @stride(32) array<f32, 4>,
  // };
  // @group(0) @binding(0) var<uniform> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : @stride(32) array<f32, 4> = s.a;
  //   let b : f32 = s.a[1];
  // }
  ProgramBuilder b;
  auto* S = b.Structure("S", {b.Member("a", b.ty.array<f32, 4>(32))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kUniform,
           b.GroupAndBinding(0, 0));
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.array<f32, 4>(32),
                            b.MemberAccessor("s", "a"))),
             b.Decl(b.Const("b", b.ty.f32(),
                            b.IndexAccessor(b.MemberAccessor("s", "a"), 1))),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect = R"(
struct strided_arr {
  @size(32)
  el : f32,
}

struct S {
  a : array<strided_arr, 4>,
}

@group(0) @binding(0) var<uniform> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : array<strided_arr, 4> = s.a;
  let b : f32 = s.a[1].el;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, ReadUniformDefaultStridedArray) {
  // struct S {
  //   a : @stride(16) array<vec4<f32>, 4>,
  // };
  // @group(0) @binding(0) var<uniform> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : @stride(16) array<vec4<f32>, 4> = s.a;
  //   let b : f32 = s.a[1][2];
  // }
  ProgramBuilder b;
  auto* S =
      b.Structure("S", {b.Member("a", b.ty.array(b.ty.vec4<f32>(), 4, 16))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kUniform,
           b.GroupAndBinding(0, 0));
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.array(b.ty.vec4<f32>(), 4, 16),
                            b.MemberAccessor("s", "a"))),
             b.Decl(b.Const(
                 "b", b.ty.f32(),
                 b.IndexAccessor(b.IndexAccessor(b.MemberAccessor("s", "a"), 1),
                                 2))),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect =
      R"(
struct S {
  a : array<vec4<f32>, 4>,
}

@group(0) @binding(0) var<uniform> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : array<vec4<f32>, 4> = s.a;
  let b : f32 = s.a[1][2];
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, ReadStorageStridedArray) {
  // struct S {
  //   a : @stride(32) array<f32, 4>,
  // };
  // @group(0) @binding(0) var<storage> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : @stride(32) array<f32, 4> = s.a;
  //   let b : f32 = s.a[1];
  // }
  ProgramBuilder b;
  auto* S = b.Structure("S", {b.Member("a", b.ty.array<f32, 4>(32))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           b.GroupAndBinding(0, 0));
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.array<f32, 4>(32),
                            b.MemberAccessor("s", "a"))),
             b.Decl(b.Const("b", b.ty.f32(),
                            b.IndexAccessor(b.MemberAccessor("s", "a"), 1))),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect = R"(
struct strided_arr {
  @size(32)
  el : f32,
}

struct S {
  a : array<strided_arr, 4>,
}

@group(0) @binding(0) var<storage> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : array<strided_arr, 4> = s.a;
  let b : f32 = s.a[1].el;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, ReadStorageDefaultStridedArray) {
  // struct S {
  //   a : @stride(4) array<f32, 4>,
  // };
  // @group(0) @binding(0) var<storage> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : @stride(4) array<f32, 4> = s.a;
  //   let b : f32 = s.a[1];
  // }
  ProgramBuilder b;
  auto* S = b.Structure("S", {b.Member("a", b.ty.array<f32, 4>(4))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           b.GroupAndBinding(0, 0));
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.array<f32, 4>(4),
                            b.MemberAccessor("s", "a"))),
             b.Decl(b.Const("b", b.ty.f32(),
                            b.IndexAccessor(b.MemberAccessor("s", "a"), 1))),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect = R"(
struct S {
  a : array<f32, 4>,
}

@group(0) @binding(0) var<storage> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : array<f32, 4> = s.a;
  let b : f32 = s.a[1];
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, WriteStorageStridedArray) {
  // struct S {
  //   a : @stride(32) array<f32, 4>,
  // };
  // @group(0) @binding(0) var<storage, read_write> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   s.a = @stride(32) array<f32, 4>();
  //   s.a = @stride(32) array<f32, 4>(1.0, 2.0, 3.0, 4.0);
  //   s.a[1] = 5.0;
  // }
  ProgramBuilder b;
  auto* S = b.Structure("S", {b.Member("a", b.ty.array<f32, 4>(32))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           ast::Access::kReadWrite, b.GroupAndBinding(0, 0));
  b.Func(
      "f", {}, b.ty.void_(),
      {
          b.Assign(b.MemberAccessor("s", "a"),
                   b.Construct(b.ty.array<f32, 4>(32))),
          b.Assign(b.MemberAccessor("s", "a"),
                   b.Construct(b.ty.array<f32, 4>(32), 1.0f, 2.0f, 3.0f, 4.0f)),
          b.Assign(b.IndexAccessor(b.MemberAccessor("s", "a"), 1), 5.0f),
      },
      {
          b.Stage(ast::PipelineStage::kCompute),
          b.WorkgroupSize(1),
      });

  auto* expect =
      R"(
struct strided_arr {
  @size(32)
  el : f32,
}

struct S {
  a : array<strided_arr, 4>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  s.a = array<strided_arr, 4>();
  s.a = array<strided_arr, 4>(strided_arr(1.0), strided_arr(2.0), strided_arr(3.0), strided_arr(4.0));
  s.a[1].el = 5.0;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, WriteStorageDefaultStridedArray) {
  // struct S {
  //   a : @stride(4) array<f32, 4>,
  // };
  // @group(0) @binding(0) var<storage, read_write> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   s.a = @stride(4) array<f32, 4>();
  //   s.a = @stride(4) array<f32, 4>(1.0, 2.0, 3.0, 4.0);
  //   s.a[1] = 5.0;
  // }
  ProgramBuilder b;
  auto* S = b.Structure("S", {b.Member("a", b.ty.array<f32, 4>(4))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           ast::Access::kReadWrite, b.GroupAndBinding(0, 0));
  b.Func(
      "f", {}, b.ty.void_(),
      {
          b.Assign(b.MemberAccessor("s", "a"),
                   b.Construct(b.ty.array<f32, 4>(4))),
          b.Assign(b.MemberAccessor("s", "a"),
                   b.Construct(b.ty.array<f32, 4>(4), 1.0f, 2.0f, 3.0f, 4.0f)),
          b.Assign(b.IndexAccessor(b.MemberAccessor("s", "a"), 1), 5.0f),
      },
      {
          b.Stage(ast::PipelineStage::kCompute),
          b.WorkgroupSize(1),
      });

  auto* expect =
      R"(
struct S {
  a : array<f32, 4>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  s.a = array<f32, 4>();
  s.a = array<f32, 4>(1.0, 2.0, 3.0, 4.0);
  s.a[1] = 5.0;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, ReadWriteViaPointerLets) {
  // struct S {
  //   a : @stride(32) array<f32, 4>,
  // };
  // @group(0) @binding(0) var<storage, read_write> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a = &s.a;
  //   let b = &*&*(a);
  //   let c = *b;
  //   let d = (*b)[1];
  //   (*b) = @stride(32) array<f32, 4>(1.0, 2.0, 3.0, 4.0);
  //   (*b)[1] = 5.0;
  // }
  ProgramBuilder b;
  auto* S = b.Structure("S", {b.Member("a", b.ty.array<f32, 4>(32))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           ast::Access::kReadWrite, b.GroupAndBinding(0, 0));
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", nullptr,
                            b.AddressOf(b.MemberAccessor("s", "a")))),
             b.Decl(b.Const("b", nullptr,
                            b.AddressOf(b.Deref(b.AddressOf(b.Deref("a")))))),
             b.Decl(b.Const("c", nullptr, b.Deref("b"))),
             b.Decl(b.Const("d", nullptr, b.IndexAccessor(b.Deref("b"), 1))),
             b.Assign(b.Deref("b"), b.Construct(b.ty.array<f32, 4>(32), 1.0f,
                                                2.0f, 3.0f, 4.0f)),
             b.Assign(b.IndexAccessor(b.Deref("b"), 1), 5.0f),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect =
      R"(
struct strided_arr {
  @size(32)
  el : f32,
}

struct S {
  a : array<strided_arr, 4>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let c = s.a;
  let d = s.a[1].el;
  s.a = array<strided_arr, 4>(strided_arr(1.0), strided_arr(2.0), strided_arr(3.0), strided_arr(4.0));
  s.a[1].el = 5.0;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, PrivateAliasedStridedArray) {
  // type ARR = @stride(32) array<f32, 4>;
  // struct S {
  //   a : ARR,
  // };
  // @group(0) @binding(0) var<storage, read_write> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : ARR = s.a;
  //   let b : f32 = s.a[1];
  //   s.a = ARR();
  //   s.a = ARR(1.0, 2.0, 3.0, 4.0);
  //   s.a[1] = 5.0;
  // }
  ProgramBuilder b;
  b.Alias("ARR", b.ty.array<f32, 4>(32));
  auto* S = b.Structure("S", {b.Member("a", b.ty.type_name("ARR"))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           ast::Access::kReadWrite, b.GroupAndBinding(0, 0));
  b.Func(
      "f", {}, b.ty.void_(),
      {
          b.Decl(
              b.Const("a", b.ty.type_name("ARR"), b.MemberAccessor("s", "a"))),
          b.Decl(b.Const("b", b.ty.f32(),
                         b.IndexAccessor(b.MemberAccessor("s", "a"), 1))),
          b.Assign(b.MemberAccessor("s", "a"),
                   b.Construct(b.ty.type_name("ARR"))),
          b.Assign(b.MemberAccessor("s", "a"),
                   b.Construct(b.ty.type_name("ARR"), 1.0f, 2.0f, 3.0f, 4.0f)),
          b.Assign(b.IndexAccessor(b.MemberAccessor("s", "a"), 1), 5.0f),
      },
      {
          b.Stage(ast::PipelineStage::kCompute),
          b.WorkgroupSize(1),
      });

  auto* expect = R"(
struct strided_arr {
  @size(32)
  el : f32,
}

type ARR = array<strided_arr, 4>;

struct S {
  a : ARR,
}

@group(0) @binding(0) var<storage, read_write> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : ARR = s.a;
  let b : f32 = s.a[1].el;
  s.a = ARR();
  s.a = ARR(strided_arr(1.0), strided_arr(2.0), strided_arr(3.0), strided_arr(4.0));
  s.a[1].el = 5.0;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}

TEST_F(DecomposeStridedArrayTest, PrivateNestedStridedArray) {
  // type ARR_A = @stride(8) array<f32, 2>;
  // type ARR_B = @stride(128) array<@stride(16) array<ARR_A, 3>, 4>;
  // struct S {
  //   a : ARR_B,
  // };
  // @group(0) @binding(0) var<storage, read_write> s : S;
  //
  // @stage(compute) @workgroup_size(1)
  // fn f() {
  //   let a : ARR_B = s.a;
  //   let b : array<@stride(8) array<f32, 2>, 3> = s.a[3];
  //   let c = s.a[3][2];
  //   let d = s.a[3][2][1];
  //   s.a = ARR_B();
  //   s.a[3][2][1] = 5.0;
  // }

  ProgramBuilder b;
  b.Alias("ARR_A", b.ty.array<f32, 2>(8));
  b.Alias("ARR_B",
          b.ty.array(                                      //
              b.ty.array(b.ty.type_name("ARR_A"), 3, 16),  //
              4, 128));
  auto* S = b.Structure("S", {b.Member("a", b.ty.type_name("ARR_B"))});
  b.Global("s", b.ty.Of(S), ast::StorageClass::kStorage,
           ast::Access::kReadWrite, b.GroupAndBinding(0, 0));
  b.Func("f", {}, b.ty.void_(),
         {
             b.Decl(b.Const("a", b.ty.type_name("ARR_B"),
                            b.MemberAccessor("s", "a"))),
             b.Decl(b.Const("b", b.ty.array(b.ty.type_name("ARR_A"), 3, 16),
                            b.IndexAccessor(                 //
                                b.MemberAccessor("s", "a"),  //
                                3))),
             b.Decl(b.Const("c", b.ty.type_name("ARR_A"),
                            b.IndexAccessor(                     //
                                b.IndexAccessor(                 //
                                    b.MemberAccessor("s", "a"),  //
                                    3),
                                2))),
             b.Decl(b.Const("d", b.ty.f32(),
                            b.IndexAccessor(                         //
                                b.IndexAccessor(                     //
                                    b.IndexAccessor(                 //
                                        b.MemberAccessor("s", "a"),  //
                                        3),
                                    2),
                                1))),
             b.Assign(b.MemberAccessor("s", "a"),
                      b.Construct(b.ty.type_name("ARR_B"))),
             b.Assign(b.IndexAccessor(                         //
                          b.IndexAccessor(                     //
                              b.IndexAccessor(                 //
                                  b.MemberAccessor("s", "a"),  //
                                  3),
                              2),
                          1),
                      5.0f),
         },
         {
             b.Stage(ast::PipelineStage::kCompute),
             b.WorkgroupSize(1),
         });

  auto* expect =
      R"(
struct strided_arr {
  @size(8)
  el : f32,
}

type ARR_A = array<strided_arr, 2>;

struct strided_arr_1 {
  @size(128)
  el : array<ARR_A, 3>,
}

type ARR_B = array<strided_arr_1, 4>;

struct S {
  a : ARR_B,
}

@group(0) @binding(0) var<storage, read_write> s : S;

@stage(compute) @workgroup_size(1)
fn f() {
  let a : ARR_B = s.a;
  let b : array<ARR_A, 3> = s.a[3].el;
  let c : ARR_A = s.a[3].el[2];
  let d : f32 = s.a[3].el[2][1].el;
  s.a = ARR_B();
  s.a[3].el[2][1].el = 5.0;
}
)";

  auto got = Run<Unshadow, SimplifyPointers, DecomposeStridedArray>(
      Program(std::move(b)));

  EXPECT_EQ(expect, str(got));
}
}  // namespace
}  // namespace tint::transform
