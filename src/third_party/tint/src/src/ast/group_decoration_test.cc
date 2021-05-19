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

#include "src/ast/constant_id_decoration.h"
#include "src/ast/test_helper.h"

namespace tint {
namespace ast {
namespace {

using GroupDecorationTest = TestHelper;

TEST_F(GroupDecorationTest, Creation) {
  auto* d = create<GroupDecoration>(2);
  EXPECT_EQ(2u, d->value());
}

TEST_F(GroupDecorationTest, Is) {
  Decoration* d = create<GroupDecoration>(2);
  EXPECT_FALSE(d->Is<BindingDecoration>());
  EXPECT_FALSE(d->Is<BuiltinDecoration>());
  EXPECT_FALSE(d->Is<ConstantIdDecoration>());
  EXPECT_FALSE(d->Is<LocationDecoration>());
  EXPECT_TRUE(d->Is<GroupDecoration>());
}

TEST_F(GroupDecorationTest, ToStr) {
  auto* d = create<GroupDecoration>(2);
  EXPECT_EQ(str(d), R"(GroupDecoration{2}
)");
}

}  // namespace
}  // namespace ast
}  // namespace tint
