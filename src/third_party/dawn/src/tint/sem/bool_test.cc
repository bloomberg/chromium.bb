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

#include "src/tint/sem/test_helper.h"
#include "src/tint/sem/texture.h"

namespace tint::sem {
namespace {

using BoolTest = TestHelper;

TEST_F(BoolTest, Creation) {
    auto* a = create<Bool>();
    auto* b = create<Bool>();
    EXPECT_EQ(a, b);
}

TEST_F(BoolTest, Hash) {
    auto* a = create<Bool>();
    auto* b = create<Bool>();
    EXPECT_EQ(a->Hash(), b->Hash());
}

TEST_F(BoolTest, Equals) {
    auto* a = create<Bool>();
    auto* b = create<Bool>();
    EXPECT_TRUE(a->Equals(*b));
    EXPECT_FALSE(a->Equals(Void{}));
}

TEST_F(BoolTest, FriendlyName) {
    Bool b;
    EXPECT_EQ(b.FriendlyName(Symbols()), "bool");
}

}  // namespace
}  // namespace tint::sem
