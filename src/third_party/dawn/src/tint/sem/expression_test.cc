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

#include "src/tint/sem/expression.h"

#include "src/tint/sem/test_helper.h"

#include "src/tint/sem/materialize.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::sem {
namespace {

using ExpressionTest = TestHelper;

TEST_F(ExpressionTest, UnwrapMaterialize) {
    auto* a = create<Expression>(/* declaration */ nullptr, create<I32>(), /* statement */ nullptr,
                                 Constant{},
                                 /* has_side_effects */ false, /* source_var */ nullptr);
    auto* b = create<Materialize>(a, /* statement */ nullptr, Constant{create<I32>(), {1_a}});

    EXPECT_EQ(a, a->UnwrapMaterialize());
    EXPECT_EQ(a, b->UnwrapMaterialize());
}

}  // namespace
}  // namespace tint::sem
