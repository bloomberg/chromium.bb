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

#include "src/ast/sampler.h"

#include "src/ast/test_helper.h"

namespace tint {
namespace ast {
namespace {

using AstSamplerTest = TestHelper;

TEST_F(AstSamplerTest, Creation) {
  auto* s = create<Sampler>(SamplerKind::kSampler);
  EXPECT_EQ(s->kind(), SamplerKind::kSampler);
}

TEST_F(AstSamplerTest, Creation_ComparisonSampler) {
  auto* s = create<Sampler>(SamplerKind::kComparisonSampler);
  EXPECT_EQ(s->kind(), SamplerKind::kComparisonSampler);
  EXPECT_TRUE(s->IsComparison());
}

TEST_F(AstSamplerTest, TypeName_Sampler) {
  auto* s = create<Sampler>(SamplerKind::kSampler);
  EXPECT_EQ(s->type_name(), "__sampler_sampler");
}

TEST_F(AstSamplerTest, TypeName_Comparison) {
  auto* s = create<Sampler>(SamplerKind::kComparisonSampler);
  EXPECT_EQ(s->type_name(), "__sampler_comparison");
}

TEST_F(AstSamplerTest, FriendlyNameSampler) {
  auto* s = create<Sampler>(SamplerKind::kSampler);
  EXPECT_EQ(s->FriendlyName(Symbols()), "sampler");
}

TEST_F(AstSamplerTest, FriendlyNameComparisonSampler) {
  auto* s = create<Sampler>(SamplerKind::kComparisonSampler);
  EXPECT_EQ(s->FriendlyName(Symbols()), "sampler_comparison");
}

}  // namespace
}  // namespace ast
}  // namespace tint
