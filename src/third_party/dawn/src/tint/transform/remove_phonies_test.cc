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

#include "src/tint/transform/remove_phonies.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using RemovePhoniesTest = TransformTest;

TEST_F(RemovePhoniesTest, ShouldRunEmptyModule) {
  auto* src = R"()";

  EXPECT_FALSE(ShouldRun<RemovePhonies>(src));
}

TEST_F(RemovePhoniesTest, ShouldRunHasPhony) {
  auto* src = R"(
fn f() {
  _ = 1;
}
)";

  EXPECT_TRUE(ShouldRun<RemovePhonies>(src));
}

TEST_F(RemovePhoniesTest, EmptyModule) {
  auto* src = "";
  auto* expect = "";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, NoSideEffects) {
  auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

fn f() {
  var v : i32;
  _ = &v;
  _ = 1;
  _ = 1 + 2;
  _ = t;
  _ = u32(3.0);
  _ = f32(i32(4u));
  _ = vec2<f32>(5.0);
  _ = vec3<i32>(6, 7, 8);
  _ = mat2x2<f32>(9.0, 10.0, 11.0, 12.0);
}
)";

  auto* expect = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

fn f() {
  var v : i32;
}
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, SingleSideEffects) {
  auto* src = R"(
fn neg(a : i32) -> i32 {
  return -(a);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn f() {
  _ = neg(1);
  _ = add(2, 3);
  _ = add(neg(4), neg(5));
  _ = u32(neg(6));
  _ = f32(add(7, 8));
  _ = vec2<f32>(f32(neg(9)));
  _ = vec3<i32>(1, neg(10), 3);
  _ = mat2x2<f32>(1.0, f32(add(11, 12)), 3.0, 4.0);
}
)";

  auto* expect = R"(
fn neg(a : i32) -> i32 {
  return -(a);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn f() {
  neg(1);
  add(2, 3);
  add(neg(4), neg(5));
  neg(6);
  add(7, 8);
  neg(9);
  neg(10);
  add(11, 12);
}
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, SingleSideEffects_OutOfOrder) {
  auto* src = R"(
fn f() {
  _ = neg(1);
  _ = add(2, 3);
  _ = add(neg(4), neg(5));
  _ = u32(neg(6));
  _ = f32(add(7, 8));
  _ = vec2<f32>(f32(neg(9)));
  _ = vec3<i32>(1, neg(10), 3);
  _ = mat2x2<f32>(1.0, f32(add(11, 12)), 3.0, 4.0);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn neg(a : i32) -> i32 {
  return -(a);
}
)";

  auto* expect = R"(
fn f() {
  neg(1);
  add(2, 3);
  add(neg(4), neg(5));
  neg(6);
  add(7, 8);
  neg(9);
  neg(10);
  add(11, 12);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn neg(a : i32) -> i32 {
  return -(a);
}
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, MultipleSideEffects) {
  auto* src = R"(
fn neg(a : i32) -> i32 {
  return -(a);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn xor(a : u32, b : u32) -> u32 {
  return (a ^ b);
}

fn f() {
  _ = (1 + add(2 + add(3, 4), 5)) * add(6, 7) * neg(8);
  _ = add(9, neg(10)) + neg(11);
  _ = xor(12u, 13u) + xor(14u, 15u);
  _ = neg(16) / neg(17) + add(18, 19);
}
)";

  auto* expect = R"(
fn neg(a : i32) -> i32 {
  return -(a);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn xor(a : u32, b : u32) -> u32 {
  return (a ^ b);
}

fn phony_sink(p0 : i32, p1 : i32, p2 : i32) {
}

fn phony_sink_1(p0 : i32, p1 : i32) {
}

fn phony_sink_2(p0 : u32, p1 : u32) {
}

fn f() {
  phony_sink(add((2 + add(3, 4)), 5), add(6, 7), neg(8));
  phony_sink_1(add(9, neg(10)), neg(11));
  phony_sink_2(xor(12u, 13u), xor(14u, 15u));
  phony_sink(neg(16), neg(17), add(18, 19));
}
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, MultipleSideEffects_OutOfOrder) {
  auto* src = R"(
fn f() {
  _ = (1 + add(2 + add(3, 4), 5)) * add(6, 7) * neg(8);
  _ = add(9, neg(10)) + neg(11);
  _ = xor(12u, 13u) + xor(14u, 15u);
  _ = neg(16) / neg(17) + add(18, 19);
}

fn neg(a : i32) -> i32 {
  return -(a);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn xor(a : u32, b : u32) -> u32 {
  return (a ^ b);
}
)";

  auto* expect = R"(
fn phony_sink(p0 : i32, p1 : i32, p2 : i32) {
}

fn phony_sink_1(p0 : i32, p1 : i32) {
}

fn phony_sink_2(p0 : u32, p1 : u32) {
}

fn f() {
  phony_sink(add((2 + add(3, 4)), 5), add(6, 7), neg(8));
  phony_sink_1(add(9, neg(10)), neg(11));
  phony_sink_2(xor(12u, 13u), xor(14u, 15u));
  phony_sink(neg(16), neg(17), add(18, 19));
}

fn neg(a : i32) -> i32 {
  return -(a);
}

fn add(a : i32, b : i32) -> i32 {
  return (a + b);
}

fn xor(a : u32, b : u32) -> u32 {
  return (a ^ b);
}
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, ForLoop) {
  auto* src = R"(
struct S {
  arr : array<i32>,
};

@group(0) @binding(0) var<storage, read_write> s : S;

fn x() -> i32 {
  return 0;
}

fn y() -> i32 {
  return 0;
}

fn z() -> i32 {
  return 0;
}

fn f() {
  for (_ = &s.arr; ;_ = &s.arr) {
    break;
  }
  for (_ = x(); ;_ = y() + z()) {
    break;
  }
}
)";

  auto* expect = R"(
struct S {
  arr : array<i32>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

fn x() -> i32 {
  return 0;
}

fn y() -> i32 {
  return 0;
}

fn z() -> i32 {
  return 0;
}

fn phony_sink(p0 : i32, p1 : i32) {
}

fn f() {
  for(; ; ) {
    break;
  }
  for(x(); ; phony_sink(y(), z())) {
    break;
  }
}
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

TEST_F(RemovePhoniesTest, ForLoop_OutOfOrder) {
  auto* src = R"(
fn f() {
  for (_ = &s.arr; ;_ = &s.arr) {
    break;
  }
  for (_ = x(); ;_ = y() + z()) {
    break;
  }
}

fn x() -> i32 {
  return 0;
}

fn y() -> i32 {
  return 0;
}

fn z() -> i32 {
  return 0;
}

struct S {
  arr : array<i32>,
};

@group(0) @binding(0) var<storage, read_write> s : S;
)";

  auto* expect = R"(
fn phony_sink(p0 : i32, p1 : i32) {
}

fn f() {
  for(; ; ) {
    break;
  }
  for(x(); ; phony_sink(y(), z())) {
    break;
  }
}

fn x() -> i32 {
  return 0;
}

fn y() -> i32 {
  return 0;
}

fn z() -> i32 {
  return 0;
}

struct S {
  arr : array<i32>,
}

@group(0) @binding(0) var<storage, read_write> s : S;
)";

  auto got = Run<RemovePhonies>(src);

  EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
