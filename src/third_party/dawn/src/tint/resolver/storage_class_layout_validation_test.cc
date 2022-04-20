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

#include "src/tint/resolver/resolver.h"

#include "gmock/gmock.h"
#include "src/tint/resolver/resolver_test_helper.h"

namespace tint::resolver {
namespace {

using ResolverStorageClassLayoutValidationTest = ResolverTest;

// Detect unaligned member for storage buffers
TEST_F(ResolverStorageClassLayoutValidationTest,
       StorageBuffer_UnalignedMember) {
  // struct S {
  //     @size(5) a : f32;
  //     @align(1) b : f32;
  // };
  // @group(0) @binding(0)
  // var<storage> a : S;

  Structure(Source{{12, 34}}, "S",
            {Member("a", ty.f32(), {MemberSize(5)}),
             Member(Source{{34, 56}}, "b", ty.f32(), {MemberAlign(1)})});

  Global(Source{{78, 90}}, "a", ty.type_name("S"), ast::StorageClass::kStorage,
         GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(34:56 error: the offset of a struct member of type 'f32' in storage class 'storage' must be a multiple of 4 bytes, but 'b' is currently at offset 5. Consider setting @align(4) on this member
12:34 note: see layout of struct:
/*           align(4) size(12) */ struct S {
/* offset(0) align(4) size( 5) */   a : f32;
/* offset(5) align(1) size( 4) */   b : f32;
/* offset(9) align(1) size( 3) */   // -- implicit struct size padding --;
/*                             */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       StorageBuffer_UnalignedMember_SuggestedFix) {
  // struct S {
  //     @size(5) a : f32;
  //     @align(4) b : f32;
  // };
  // @group(0) @binding(0)
  // var<storage> a : S;

  Structure(Source{{12, 34}}, "S",
            {Member("a", ty.f32(), {MemberSize(5)}),
             Member(Source{{34, 56}}, "b", ty.f32(), {MemberAlign(4)})});

  Global(Source{{78, 90}}, "a", ty.type_name("S"), ast::StorageClass::kStorage,
         GroupAndBinding(0, 0));

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

// Detect unaligned struct member for uniform buffers
TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_UnalignedMember_Struct) {
  // struct Inner {
  //   scalar : i32;
  // };
  //
  // struct Outer {
  //   scalar : f32;
  //   inner : Inner;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Structure(Source{{12, 34}}, "Inner", {Member("scalar", ty.i32())});

  Structure(Source{{34, 56}}, "Outer",
            {
                Member("scalar", ty.f32()),
                Member(Source{{56, 78}}, "inner", ty.type_name("Inner")),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(56:78 error: the offset of a struct member of type 'Inner' in storage class 'uniform' must be a multiple of 16 bytes, but 'inner' is currently at offset 4. Consider setting @align(16) on this member
34:56 note: see layout of struct:
/*           align(4) size(8) */ struct Outer {
/* offset(0) align(4) size(4) */   scalar : f32;
/* offset(4) align(4) size(4) */   inner : Inner;
/*                            */ };
12:34 note: and layout of struct member:
/*           align(4) size(4) */ struct Inner {
/* offset(0) align(4) size(4) */   scalar : i32;
/*                            */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_UnalignedMember_Struct_SuggestedFix) {
  // struct Inner {
  //   scalar : i32;
  // };
  //
  // struct Outer {
  //   scalar : f32;
  //   @align(16) inner : Inner;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Structure(Source{{12, 34}}, "Inner", {Member("scalar", ty.i32())});

  Structure(Source{{34, 56}}, "Outer",
            {
                Member("scalar", ty.f32()),
                Member(Source{{56, 78}}, "inner", ty.type_name("Inner"),
                       {MemberAlign(16)}),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

// Detect unaligned array member for uniform buffers
TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_UnalignedMember_Array) {
  // type Inner = @stride(16) array<f32, 10>;
  //
  // struct Outer {
  //   scalar : f32;
  //   inner : Inner;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;
  Alias("Inner", ty.array(ty.f32(), 10, 16));

  Structure(Source{{12, 34}}, "Outer",
            {
                Member("scalar", ty.f32()),
                Member(Source{{56, 78}}, "inner", ty.type_name("Inner")),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(56:78 error: the offset of a struct member of type '@stride(16) array<f32, 10>' in storage class 'uniform' must be a multiple of 16 bytes, but 'inner' is currently at offset 4. Consider setting @align(16) on this member
12:34 note: see layout of struct:
/*             align(4) size(164) */ struct Outer {
/* offset(  0) align(4) size(  4) */   scalar : f32;
/* offset(  4) align(4) size(160) */   inner : @stride(16) array<f32, 10>;
/*                                */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_UnalignedMember_Array_SuggestedFix) {
  // type Inner = @stride(16) array<f32, 10>;
  //
  // struct Outer {
  //   scalar : f32;
  //   @align(16) inner : Inner;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;
  Alias("Inner", ty.array(ty.f32(), 10, 16));

  Structure(Source{{12, 34}}, "Outer",
            {
                Member("scalar", ty.f32()),
                Member(Source{{34, 56}}, "inner", ty.type_name("Inner"),
                       {MemberAlign(16)}),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

// Detect uniform buffers with byte offset between 2 members that is not a
// multiple of 16 bytes
TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_MembersOffsetNotMultipleOf16) {
  // struct Inner {
  //   @align(1) @size(5) scalar : i32;
  // };
  //
  // struct Outer {
  //   inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Structure(Source{{12, 34}}, "Inner",
            {Member("scalar", ty.i32(), {MemberAlign(1), MemberSize(5)})});

  Structure(Source{{34, 56}}, "Outer",
            {
                Member(Source{{56, 78}}, "inner", ty.type_name("Inner")),
                Member(Source{{78, 90}}, "scalar", ty.i32()),
            });

  Global(Source{{22, 24}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(78:90 error: uniform storage requires that the number of bytes between the start of the previous member of type struct and the current member be a multiple of 16 bytes, but there are currently 8 bytes between 'inner' and 'scalar'. Consider setting @align(16) on this member
34:56 note: see layout of struct:
/*            align(4) size(12) */ struct Outer {
/* offset( 0) align(1) size( 5) */   inner : Inner;
/* offset( 5) align(1) size( 3) */   // -- implicit field alignment padding --;
/* offset( 8) align(4) size( 4) */   scalar : i32;
/*                              */ };
12:34 note: and layout of previous member struct:
/*           align(1) size(5) */ struct Inner {
/* offset(0) align(1) size(5) */   scalar : i32;
/*                            */ };
22:24 note: see declaration of variable)");
}

// See https://crbug.com/tint/1344
TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_MembersOffsetNotMultipleOf16_InnerMoreMembersThanOuter) {
  // struct Inner {
  //   a : i32;
  //   b : i32;
  //   c : i32;
  //   @align(1) @size(5) scalar : i32;
  // };
  //
  // struct Outer {
  //   inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Structure(Source{{12, 34}}, "Inner",
            {
                Member("a", ty.i32()),
                Member("b", ty.i32()),
                Member("c", ty.i32()),
                Member("scalar", ty.i32(), {MemberAlign(1), MemberSize(5)}),
            });

  Structure(Source{{34, 56}}, "Outer",
            {
                Member(Source{{56, 78}}, "inner", ty.type_name("Inner")),
                Member(Source{{78, 90}}, "scalar", ty.i32()),
            });

  Global(Source{{22, 24}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(78:90 error: uniform storage requires that the number of bytes between the start of the previous member of type struct and the current member be a multiple of 16 bytes, but there are currently 20 bytes between 'inner' and 'scalar'. Consider setting @align(16) on this member
34:56 note: see layout of struct:
/*            align(4) size(24) */ struct Outer {
/* offset( 0) align(4) size(20) */   inner : Inner;
/* offset(20) align(4) size( 4) */   scalar : i32;
/*                              */ };
12:34 note: and layout of previous member struct:
/*            align(4) size(20) */ struct Inner {
/* offset( 0) align(4) size( 4) */   a : i32;
/* offset( 4) align(4) size( 4) */   b : i32;
/* offset( 8) align(4) size( 4) */   c : i32;
/* offset(12) align(1) size( 5) */   scalar : i32;
/* offset(17) align(1) size( 3) */   // -- implicit struct size padding --;
/*                              */ };
22:24 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_MembersOffsetNotMultipleOf16_SuggestedFix) {
  // struct Inner {
  //   @align(1) @size(5) scalar : i32;
  // };
  //
  // struct Outer {
  //   @align(16) inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Structure(Source{{12, 34}}, "Inner",
            {Member("scalar", ty.i32(), {MemberAlign(1), MemberSize(5)})});

  Structure(Source{{34, 56}}, "Outer",
            {
                Member(Source{{56, 78}}, "inner", ty.type_name("Inner")),
                Member(Source{{78, 90}}, "scalar", ty.i32(), {MemberAlign(16)}),
            });

  Global(Source{{22, 34}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

// Make sure that this doesn't fail validation because vec3's align is 16, but
// size is 12. 's' should be at offset 12, which is okay here.
TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_Vec3MemberOffset_NoFail) {
  // struct ScalarPackedAtEndOfVec3 {
  //     v : vec3<f32>;
  //     s : f32;
  // };
  // @group(0) @binding(0)
  // var<uniform> a : ScalarPackedAtEndOfVec3;

  Structure("ScalarPackedAtEndOfVec3", {
                                           Member("v", ty.vec3(ty.f32())),
                                           Member("s", ty.f32()),
                                       });

  Global(Source{{78, 90}}, "a", ty.type_name("ScalarPackedAtEndOfVec3"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

// Detect array stride must be a multiple of 16 bytes for uniform buffers
TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_InvalidArrayStride_Scalar) {
  // type Inner = array<f32, 10>;
  //
  // struct Outer {
  //   inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Alias("Inner", ty.array(ty.f32(), 10));

  Structure(Source{{12, 34}}, "Outer",
            {
                Member("inner", ty.type_name(Source{{34, 56}}, "Inner")),
                Member("scalar", ty.i32()),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(34:56 error: uniform storage requires that array elements be aligned to 16 bytes, but array element alignment is currently 4. Consider using a vector or struct as the element type instead.
12:34 note: see layout of struct:
/*            align(4) size(44) */ struct Outer {
/* offset( 0) align(4) size(40) */   inner : array<f32, 10>;
/* offset(40) align(4) size( 4) */   scalar : i32;
/*                              */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_InvalidArrayStride_Vector) {
  // type Inner = array<vec2<f32>, 10>;
  //
  // struct Outer {
  //   inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Alias("Inner", ty.array(ty.vec2<f32>(), 10));

  Structure(Source{{12, 34}}, "Outer",
            {
                Member("inner", ty.type_name(Source{{34, 56}}, "Inner")),
                Member("scalar", ty.i32()),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(34:56 error: uniform storage requires that array elements be aligned to 16 bytes, but array element alignment is currently 8. Consider using a vec4 instead.
12:34 note: see layout of struct:
/*            align(8) size(88) */ struct Outer {
/* offset( 0) align(8) size(80) */   inner : array<vec2<f32>, 10>;
/* offset(80) align(4) size( 4) */   scalar : i32;
/* offset(84) align(1) size( 4) */   // -- implicit struct size padding --;
/*                              */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_InvalidArrayStride_Struct) {
  // struct ArrayElem {
  //   a : f32;
  //   b : i32;
  // }
  // type Inner = array<ArrayElem, 10>;
  //
  // struct Outer {
  //   inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  auto* array_elem = Structure("ArrayElem", {
                                                Member("a", ty.f32()),
                                                Member("b", ty.i32()),
                                            });
  Alias("Inner", ty.array(ty.Of(array_elem), 10));

  Structure(Source{{12, 34}}, "Outer",
            {
                Member("inner", ty.type_name(Source{{34, 56}}, "Inner")),
                Member("scalar", ty.i32()),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(34:56 error: uniform storage requires that array elements be aligned to 16 bytes, but array element alignment is currently 8. Consider using the @size attribute on the last struct member.
12:34 note: see layout of struct:
/*            align(4) size(84) */ struct Outer {
/* offset( 0) align(4) size(80) */   inner : array<ArrayElem, 10>;
/* offset(80) align(4) size( 4) */   scalar : i32;
/*                              */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_InvalidArrayStride_TopLevelArray) {
  // @group(0) @binding(0)
  // var<uniform> a : array<f32, 4>;
  Global(Source{{78, 90}}, "a", ty.array(Source{{34, 56}}, ty.f32(), 4),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(34:56 error: uniform storage requires that array elements be aligned to 16 bytes, but array element alignment is currently 4. Consider using a vector or struct as the element type instead.)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_InvalidArrayStride_NestedArray) {
  // struct Outer {
  //   inner : array<array<f32, 4>, 4>
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : array<Outer, 4>;

  Structure(
      Source{{12, 34}}, "Outer",
      {
          Member("inner", ty.array(Source{{34, 56}}, ty.array(ty.f32(), 4), 4)),
      });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(34:56 error: uniform storage requires that array elements be aligned to 16 bytes, but array element alignment is currently 4. Consider using a vector or struct as the element type instead.
12:34 note: see layout of struct:
/*            align(4) size(64) */ struct Outer {
/* offset( 0) align(4) size(64) */   inner : array<array<f32, 4>, 4>;
/*                              */ };
78:90 note: see declaration of variable)");
}

TEST_F(ResolverStorageClassLayoutValidationTest,
       UniformBuffer_InvalidArrayStride_SuggestedFix) {
  // type Inner = @stride(16) array<f32, 10>;
  //
  // struct Outer {
  //   inner : Inner;
  //   scalar : i32;
  // };
  //
  // @group(0) @binding(0)
  // var<uniform> a : Outer;

  Alias("Inner", ty.array(ty.f32(), 10, 16));

  Structure(Source{{12, 34}}, "Outer",
            {
                Member("inner", ty.type_name(Source{{34, 56}}, "Inner")),
                Member("scalar", ty.i32()),
            });

  Global(Source{{78, 90}}, "a", ty.type_name("Outer"),
         ast::StorageClass::kUniform, GroupAndBinding(0, 0));

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

}  // namespace
}  // namespace tint::resolver
