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

#include "gtest/gtest.h"
#include "src/ast/type/alias_type.h"
#include "src/ast/type/array_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/struct_type.h"
#include "src/reader/wgsl/parser_impl.h"
#include "src/reader/wgsl/parser_impl_test_helper.h"
#include "src/type_manager.h"

namespace tint {
namespace reader {
namespace wgsl {
namespace {

TEST_F(ParserImplTest, TypeDecl_ParsesType) {
  auto* i32 = tm()->Get(std::make_unique<ast::type::I32Type>());

  auto* p = parser("type a = i32");
  auto t = p->type_alias();
  EXPECT_FALSE(p->has_error());
  EXPECT_FALSE(t.errored);
  EXPECT_TRUE(t.matched);
  ASSERT_NE(t.value, nullptr);
  ASSERT_TRUE(t->IsAlias());
  auto* alias = t->AsAlias();
  ASSERT_TRUE(alias->type()->IsI32());
  ASSERT_EQ(alias->type(), i32);
}

TEST_F(ParserImplTest, TypeDecl_ParsesStruct_Ident) {
  ast::type::StructType str("B", {});

  auto* p = parser("type a = B");
  p->register_constructed("B", &str);

  auto t = p->type_alias();
  EXPECT_FALSE(p->has_error());
  EXPECT_FALSE(t.errored);
  EXPECT_TRUE(t.matched);
  ASSERT_NE(t.value, nullptr);
  ASSERT_TRUE(t->IsAlias());
  auto* alias = t->AsAlias();
  EXPECT_EQ(alias->name(), "a");
  ASSERT_TRUE(alias->type()->IsStruct());

  auto* s = alias->type()->AsStruct();
  EXPECT_EQ(s->name(), "B");
}

TEST_F(ParserImplTest, TypeDecl_MissingIdent) {
  auto* p = parser("type = i32");
  auto t = p->type_alias();
  EXPECT_TRUE(t.errored);
  EXPECT_FALSE(t.matched);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(t.value, nullptr);
  EXPECT_EQ(p->error(), "1:6: expected identifier for type alias");
}

TEST_F(ParserImplTest, TypeDecl_InvalidIdent) {
  auto* p = parser("type 123 = i32");
  auto t = p->type_alias();
  EXPECT_TRUE(t.errored);
  EXPECT_FALSE(t.matched);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(t.value, nullptr);
  EXPECT_EQ(p->error(), "1:6: expected identifier for type alias");
}

TEST_F(ParserImplTest, TypeDecl_MissingEqual) {
  auto* p = parser("type a i32");
  auto t = p->type_alias();
  EXPECT_TRUE(t.errored);
  EXPECT_FALSE(t.matched);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(t.value, nullptr);
  EXPECT_EQ(p->error(), "1:8: expected '=' for type alias");
}

TEST_F(ParserImplTest, TypeDecl_InvalidType) {
  auto* p = parser("type a = B");
  auto t = p->type_alias();
  EXPECT_TRUE(t.errored);
  EXPECT_FALSE(t.matched);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(t.value, nullptr);
  EXPECT_EQ(p->error(), "1:10: unknown constructed type 'B'");
}

}  // namespace
}  // namespace wgsl
}  // namespace reader
}  // namespace tint
