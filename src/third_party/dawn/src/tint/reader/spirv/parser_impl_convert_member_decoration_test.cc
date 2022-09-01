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

#include "gmock/gmock.h"
#include "src/tint/reader/spirv/parser_impl_test_helper.h"

namespace tint::reader::spirv {
namespace {

using ::testing::Eq;

TEST_F(SpvParserTest, ConvertMemberDecoration_Empty) {
    auto p = parser(std::vector<uint32_t>{});

    auto result = p->ConvertMemberDecoration(1, 1, nullptr, {});
    EXPECT_TRUE(result.empty());
    EXPECT_THAT(p->error(), Eq("malformed SPIR-V decoration: it's empty"));
}

TEST_F(SpvParserTest, ConvertMemberDecoration_OffsetWithoutOperand) {
    auto p = parser(std::vector<uint32_t>{});

    auto result = p->ConvertMemberDecoration(12, 13, nullptr, {SpvDecorationOffset});
    EXPECT_TRUE(result.empty());
    EXPECT_THAT(p->error(), Eq("malformed Offset decoration: expected 1 literal "
                               "operand, has 0: member 13 of SPIR-V type 12"));
}

TEST_F(SpvParserTest, ConvertMemberDecoration_OffsetWithTooManyOperands) {
    auto p = parser(std::vector<uint32_t>{});

    auto result = p->ConvertMemberDecoration(12, 13, nullptr, {SpvDecorationOffset, 3, 4});
    EXPECT_TRUE(result.empty());
    EXPECT_THAT(p->error(), Eq("malformed Offset decoration: expected 1 literal "
                               "operand, has 2: member 13 of SPIR-V type 12"));
}

TEST_F(SpvParserTest, ConvertMemberDecoration_Offset) {
    auto p = parser(std::vector<uint32_t>{});

    auto result = p->ConvertMemberDecoration(1, 1, nullptr, {SpvDecorationOffset, 8});
    ASSERT_FALSE(result.empty());
    EXPECT_TRUE(result[0]->Is<ast::StructMemberOffsetAttribute>());
    auto* offset_deco = result[0]->As<ast::StructMemberOffsetAttribute>();
    ASSERT_NE(offset_deco, nullptr);
    EXPECT_EQ(offset_deco->offset, 8u);
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_Matrix2x2_Stride_Natural) {
    auto p = parser(std::vector<uint32_t>{});

    spirv::F32 f32;
    spirv::Matrix matrix(&f32, 2, 2);
    auto result = p->ConvertMemberDecoration(1, 1, &matrix, {SpvDecorationMatrixStride, 8});
    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_Matrix2x2_Stride_Custom) {
    auto p = parser(std::vector<uint32_t>{});

    spirv::F32 f32;
    spirv::Matrix matrix(&f32, 2, 2);
    auto result = p->ConvertMemberDecoration(1, 1, &matrix, {SpvDecorationMatrixStride, 16});
    ASSERT_FALSE(result.empty());
    EXPECT_TRUE(result[0]->Is<ast::StrideAttribute>());
    auto* stride_deco = result[0]->As<ast::StrideAttribute>();
    ASSERT_NE(stride_deco, nullptr);
    EXPECT_EQ(stride_deco->stride, 16u);
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_Matrix2x4_Stride_Natural) {
    auto p = parser(std::vector<uint32_t>{});

    spirv::F32 f32;
    spirv::Matrix matrix(&f32, 2, 4);
    auto result = p->ConvertMemberDecoration(1, 1, &matrix, {SpvDecorationMatrixStride, 16});
    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_Matrix2x4_Stride_Custom) {
    auto p = parser(std::vector<uint32_t>{});

    spirv::F32 f32;
    spirv::Matrix matrix(&f32, 2, 4);
    auto result = p->ConvertMemberDecoration(1, 1, &matrix, {SpvDecorationMatrixStride, 64});
    ASSERT_FALSE(result.empty());
    EXPECT_TRUE(result[0]->Is<ast::StrideAttribute>());
    auto* stride_deco = result[0]->As<ast::StrideAttribute>();
    ASSERT_NE(stride_deco, nullptr);
    EXPECT_EQ(stride_deco->stride, 64u);
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_Matrix2x3_Stride_Custom) {
    auto p = parser(std::vector<uint32_t>{});

    spirv::F32 f32;
    spirv::Matrix matrix(&f32, 2, 3);
    auto result = p->ConvertMemberDecoration(1, 1, &matrix, {SpvDecorationMatrixStride, 32});
    ASSERT_FALSE(result.empty());
    EXPECT_TRUE(result[0]->Is<ast::StrideAttribute>());
    auto* stride_deco = result[0]->As<ast::StrideAttribute>();
    ASSERT_NE(stride_deco, nullptr);
    EXPECT_EQ(stride_deco->stride, 32u);
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_RelaxedPrecision) {
    // WGSL does not support relaxed precision. Drop it.
    // It's functionally correct to use full precision f32 instead of
    // relaxed precision f32.
    auto p = parser(std::vector<uint32_t>{});

    auto result = p->ConvertMemberDecoration(1, 1, nullptr, {SpvDecorationRelaxedPrecision});
    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(p->error().empty());
}

TEST_F(SpvParserTest, ConvertMemberDecoration_UnhandledDecoration) {
    auto p = parser(std::vector<uint32_t>{});

    auto result = p->ConvertMemberDecoration(12, 13, nullptr, {12345678});
    EXPECT_TRUE(result.empty());
    EXPECT_THAT(p->error(), Eq("unhandled member decoration: 12345678 on member "
                               "13 of SPIR-V type 12"));
}

}  // namespace
}  // namespace tint::reader::spirv
