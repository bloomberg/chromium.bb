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

#include "src/tint/sem/abstract_float.h"
#include "src/tint/sem/abstract_int.h"
#include "src/tint/sem/reference.h"
#include "src/tint/sem/test_helper.h"

namespace tint::sem {
namespace {

using TypeTest = TestHelper;

TEST_F(TypeTest, ConversionRank) {
    auto* af = create<AbstractFloat>();
    auto* ai = create<AbstractInt>();
    auto* f32 = create<F32>();
    auto* f16 = create<F16>();
    auto* i32 = create<I32>();
    auto* u32 = create<U32>();
    auto* vec3_f32 = create<Vector>(f32, 3u);
    auto* vec3_f16 = create<Vector>(f16, 3u);
    auto* vec4_f32 = create<Vector>(f32, 4u);
    auto* vec3_u32 = create<Vector>(u32, 3u);
    auto* vec3_i32 = create<Vector>(i32, 3u);
    auto* vec3_af = create<Vector>(af, 3u);
    auto* vec3_ai = create<Vector>(ai, 3u);
    auto* mat3x4_f32 = create<Matrix>(vec4_f32, 3u);
    auto* mat4x3_f32 = create<Matrix>(vec3_f32, 4u);
    auto* mat4x3_f16 = create<Matrix>(vec3_f16, 4u);
    auto* mat4x3_af = create<Matrix>(vec3_af, 4u);
    auto* ref_u32 = create<Reference>(u32, ast::StorageClass::kPrivate, ast::Access::kReadWrite);

    EXPECT_EQ(Type::ConversionRank(i32, i32), 0u);
    EXPECT_EQ(Type::ConversionRank(f32, f32), 0u);
    EXPECT_EQ(Type::ConversionRank(u32, u32), 0u);
    EXPECT_EQ(Type::ConversionRank(vec3_f32, vec3_f32), 0u);
    EXPECT_EQ(Type::ConversionRank(vec3_f16, vec3_f16), 0u);
    EXPECT_EQ(Type::ConversionRank(vec4_f32, vec4_f32), 0u);
    EXPECT_EQ(Type::ConversionRank(vec3_u32, vec3_u32), 0u);
    EXPECT_EQ(Type::ConversionRank(vec3_i32, vec3_i32), 0u);
    EXPECT_EQ(Type::ConversionRank(vec3_af, vec3_af), 0u);
    EXPECT_EQ(Type::ConversionRank(vec3_ai, vec3_ai), 0u);
    EXPECT_EQ(Type::ConversionRank(mat3x4_f32, mat3x4_f32), 0u);
    EXPECT_EQ(Type::ConversionRank(mat4x3_f32, mat4x3_f32), 0u);
    EXPECT_EQ(Type::ConversionRank(mat4x3_f16, mat4x3_f16), 0u);
    EXPECT_EQ(Type::ConversionRank(mat4x3_af, mat4x3_af), 0u);
    EXPECT_EQ(Type::ConversionRank(ref_u32, u32), 0u);

    EXPECT_EQ(Type::ConversionRank(af, f32), 1u);
    EXPECT_EQ(Type::ConversionRank(vec3_af, vec3_f32), 1u);
    EXPECT_EQ(Type::ConversionRank(mat4x3_af, mat4x3_f32), 1u);
    EXPECT_EQ(Type::ConversionRank(af, f16), 2u);
    EXPECT_EQ(Type::ConversionRank(vec3_af, vec3_f16), 2u);
    EXPECT_EQ(Type::ConversionRank(mat4x3_af, mat4x3_f16), 2u);
    EXPECT_EQ(Type::ConversionRank(ai, i32), 3u);
    EXPECT_EQ(Type::ConversionRank(vec3_ai, vec3_i32), 3u);
    EXPECT_EQ(Type::ConversionRank(ai, u32), 4u);
    EXPECT_EQ(Type::ConversionRank(vec3_ai, vec3_u32), 4u);
    EXPECT_EQ(Type::ConversionRank(ai, af), 5u);
    EXPECT_EQ(Type::ConversionRank(ai, f32), 6u);
    EXPECT_EQ(Type::ConversionRank(ai, f16), 7u);

    EXPECT_EQ(Type::ConversionRank(i32, f32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(f32, u32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(u32, i32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(vec3_u32, vec3_f32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(vec3_f32, vec4_f32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(mat3x4_f32, mat4x3_f32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(mat4x3_f32, mat3x4_f32), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(mat4x3_f32, mat4x3_af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(f32, af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(f16, af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(vec3_f16, vec3_af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(mat4x3_f16, mat4x3_af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(i32, af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(u32, af), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(af, ai), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(f32, ai), Type::kNoConversion);
    EXPECT_EQ(Type::ConversionRank(f16, ai), Type::kNoConversion);
}

TEST_F(TypeTest, ElementOf) {
    auto* f32 = create<F32>();
    auto* f16 = create<F16>();
    auto* i32 = create<I32>();
    auto* u32 = create<U32>();
    auto* vec2_f32 = create<Vector>(f32, 2u);
    auto* vec3_f16 = create<Vector>(f16, 3u);
    auto* vec4_f32 = create<Vector>(f32, 4u);
    auto* vec3_u32 = create<Vector>(u32, 3u);
    auto* vec3_i32 = create<Vector>(i32, 3u);
    auto* mat2x4_f32 = create<Matrix>(vec4_f32, 2u);
    auto* mat4x2_f32 = create<Matrix>(vec2_f32, 4u);
    auto* mat4x3_f16 = create<Matrix>(vec3_f16, 4u);
    auto* arr_i32 = create<Array>(
        /* element */ i32,
        /* count */ 5u,
        /* align */ 4u,
        /* size */ 5u * 4u,
        /* stride */ 5u * 4u,
        /* implicit_stride */ 5u * 4u);

    // No count
    EXPECT_TYPE(Type::ElementOf(f32), f32);
    EXPECT_TYPE(Type::ElementOf(f16), f16);
    EXPECT_TYPE(Type::ElementOf(i32), i32);
    EXPECT_TYPE(Type::ElementOf(u32), u32);
    EXPECT_TYPE(Type::ElementOf(vec2_f32), f32);
    EXPECT_TYPE(Type::ElementOf(vec3_f16), f16);
    EXPECT_TYPE(Type::ElementOf(vec4_f32), f32);
    EXPECT_TYPE(Type::ElementOf(vec3_u32), u32);
    EXPECT_TYPE(Type::ElementOf(vec3_i32), i32);
    EXPECT_TYPE(Type::ElementOf(mat2x4_f32), f32);
    EXPECT_TYPE(Type::ElementOf(mat4x2_f32), f32);
    EXPECT_TYPE(Type::ElementOf(mat4x3_f16), f16);
    EXPECT_TYPE(Type::ElementOf(arr_i32), i32);

    // With count
    uint32_t count = 0;
    EXPECT_TYPE(Type::ElementOf(f32, &count), f32);
    EXPECT_EQ(count, 1u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(f16, &count), f16);
    EXPECT_EQ(count, 1u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(i32, &count), i32);
    EXPECT_EQ(count, 1u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(u32, &count), u32);
    EXPECT_EQ(count, 1u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(vec2_f32, &count), f32);
    EXPECT_EQ(count, 2u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(vec3_f16, &count), f16);
    EXPECT_EQ(count, 3u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(vec4_f32, &count), f32);
    EXPECT_EQ(count, 4u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(vec3_u32, &count), u32);
    EXPECT_EQ(count, 3u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(vec3_i32, &count), i32);
    EXPECT_EQ(count, 3u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(mat2x4_f32, &count), f32);
    EXPECT_EQ(count, 8u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(mat4x2_f32, &count), f32);
    EXPECT_EQ(count, 8u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(mat4x3_f16, &count), f16);
    EXPECT_EQ(count, 12u);
    count = 0;
    EXPECT_TYPE(Type::ElementOf(arr_i32, &count), i32);
    EXPECT_EQ(count, 5u);
}

TEST_F(TypeTest, Common2) {
    auto* ai = create<AbstractInt>();
    auto* af = create<AbstractFloat>();
    auto* f32 = create<F32>();
    auto* f16 = create<F16>();
    auto* i32 = create<I32>();
    auto* u32 = create<U32>();

    EXPECT_TYPE(Type::Common({ai, ai}), ai);
    EXPECT_TYPE(Type::Common({af, af}), af);
    EXPECT_TYPE(Type::Common({f32, f32}), f32);
    EXPECT_TYPE(Type::Common({f16, f16}), f16);
    EXPECT_TYPE(Type::Common({i32, i32}), i32);
    EXPECT_TYPE(Type::Common({u32, u32}), u32);

    EXPECT_TYPE(Type::Common({i32, u32}), nullptr);
    EXPECT_TYPE(Type::Common({u32, f32}), nullptr);
    EXPECT_TYPE(Type::Common({f32, f16}), nullptr);
    EXPECT_TYPE(Type::Common({f16, i32}), nullptr);

    EXPECT_TYPE(Type::Common({ai, af}), af);
    EXPECT_TYPE(Type::Common({ai, f32}), f32);
    EXPECT_TYPE(Type::Common({ai, f16}), f16);
    EXPECT_TYPE(Type::Common({ai, i32}), i32);
    EXPECT_TYPE(Type::Common({ai, u32}), u32);

    EXPECT_TYPE(Type::Common({af, ai}), af);
    EXPECT_TYPE(Type::Common({f32, ai}), f32);
    EXPECT_TYPE(Type::Common({f16, ai}), f16);
    EXPECT_TYPE(Type::Common({i32, ai}), i32);
    EXPECT_TYPE(Type::Common({u32, ai}), u32);

    EXPECT_TYPE(Type::Common({ai, af}), af);
    EXPECT_TYPE(Type::Common({f32, af}), f32);
    EXPECT_TYPE(Type::Common({f16, af}), f16);
    EXPECT_TYPE(Type::Common({i32, af}), nullptr);
    EXPECT_TYPE(Type::Common({u32, af}), nullptr);

    EXPECT_TYPE(Type::Common({af, ai}), af);
    EXPECT_TYPE(Type::Common({af, f32}), f32);
    EXPECT_TYPE(Type::Common({af, f16}), f16);
    EXPECT_TYPE(Type::Common({af, i32}), nullptr);
    EXPECT_TYPE(Type::Common({af, u32}), nullptr);

    auto* vec3_ai = create<Vector>(ai, 3u);
    auto* vec3_af = create<Vector>(af, 3u);
    auto* vec3_f32 = create<Vector>(f32, 3u);
    auto* vec3_f16 = create<Vector>(f16, 3u);
    auto* vec4_f32 = create<Vector>(f32, 4u);
    auto* vec3_u32 = create<Vector>(u32, 3u);
    auto* vec3_i32 = create<Vector>(i32, 3u);

    EXPECT_TYPE(Type::Common({vec3_ai, vec3_ai}), vec3_ai);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_af}), vec3_af);
    EXPECT_TYPE(Type::Common({vec3_f32, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_f16, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec4_f32, vec4_f32}), vec4_f32);
    EXPECT_TYPE(Type::Common({vec3_u32, vec3_u32}), vec3_u32);
    EXPECT_TYPE(Type::Common({vec3_i32, vec3_i32}), vec3_i32);

    EXPECT_TYPE(Type::Common({vec3_ai, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec3_ai, vec4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_u32}), vec3_u32);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_i32}), vec3_i32);

    EXPECT_TYPE(Type::Common({vec3_f32, vec3_ai}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_f16, vec3_ai}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec4_f32, vec3_ai}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_u32, vec3_ai}), vec3_u32);
    EXPECT_TYPE(Type::Common({vec3_i32, vec3_ai}), vec3_i32);

    EXPECT_TYPE(Type::Common({vec3_af, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec3_af, vec4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_u32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_i32}), nullptr);

    EXPECT_TYPE(Type::Common({vec3_f32, vec3_af}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_f16, vec3_af}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec4_f32, vec3_af}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_u32, vec3_af}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_i32, vec3_af}), nullptr);

    auto* mat4x3_af = create<Matrix>(vec3_af, 4u);
    auto* mat3x4_f32 = create<Matrix>(vec4_f32, 3u);
    auto* mat4x3_f32 = create<Matrix>(vec3_f32, 4u);
    auto* mat4x3_f16 = create<Matrix>(vec3_f16, 4u);

    EXPECT_TYPE(Type::Common({mat4x3_af, mat4x3_af}), mat4x3_af);
    EXPECT_TYPE(Type::Common({mat3x4_f32, mat3x4_f32}), mat3x4_f32);
    EXPECT_TYPE(Type::Common({mat4x3_f32, mat4x3_f32}), mat4x3_f32);
    EXPECT_TYPE(Type::Common({mat4x3_f16, mat4x3_f16}), mat4x3_f16);

    EXPECT_TYPE(Type::Common({mat4x3_af, mat3x4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({mat4x3_af, mat4x3_f32}), mat4x3_f32);
    EXPECT_TYPE(Type::Common({mat4x3_af, mat4x3_f16}), mat4x3_f16);

    EXPECT_TYPE(Type::Common({mat3x4_f32, mat4x3_af}), nullptr);
    EXPECT_TYPE(Type::Common({mat4x3_f32, mat4x3_af}), mat4x3_f32);
    EXPECT_TYPE(Type::Common({mat4x3_f16, mat4x3_af}), mat4x3_f16);
}

TEST_F(TypeTest, Common3) {
    auto* ai = create<AbstractInt>();
    auto* af = create<AbstractFloat>();
    auto* f32 = create<F32>();
    auto* f16 = create<F16>();
    auto* i32 = create<I32>();
    auto* u32 = create<U32>();

    EXPECT_TYPE(Type::Common({ai, ai, ai}), ai);
    EXPECT_TYPE(Type::Common({af, af, af}), af);
    EXPECT_TYPE(Type::Common({f32, f32, f32}), f32);
    EXPECT_TYPE(Type::Common({f16, f16, f16}), f16);
    EXPECT_TYPE(Type::Common({i32, i32, i32}), i32);
    EXPECT_TYPE(Type::Common({u32, u32, u32}), u32);

    EXPECT_TYPE(Type::Common({ai, af, ai}), af);
    EXPECT_TYPE(Type::Common({ai, f32, ai}), f32);
    EXPECT_TYPE(Type::Common({ai, f16, ai}), f16);
    EXPECT_TYPE(Type::Common({ai, i32, ai}), i32);
    EXPECT_TYPE(Type::Common({ai, u32, ai}), u32);

    EXPECT_TYPE(Type::Common({af, ai, af}), af);
    EXPECT_TYPE(Type::Common({f32, ai, f32}), f32);
    EXPECT_TYPE(Type::Common({f16, ai, f16}), f16);
    EXPECT_TYPE(Type::Common({i32, ai, i32}), i32);
    EXPECT_TYPE(Type::Common({u32, ai, u32}), u32);

    EXPECT_TYPE(Type::Common({ai, f32, ai}), f32);
    EXPECT_TYPE(Type::Common({ai, f16, ai}), f16);
    EXPECT_TYPE(Type::Common({ai, i32, ai}), i32);
    EXPECT_TYPE(Type::Common({ai, u32, ai}), u32);

    EXPECT_TYPE(Type::Common({f32, ai, f32}), f32);
    EXPECT_TYPE(Type::Common({f16, ai, f16}), f16);
    EXPECT_TYPE(Type::Common({i32, ai, i32}), i32);
    EXPECT_TYPE(Type::Common({u32, ai, u32}), u32);

    EXPECT_TYPE(Type::Common({af, f32, af}), f32);
    EXPECT_TYPE(Type::Common({af, f16, af}), f16);
    EXPECT_TYPE(Type::Common({af, i32, af}), nullptr);
    EXPECT_TYPE(Type::Common({af, u32, af}), nullptr);

    EXPECT_TYPE(Type::Common({f32, af, f32}), f32);
    EXPECT_TYPE(Type::Common({f16, af, f16}), f16);
    EXPECT_TYPE(Type::Common({i32, af, i32}), nullptr);
    EXPECT_TYPE(Type::Common({u32, af, u32}), nullptr);

    EXPECT_TYPE(Type::Common({ai, af, f32}), f32);
    EXPECT_TYPE(Type::Common({ai, af, f16}), f16);
    EXPECT_TYPE(Type::Common({ai, af, i32}), nullptr);
    EXPECT_TYPE(Type::Common({ai, af, u32}), nullptr);

    auto* vec3_ai = create<Vector>(ai, 3u);
    auto* vec3_af = create<Vector>(af, 3u);
    auto* vec3_f32 = create<Vector>(f32, 3u);
    auto* vec3_f16 = create<Vector>(f16, 3u);
    auto* vec4_f32 = create<Vector>(f32, 4u);
    auto* vec3_u32 = create<Vector>(u32, 3u);
    auto* vec3_i32 = create<Vector>(i32, 3u);

    EXPECT_TYPE(Type::Common({vec3_ai, vec3_ai, vec3_ai}), vec3_ai);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_af, vec3_af}), vec3_af);
    EXPECT_TYPE(Type::Common({vec3_f32, vec3_f32, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_f16, vec3_f16, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec4_f32, vec4_f32, vec4_f32}), vec4_f32);
    EXPECT_TYPE(Type::Common({vec3_u32, vec3_u32, vec3_u32}), vec3_u32);
    EXPECT_TYPE(Type::Common({vec3_i32, vec3_i32, vec3_i32}), vec3_i32);

    EXPECT_TYPE(Type::Common({vec3_f32, vec3_ai, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_f16, vec3_ai, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec4_f32, vec3_ai, vec4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_u32, vec3_ai, vec3_u32}), vec3_u32);
    EXPECT_TYPE(Type::Common({vec3_i32, vec3_ai, vec3_i32}), vec3_i32);

    EXPECT_TYPE(Type::Common({vec3_ai, vec3_f32, vec3_ai}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_f16, vec3_ai}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec3_ai, vec4_f32, vec3_ai}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_u32, vec3_ai}), vec3_u32);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_i32, vec3_ai}), vec3_i32);

    EXPECT_TYPE(Type::Common({vec3_f32, vec3_af, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_f16, vec3_af, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec4_f32, vec3_af, vec4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_u32, vec3_af, vec3_u32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_i32, vec3_af, vec3_i32}), nullptr);

    EXPECT_TYPE(Type::Common({vec3_af, vec3_f32, vec3_af}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_f16, vec3_af}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec3_af, vec4_f32, vec3_af}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_u32, vec3_af}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_af, vec3_i32, vec3_af}), nullptr);

    EXPECT_TYPE(Type::Common({vec3_ai, vec3_af, vec3_f32}), vec3_f32);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_af, vec3_f16}), vec3_f16);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_af, vec4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_af, vec3_u32}), nullptr);
    EXPECT_TYPE(Type::Common({vec3_ai, vec3_af, vec3_i32}), nullptr);

    auto* mat4x3_af = create<Matrix>(vec3_af, 4u);
    auto* mat3x4_f32 = create<Matrix>(vec4_f32, 3u);
    auto* mat4x3_f32 = create<Matrix>(vec3_f32, 4u);
    auto* mat4x3_f16 = create<Matrix>(vec3_f16, 4u);

    EXPECT_TYPE(Type::Common({mat4x3_af, mat4x3_af, mat4x3_af}), mat4x3_af);
    EXPECT_TYPE(Type::Common({mat3x4_f32, mat3x4_f32, mat3x4_f32}), mat3x4_f32);
    EXPECT_TYPE(Type::Common({mat4x3_f32, mat4x3_f32, mat4x3_f32}), mat4x3_f32);
    EXPECT_TYPE(Type::Common({mat4x3_f16, mat4x3_f16, mat4x3_f16}), mat4x3_f16);

    EXPECT_TYPE(Type::Common({mat3x4_f32, mat4x3_af, mat3x4_f32}), nullptr);
    EXPECT_TYPE(Type::Common({mat4x3_f32, mat4x3_af, mat4x3_f32}), mat4x3_f32);
    EXPECT_TYPE(Type::Common({mat4x3_f16, mat4x3_af, mat4x3_f16}), mat4x3_f16);

    EXPECT_TYPE(Type::Common({mat4x3_af, mat3x4_f32, mat4x3_af}), nullptr);
    EXPECT_TYPE(Type::Common({mat4x3_af, mat4x3_f32, mat4x3_af}), mat4x3_f32);
    EXPECT_TYPE(Type::Common({mat4x3_af, mat4x3_f16, mat4x3_af}), mat4x3_f16);
}

}  // namespace
}  // namespace tint::sem
