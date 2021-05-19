// Copyright 2021 The Tint Authors.
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

#include "src/transform/hlsl.h"

#include "src/transform/test_helper.h"

namespace tint {
namespace transform {
namespace {

using HlslTest = TransformTest;

TEST_F(HlslTest, PromoteArrayInitializerToConstVar_Basic) {
  auto* src = R"(
[[stage(vertex)]]
fn main() -> void {
  var f0 : f32 = 1.0;
  var f1 : f32 = 2.0;
  var f2 : f32 = 3.0;
  var f3 : f32 = 4.0;
  var i : f32 = array<f32, 4>(f0, f1, f2, f3)[2];
}
)";

  auto* expect = R"(
[[stage(vertex)]]
fn main() -> void {
  var f0 : f32 = 1.0;
  var f1 : f32 = 2.0;
  var f2 : f32 = 3.0;
  var f3 : f32 = 4.0;
  const tint_symbol_1 : array<f32, 4> = array<f32, 4>(f0, f1, f2, f3);
  var i : f32 = tint_symbol_1[2];
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, PromoteStructureInitializerToConstVar_Basic) {
  auto* src = R"(
struct S {
  a : i32;
  b : f32;
  c : vec3<f32>;
};

[[stage(vertex)]]
fn main() -> void {
  var x : f32 = S(1, 2.0, vec3<f32>()).b;
}
)";

  auto* expect = R"(
struct S {
  a : i32;
  b : f32;
  c : vec3<f32>;
};

[[stage(vertex)]]
fn main() -> void {
  const tint_symbol_1 : S = S(1, 2.0, vec3<f32>());
  var x : f32 = tint_symbol_1.b;
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, PromoteArrayInitializerToConstVar_ArrayInArray) {
  auto* src = R"(
[[stage(vertex)]]
fn main() -> void {
  var i : f32 = array<array<f32, 2>, 2>(array<f32, 2>(1.0, 2.0), array<f32, 2>(3.0, 4.0))[0][1];
}
)";

  auto* expect = R"(
[[stage(vertex)]]
fn main() -> void {
  const tint_symbol_1 : array<f32, 2> = array<f32, 2>(1.0, 2.0);
  const tint_symbol_2 : array<f32, 2> = array<f32, 2>(3.0, 4.0);
  const tint_symbol_3 : array<array<f32, 2>, 2> = array<array<f32, 2>, 2>(tint_symbol_1, tint_symbol_2);
  var i : f32 = tint_symbol_3[0][1];
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, PromoteStructureInitializerToConstVar_Nested) {
  auto* src = R"(
struct S1 {
  a : i32;
};

struct S2 {
  a : i32;
  b : S1;
  c : i32;
};

struct S3 {
  a : S2;
};

[[stage(vertex)]]
fn main() -> void {
  var x : i32 = S3(S2(1, S1(2), 3)).a.b.a;
}
)";

  auto* expect = R"(
struct S1 {
  a : i32;
};

struct S2 {
  a : i32;
  b : S1;
  c : i32;
};

struct S3 {
  a : S2;
};

[[stage(vertex)]]
fn main() -> void {
  const tint_symbol_1 : S1 = S1(2);
  const tint_symbol_4 : S2 = S2(1, tint_symbol_1, 3);
  const tint_symbol_8 : S3 = S3(tint_symbol_4);
  var x : i32 = tint_symbol_8.a.b.a;
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, PromoteInitializerToConstVar_Mixed) {
  auto* src = R"(
struct S1 {
  a : i32;
};

struct S2 {
  a : array<S1, 3>;
};

[[stage(vertex)]]
fn main() -> void {
  var x : i32 = S2(array<S1, 3>(S1(1), S1(2), S1(3))).a[1].a;
}
)";

  auto* expect = R"(
struct S1 {
  a : i32;
};

struct S2 {
  a : array<S1, 3>;
};

[[stage(vertex)]]
fn main() -> void {
  const tint_symbol_1 : S1 = S1(1);
  const tint_symbol_4 : S1 = S1(2);
  const tint_symbol_5 : S1 = S1(3);
  const tint_symbol_6 : array<S1, 3> = array<S1, 3>(tint_symbol_1, tint_symbol_4, tint_symbol_5);
  const tint_symbol_7 : S2 = S2(tint_symbol_6);
  var x : i32 = tint_symbol_7.a[1].a;
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, PromoteInitializerToConstVar_NoChangeOnVarDecl) {
  auto* src = R"(
struct S {
  a : i32;
  b : f32;
  c : i32;
};

[[stage(vertex)]]
fn main() -> void {
  var local_arr : array<f32, 4> = array<f32, 4>(0.0, 1.0, 2.0, 3.0);
  var local_str : S = S(1, 2.0, 3);
}

const module_arr : array<f32, 4> = array<f32, 4>(0.0, 1.0, 2.0, 3.0);

const module_str : S = S(1, 2.0, 3);
)";

  auto* expect = src;

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, PromoteArrayInitializerToConstVar_Bug406) {
  // See crbug.com/tint/406
  auto* src = R"(
[[block]]
struct Uniforms {
  transform : mat2x2<f32>;
};

[[group(0), binding(0)]] var<uniform> ubo : Uniforms;

[[builtin(vertex_index)]] var<in> vertex_index : u32;

[[builtin(position)]] var<out> position : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  const transform : mat2x2<f32> = ubo.transform;
  var coord : vec2<f32> = array<vec2<f32>, 3>(
      vec2<f32>(-1.0,  1.0),
      vec2<f32>( 1.0,  1.0),
      vec2<f32>(-1.0, -1.0)
  )[vertex_index];
  position = vec4<f32>(transform * coord, 0.0, 1.0);
}
)";

  auto* expect = R"(
[[block]]
struct Uniforms {
  transform : mat2x2<f32>;
};

[[group(0), binding(0)]] var<uniform> ubo : Uniforms;

[[builtin(vertex_index)]] var<in> vertex_index : u32;

[[builtin(position)]] var<out> position : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  const transform : mat2x2<f32> = ubo.transform;
  const tint_symbol_1 : array<vec2<f32>, 3> = array<vec2<f32>, 3>(vec2<f32>(-1.0, 1.0), vec2<f32>(1.0, 1.0), vec2<f32>(-1.0, -1.0));
  var coord : vec2<f32> = tint_symbol_1[vertex_index];
  position = vec4<f32>((transform * coord), 0.0, 1.0);
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(HlslTest, AddEmptyEntryPoint) {
  auto* src = R"()";

  auto* expect = R"(
[[stage(vertex)]]
fn _tint_unused_entry_point() -> void {
}
)";

  auto got = Run<Hlsl>(src);

  EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace transform
}  // namespace tint
