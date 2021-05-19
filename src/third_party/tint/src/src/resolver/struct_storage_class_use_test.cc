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

#include "src/resolver/resolver.h"

#include "gmock/gmock.h"
#include "src/resolver/resolver_test_helper.h"
#include "src/semantic/struct.h"

using ::testing::UnorderedElementsAre;

namespace tint {
namespace resolver {
namespace {

using ResolverStorageClassUseTest = ResolverTest;

TEST_F(ResolverStorageClassUseTest, UnreachableStruct) {
  auto* s = Structure("S", {Member("a", ty.f32())});

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_TRUE(sem->StorageClassUsage().empty());
}

TEST_F(ResolverStorageClassUseTest, StructReachableFromParameter) {
  auto* s = Structure("S", {Member("a", ty.f32())});

  Func("f", {Var("param", s, ast::StorageClass::kNone)}, ty.void_(), {}, {});

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kNone));
}

TEST_F(ResolverStorageClassUseTest, StructReachableFromReturnType) {
  auto* s = Structure("S", {Member("a", ty.f32())});

  Func("f", {}, s, {Return(Construct(s))}, {});

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kNone));
}

TEST_F(ResolverStorageClassUseTest, StructReachableFromGlobal) {
  auto* s = Structure("S", {Member("a", ty.f32())});

  Global("g", s, ast::StorageClass::kStorage);

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kStorage));
}

TEST_F(ResolverStorageClassUseTest, StructReachableViaGlobalAlias) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  auto* a = ty.alias("A", s);
  Global("g", a, ast::StorageClass::kStorage);

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kStorage));
}

TEST_F(ResolverStorageClassUseTest, StructReachableViaGlobalStruct) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  auto* o = Structure("O", {Member("a", s)});
  Global("g", o, ast::StorageClass::kStorage);

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kStorage));
}

TEST_F(ResolverStorageClassUseTest, StructReachableViaGlobalArray) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  auto* a = ty.array(s, 3);
  Global("g", a, ast::StorageClass::kStorage);

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kStorage));
}

TEST_F(ResolverStorageClassUseTest, StructReachableFromLocal) {
  auto* s = Structure("S", {Member("a", ty.f32())});

  WrapInFunction(Var("g", s, ast::StorageClass::kFunction));

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kFunction));
}

TEST_F(ResolverStorageClassUseTest, StructReachableViaLocalAlias) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  auto* a = ty.alias("A", s);
  WrapInFunction(Var("g", a, ast::StorageClass::kFunction));

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kFunction));
}

TEST_F(ResolverStorageClassUseTest, StructReachableViaLocalStruct) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  auto* o = Structure("O", {Member("a", s)});
  WrapInFunction(Var("g", o, ast::StorageClass::kFunction));

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kFunction));
}

TEST_F(ResolverStorageClassUseTest, StructReachableViaLocalArray) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  auto* a = ty.array(s, 3);
  WrapInFunction(Var("g", a, ast::StorageClass::kFunction));

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kFunction));
}

TEST_F(ResolverStorageClassUseTest, StructMultipleStorageClassUses) {
  auto* s = Structure("S", {Member("a", ty.f32())});
  Global("x", s, ast::StorageClass::kStorage);
  Global("y", s, ast::StorageClass::kUniform);
  WrapInFunction(Var("g", s, ast::StorageClass::kFunction));

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* sem = Sem().Get(s);
  ASSERT_NE(sem, nullptr);
  EXPECT_THAT(sem->StorageClassUsage(),
              UnorderedElementsAre(ast::StorageClass::kStorage,
                                   ast::StorageClass::kUniform,
                                   ast::StorageClass::kFunction));
}

}  // namespace
}  // namespace resolver
}  // namespace tint
