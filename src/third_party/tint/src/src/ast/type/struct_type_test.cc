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

#include "src/ast/type/struct_type.h"

#include <utility>

#include "gtest/gtest.h"
#include "src/ast/stride_decoration.h"
#include "src/ast/struct_member.h"
#include "src/ast/struct_member_decoration.h"
#include "src/ast/struct_member_offset_decoration.h"
#include "src/ast/type/array_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/u32_type.h"
#include "src/ast/type/vector_type.h"

namespace tint {
namespace ast {
namespace type {
namespace {

using StructTypeTest = testing::Test;

TEST_F(StructTypeTest, Creation) {
  auto impl = std::make_unique<Struct>();
  auto* ptr = impl.get();
  StructType s{"S", std::move(impl)};
  EXPECT_EQ(s.impl(), ptr);
}

TEST_F(StructTypeTest, Is) {
  auto impl = std::make_unique<Struct>();
  StructType s{"S", std::move(impl)};
  EXPECT_FALSE(s.IsAccessControl());
  EXPECT_FALSE(s.IsAlias());
  EXPECT_FALSE(s.IsArray());
  EXPECT_FALSE(s.IsBool());
  EXPECT_FALSE(s.IsF32());
  EXPECT_FALSE(s.IsI32());
  EXPECT_FALSE(s.IsMatrix());
  EXPECT_FALSE(s.IsPointer());
  EXPECT_FALSE(s.IsSampler());
  EXPECT_TRUE(s.IsStruct());
  EXPECT_FALSE(s.IsTexture());
  EXPECT_FALSE(s.IsU32());
  EXPECT_FALSE(s.IsVector());
}

TEST_F(StructTypeTest, TypeName) {
  auto impl = std::make_unique<Struct>();
  StructType s{"my_struct", std::move(impl)};
  EXPECT_EQ(s.type_name(), "__struct_my_struct");
}

TEST_F(StructTypeTest, MinBufferBindingSize) {
  U32Type u32;
  StructMemberList members;

  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(4, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u,
            struct_type.MinBufferBindingSize(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(8u, struct_type.MinBufferBindingSize(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, MinBufferBindingSizeArray) {
  U32Type u32;
  ArrayType arr(&u32, 4);
  {
    ArrayDecorationList decos;
    decos.push_back(std::make_unique<StrideDecoration>(4, Source{}));
    arr.set_decorations(std::move(decos));
  }

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(4, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(8, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &arr, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(32u,
            struct_type.MinBufferBindingSize(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(24u,
            struct_type.MinBufferBindingSize(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, MinBufferBindingSizeRuntimeArray) {
  U32Type u32;
  ArrayType arr(&u32);
  {
    ArrayDecorationList decos;
    decos.push_back(std::make_unique<StrideDecoration>(4, Source{}));
    arr.set_decorations(std::move(decos));
  }

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(4, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(8, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(12u,
            struct_type.MinBufferBindingSize(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, MinBufferBindingSizeVec2) {
  U32Type u32;
  VectorType vec2(&u32, 2);

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &vec2, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u,
            struct_type.MinBufferBindingSize(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(8u, struct_type.MinBufferBindingSize(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, MinBufferBindingSizeVec3) {
  U32Type u32;
  VectorType vec3(&u32, 3);

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &vec3, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u,
            struct_type.MinBufferBindingSize(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(16u,
            struct_type.MinBufferBindingSize(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, MinBufferBindingSizeVec4) {
  U32Type u32;
  VectorType vec4(&u32, 4);

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &vec4, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u,
            struct_type.MinBufferBindingSize(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(16u,
            struct_type.MinBufferBindingSize(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, BaseAlignment) {
  U32Type u32;
  StructMemberList members;

  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(4, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(4u, struct_type.BaseAlignment(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, BaseAlignmentArray) {
  U32Type u32;
  ArrayType arr(&u32, 4);
  {
    ArrayDecorationList decos;
    decos.push_back(std::make_unique<StrideDecoration>(4, Source{}));
    arr.set_decorations(std::move(decos));
  }

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(4, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(8, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &arr, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(4u, struct_type.BaseAlignment(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, BaseAlignmentRuntimeArray) {
  U32Type u32;
  ArrayType arr(&u32);
  {
    ArrayDecorationList decos;
    decos.push_back(std::make_unique<StrideDecoration>(4, Source{}));
    arr.set_decorations(std::move(decos));
  }

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(4, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(8, Source{}));
    members.push_back(
        std::make_unique<StructMember>("bar", &u32, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(4u, struct_type.BaseAlignment(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, BaseAlignmentVec2) {
  U32Type u32;
  VectorType vec2(&u32, 2);

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &vec2, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(8u, struct_type.BaseAlignment(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, BaseAlignmentVec3) {
  U32Type u32;
  VectorType vec3(&u32, 3);

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &vec3, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kStorageBuffer));
}

TEST_F(StructTypeTest, BaseAlignmentVec4) {
  U32Type u32;
  VectorType vec4(&u32, 4);

  StructMemberList members;
  {
    StructMemberDecorationList deco;
    deco.push_back(std::make_unique<StructMemberOffsetDecoration>(0, Source{}));
    members.push_back(
        std::make_unique<StructMember>("foo", &vec4, std::move(deco)));
  }
  ast::StructDecorationList decos;

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));
  StructType struct_type("struct_type", std::move(str));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kUniformBuffer));
  EXPECT_EQ(16u, struct_type.BaseAlignment(MemoryLayout::kStorageBuffer));
}

}  // namespace
}  // namespace type
}  // namespace ast
}  // namespace tint
