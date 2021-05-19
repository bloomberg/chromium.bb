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

#include "src/transform/binding_remapper.h"

#include <utility>

#include "src/transform/test_helper.h"

namespace tint {
namespace transform {
namespace {

using BindingRemapperTest = TransformTest;

TEST_F(BindingRemapperTest, NoRemappings) {
  auto* src = R"(
[[block]]
struct S {
};

[[group(2), binding(1)]] var<storage> a : S;

[[group(3), binding(2)]] var<storage> b : S;

[[stage(compute)]]
fn f() -> void {
}
)";

  auto* expect = src;

  DataMap data;
  data.Add<BindingRemapper::Remappings>(BindingRemapper::BindingPoints{},
                                        BindingRemapper::AccessControls{});
  auto got = Run<BindingRemapper>(src, std::move(data));

  EXPECT_EQ(expect, str(got));
}

TEST_F(BindingRemapperTest, RemapBindingPoints) {
  auto* src = R"(
[[block]]
struct S {
};

[[group(2), binding(1)]] var<storage> a : S;

[[group(3), binding(2)]] var<storage> b : S;

[[stage(compute)]]
fn f() -> void {
}
)";

  auto* expect = R"(
[[block]]
struct S {
};

[[group(1), binding(2)]] var<storage> a : S;

[[group(3), binding(2)]] var<storage> b : S;

[[stage(compute)]]
fn f() -> void {
}
)";

  DataMap data;
  data.Add<BindingRemapper::Remappings>(
      BindingRemapper::BindingPoints{
          {{2, 1}, {1, 2}},  // Remap
          {{4, 5}, {6, 7}},  // Not found
                             // Keep [[group(3), binding(2)]] as is
      },
      BindingRemapper::AccessControls{});
  auto got = Run<BindingRemapper>(src, std::move(data));

  EXPECT_EQ(expect, str(got));
}

TEST_F(BindingRemapperTest, RemapAccessControls) {
  auto* src = R"(
[[block]]
struct S {
};

[[group(2), binding(1)]] var<storage> a : [[access(read)]]
S;

[[group(3), binding(2)]] var<storage> b : [[access(write)]]
S;

[[group(4), binding(3)]] var<storage> c : S;

[[stage(compute)]]
fn f() -> void {
}
)";

  auto* expect = R"(
[[block]]
struct S {
};

[[group(2), binding(1)]] var<storage> a : [[access(write)]]
S;

[[group(3), binding(2)]] var<storage> b : [[access(write)]]
S;

[[group(4), binding(3)]] var<storage> c : [[access(read)]]
S;

[[stage(compute)]]
fn f() -> void {
}
)";

  DataMap data;
  data.Add<BindingRemapper::Remappings>(
      BindingRemapper::BindingPoints{},
      BindingRemapper::AccessControls{
          {{2, 1}, ast::AccessControl::kWriteOnly},  // Modify access control
          // Keep [[group(3), binding(2)]] as is
          {{4, 3}, ast::AccessControl::kReadOnly},  // Add access control
      });
  auto got = Run<BindingRemapper>(src, std::move(data));

  EXPECT_EQ(expect, str(got));
}

// TODO(crbug.com/676): Possibly enable if the spec allows for access
// decorations in type aliases. If not, just remove.
TEST_F(BindingRemapperTest, DISABLED_RemapAccessControlsWithAliases) {
  auto* src = R"(
[[block]]
struct S {
};

type ReadOnlyS = [[access(read)]]
S;

type WriteOnlyS = [[access(write)]]
S;

type A = S;

[[group(2), binding(1)]] var<storage> a : ReadOnlyS;

[[group(3), binding(2)]] var<storage> b : WriteOnlyS;

[[group(4), binding(3)]] var<storage> c : A;

[[stage(compute)]]
fn f() -> void {
}
)";

  auto* expect = R"(
[[block]]
struct S {
};

type ReadOnlyS = [[access(read)]]
S;

type WriteOnlyS = [[access(write)]]
S;

type A = S;

[[group(2), binding(1)]] var<storage> a : [[access(write)]]
S;

[[group(3), binding(2)]] var<storage> b : WriteOnlyS;

[[group(4), binding(3)]] var<storage> c : [[access(write)]] S;

[[stage(compute)]]
fn f() -> void {
}
)";

  DataMap data;
  data.Add<BindingRemapper::Remappings>(
      BindingRemapper::BindingPoints{},
      BindingRemapper::AccessControls{
          {{2, 1}, ast::AccessControl::kWriteOnly},  // Modify access control
          // Keep [[group(3), binding(2)]] as is
          {{4, 3}, ast::AccessControl::kReadOnly},  // Add access control
      });
  auto got = Run<BindingRemapper>(src, std::move(data));

  EXPECT_EQ(expect, str(got));
}

TEST_F(BindingRemapperTest, RemapAll) {
  auto* src = R"(
[[block]]
struct S {
};

[[group(2), binding(1)]] var<storage> a : [[access(read)]]
S;

[[group(3), binding(2)]] var<storage> b : S;

[[stage(compute)]]
fn f() -> void {
}
)";

  auto* expect = R"(
[[block]]
struct S {
};

[[group(4), binding(5)]] var<storage> a : [[access(write)]]
S;

[[group(6), binding(7)]] var<storage> b : [[access(write)]]
S;

[[stage(compute)]]
fn f() -> void {
}
)";

  DataMap data;
  data.Add<BindingRemapper::Remappings>(
      BindingRemapper::BindingPoints{
          {{2, 1}, {4, 5}},
          {{3, 2}, {6, 7}},
      },
      BindingRemapper::AccessControls{
          {{2, 1}, ast::AccessControl::kWriteOnly},
          {{3, 2}, ast::AccessControl::kWriteOnly},
      });
  auto got = Run<BindingRemapper>(src, std::move(data));

  EXPECT_EQ(expect, str(got));
}

TEST_F(BindingRemapperTest, NoData) {
  auto* src = R"(
[[block]]
struct S {
};

[[group(2), binding(1)]] var<storage> a : S;
[[group(3), binding(2)]] var<storage> b : S;

[[stage(compute)]]
fn f() -> void {}
)";

  auto* expect = "error: BindingRemapper did not find the remapping data";

  auto got = Run<BindingRemapper>(src);

  EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace transform
}  // namespace tint
