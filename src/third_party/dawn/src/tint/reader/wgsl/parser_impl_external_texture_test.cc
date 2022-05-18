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

#include "src/tint/reader/wgsl/parser_impl_test_helper.h"

namespace tint::reader::wgsl {
namespace {

TEST_F(ParserImplTest, ExternalTextureType_Invalid) {
    auto p = parser("1234");
    auto t = p->external_texture();
    EXPECT_FALSE(t.matched);
    EXPECT_FALSE(t.errored);
    EXPECT_FALSE(p->has_error());
}

TEST_F(ParserImplTest, ExternalTextureType) {
    auto p = parser("texture_external");
    auto t = p->external_texture();
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    EXPECT_EQ(t.value->source.range, (Source::Range{{1u, 1u}, {1u, 17u}}));
}

}  // namespace
}  // namespace tint::reader::wgsl
