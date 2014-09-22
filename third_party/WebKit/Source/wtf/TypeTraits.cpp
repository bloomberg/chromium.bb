 /*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "TypeTraits.h"

#include "Assertions.h"

namespace WTF {

COMPILE_ASSERT(IsInteger<bool>::value, WTF_IsInteger_bool_true);
COMPILE_ASSERT(IsInteger<char>::value, WTF_IsInteger_char_true);
COMPILE_ASSERT(IsInteger<signed char>::value, WTF_IsInteger_signed_char_true);
COMPILE_ASSERT(IsInteger<unsigned char>::value, WTF_IsInteger_unsigned_char_true);
COMPILE_ASSERT(IsInteger<short>::value, WTF_IsInteger_short_true);
COMPILE_ASSERT(IsInteger<unsigned short>::value, WTF_IsInteger_unsigned_short_true);
COMPILE_ASSERT(IsInteger<int>::value, WTF_IsInteger_int_true);
COMPILE_ASSERT(IsInteger<unsigned>::value, WTF_IsInteger_unsigned_int_true);
COMPILE_ASSERT(IsInteger<long>::value, WTF_IsInteger_long_true);
COMPILE_ASSERT(IsInteger<unsigned long>::value, WTF_IsInteger_unsigned_long_true);
COMPILE_ASSERT(IsInteger<long long>::value, WTF_IsInteger_long_long_true);
COMPILE_ASSERT(IsInteger<unsigned long long>::value, WTF_IsInteger_unsigned_long_long_true);
#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
COMPILE_ASSERT(IsInteger<wchar_t>::value, WTF_IsInteger_wchar_t_true);
#endif
COMPILE_ASSERT(!IsInteger<char*>::value, WTF_IsInteger_char_pointer_false);
COMPILE_ASSERT(!IsInteger<const char*>::value, WTF_IsInteger_const_char_pointer_false);
COMPILE_ASSERT(!IsInteger<volatile char*>::value, WTF_IsInteger_volatile_char_pointer_false);
COMPILE_ASSERT(!IsInteger<double>::value, WTF_IsInteger_double_false);
COMPILE_ASSERT(!IsInteger<float>::value, WTF_IsInteger_float_false);

COMPILE_ASSERT(IsFloatingPoint<float>::value, WTF_IsFloatingPoint_float_true);
COMPILE_ASSERT(IsFloatingPoint<double>::value, WTF_IsFloatingPoint_double_true);
COMPILE_ASSERT(IsFloatingPoint<long double>::value, WTF_IsFloatingPoint_long_double_true);
COMPILE_ASSERT(!IsFloatingPoint<int>::value, WTF_IsFloatingPoint_int_false);

COMPILE_ASSERT(IsPointer<void*>::value, WTF_IsPointer_void_star_true);
COMPILE_ASSERT(IsPointer<char*>::value, WTF_IsPointer_char_star_true);
COMPILE_ASSERT(IsPointer<const char*>::value, WTF_IsPointer_const_char_star_true);
COMPILE_ASSERT(!IsPointer<char>::value, WTF_IsPointer_char_false);
COMPILE_ASSERT(!IsPointer<int>::value, WTF_IsPointer_int_false);
COMPILE_ASSERT(!IsPointer<long>::value, WTF_IsPointer_long_false);

enum TestEnum { TestEnumValue };
COMPILE_ASSERT(IsEnum<TestEnum>::value, WTF_IsEnum_enum_true);
COMPILE_ASSERT(!IsEnum<int>::value, WTF_IsEnum_int_false);
COMPILE_ASSERT(!IsEnum<TestEnum*>::value, WTF_IsEnum_enum_star_false);

COMPILE_ASSERT(IsScalar<TestEnum>::value, WTF_IsScalar_enum_true);
COMPILE_ASSERT(IsScalar<int>::value, WTF_IsScalar_int_true);
COMPILE_ASSERT(IsScalar<TestEnum*>::value, WTF_IsScalar_enum_star_true);
COMPILE_ASSERT(IsScalar<double>::value, WTF_IsScalar_double_true);

COMPILE_ASSERT(IsPod<bool>::value, WTF_IsPod_bool_true);
COMPILE_ASSERT(IsPod<char>::value, WTF_IsPod_char_true);
COMPILE_ASSERT(IsPod<signed char>::value, WTF_IsPod_signed_char_true);
COMPILE_ASSERT(IsPod<unsigned char>::value, WTF_IsPod_unsigned_char_true);
COMPILE_ASSERT(IsPod<short>::value, WTF_IsPod_short_true);
COMPILE_ASSERT(IsPod<unsigned short>::value, WTF_IsPod_unsigned_short_true);
COMPILE_ASSERT(IsPod<int>::value, WTF_IsPod_int_true);
COMPILE_ASSERT(IsPod<unsigned>::value, WTF_IsPod_unsigned_int_true);
COMPILE_ASSERT(IsPod<long>::value, WTF_IsPod_long_true);
COMPILE_ASSERT(IsPod<unsigned long>::value, WTF_IsPod_unsigned_long_true);
COMPILE_ASSERT(IsPod<long long>::value, WTF_IsPod_long_long_true);
COMPILE_ASSERT(IsPod<unsigned long long>::value, WTF_IsPod_unsigned_long_long_true);
#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
COMPILE_ASSERT(IsPod<wchar_t>::value, WTF_IsPod_wchar_t_true);
#endif
COMPILE_ASSERT(IsPod<char*>::value, WTF_IsPod_char_pointer_true);
COMPILE_ASSERT(IsPod<const char*>::value, WTF_IsPod_const_char_pointer_true);
COMPILE_ASSERT(IsPod<volatile char*>::value, WTF_IsPod_volatile_char_pointer_true);
COMPILE_ASSERT(IsPod<double>::value, WTF_IsPod_double_true);
COMPILE_ASSERT(IsPod<long double>::value, WTF_IsPod_long_double_true);
COMPILE_ASSERT(IsPod<float>::value, WTF_IsPod_float_true);
struct VirtualClass {
    virtual void A() { }
};
COMPILE_ASSERT(!IsTriviallyMoveAssignable<VirtualClass>::value, WTF_IsTriviallyMoveAssignable_VirtualClass_false);
COMPILE_ASSERT(!IsScalar<VirtualClass>::value, WTF_IsScalar_class_false);
COMPILE_ASSERT(IsScalar<VirtualClass*>::value, WTF_IsScalar_class_ptr_true);

struct DestructorClass {
    ~DestructorClass() { }
};
COMPILE_ASSERT(IsTriviallyMoveAssignable<DestructorClass>::value, WTF_IsTriviallyMoveAssignable_DestructorClass_true);
COMPILE_ASSERT(IsTriviallyCopyAssignable<DestructorClass>::value, WTF_IsTriviallyCopyAssignable_DestructorClass_true);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<DestructorClass>::value, WTF_IsTriviallyDefaultConstructable_DestructorClass_true);

struct MixedPrivate {
    int M2() { return m2; }
    int m1;
private:
    int m2;
};
COMPILE_ASSERT(IsTriviallyMoveAssignable<MixedPrivate>::value, WTF_IsTriviallyMoveAssignable_MixedPrivate_true);
COMPILE_ASSERT(IsTriviallyCopyAssignable<MixedPrivate>::value, WTF_IsTriviallyCopyAssignable_MixedPrivate_true);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<MixedPrivate>::value, WTF_IsTriviallyDefaultConstructable_MixedPrivate_true);
struct JustPrivate {
    int M2() { return m2; }
private:
    int m2;
};
COMPILE_ASSERT(IsTriviallyMoveAssignable<JustPrivate>::value, WTF_IsTriviallyMoveAssignable_JustPrivate_true);
COMPILE_ASSERT(IsTriviallyCopyAssignable<JustPrivate>::value, WTF_IsTriviallyCopyAssignable_JustPrivate_true);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<JustPrivate>::value, WTF_IsTriviallyDefaultConstructable_JustPrivate_true);
struct JustPublic {
    int m2;
};
COMPILE_ASSERT(IsTriviallyMoveAssignable<JustPublic>::value, WTF_IsTriviallyMoveAssignable_JustPublic_true);
COMPILE_ASSERT(IsTriviallyCopyAssignable<JustPublic>::value, WTF_IsTriviallyCopyAssignable_JustPublic_true);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<JustPublic>::value, WTF_IsTriviallyDefaultConstructable_JustPublic_true);
struct NestedInherited : public JustPublic, JustPrivate {
    float m3;
};
COMPILE_ASSERT(IsTriviallyMoveAssignable<NestedInherited>::value, WTF_IsTriviallyMoveAssignable_NestedInherited_true);
COMPILE_ASSERT(IsTriviallyCopyAssignable<NestedInherited>::value, WTF_IsTriviallyCopyAssignable_NestedInherited_true);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<NestedInherited>::value, WTF_IsTriviallyDefaultConstructable_NestedInherited_true);
struct NestedOwned {
    JustPublic m1;
    JustPrivate m2;
    float m3;
};

COMPILE_ASSERT(IsTriviallyMoveAssignable<NestedOwned>::value, WTF_IsTriviallyMoveAssignable_NestedOwned_true);
COMPILE_ASSERT(IsTriviallyCopyAssignable<NestedOwned>::value, WTF_IsTriviallyCopyAssignable_NestedOwned_true);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<NestedOwned>::value, WTF_IsTriviallyDefaultConstructable_NestedOwned_true);

class NonCopyableClass {
#if COMPILER_SUPPORTS(CXX_DELETED_FUNCTIONS)
    NonCopyableClass(const NonCopyableClass&) = delete;
    NonCopyableClass& operator=(const NonCopyableClass&) = delete;
#else
    NonCopyableClass(const NonCopyableClass&);
    NonCopyableClass& operator=(const NonCopyableClass&);
#endif // COMPILER_SUPPORTS(CXX_DELETED_FUNCTIONS)
};
#if 0 // Compilers don't get this "right" yet if using = delete.
COMPILE_ASSERT(!IsTriviallyMoveAssignable<NonCopyableClass>::value, WTF_IsTriviallyMoveAssignable_NonCopyableClass_false);
COMPILE_ASSERT(!IsTriviallyCopyAssignable<NonCopyableClass>::value, WTF_IsTriviallyCopyAssignable_NonCopyableClass_false);
COMPILE_ASSERT(IsTriviallyDefaultConstructible<NonCopyableClass>::value, WTF_IsTriviallyDefaultConstructable_NonCopyableClass_true);
#endif // 0

enum IsConvertibleToIntegerCheck { };
COMPILE_ASSERT(IsConvertibleToInteger<IsConvertibleToIntegerCheck>::value, WTF_IsConvertibleToInteger_enum_true);
COMPILE_ASSERT(IsConvertibleToInteger<bool>::value, WTF_IsConvertibleToInteger_bool_true);
COMPILE_ASSERT(IsConvertibleToInteger<char>::value, WTF_IsConvertibleToInteger_char_true);
COMPILE_ASSERT(IsConvertibleToInteger<signed char>::value, WTF_IsConvertibleToInteger_signed_char_true);
COMPILE_ASSERT(IsConvertibleToInteger<unsigned char>::value, WTF_IsConvertibleToInteger_unsigned_char_true);
COMPILE_ASSERT(IsConvertibleToInteger<short>::value, WTF_IsConvertibleToInteger_short_true);
COMPILE_ASSERT(IsConvertibleToInteger<unsigned short>::value, WTF_IsConvertibleToInteger_unsigned_short_true);
COMPILE_ASSERT(IsConvertibleToInteger<int>::value, WTF_IsConvertibleToInteger_int_true);
COMPILE_ASSERT(IsConvertibleToInteger<unsigned>::value, WTF_IsConvertibleToInteger_unsigned_int_true);
COMPILE_ASSERT(IsConvertibleToInteger<long>::value, WTF_IsConvertibleToInteger_long_true);
COMPILE_ASSERT(IsConvertibleToInteger<unsigned long>::value, WTF_IsConvertibleToInteger_unsigned_long_true);
COMPILE_ASSERT(IsConvertibleToInteger<long long>::value, WTF_IsConvertibleToInteger_long_long_true);
COMPILE_ASSERT(IsConvertibleToInteger<unsigned long long>::value, WTF_IsConvertibleToInteger_unsigned_long_long_true);
#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
COMPILE_ASSERT(IsConvertibleToInteger<wchar_t>::value, WTF_IsConvertibleToInteger_wchar_t_true);
#endif
COMPILE_ASSERT(IsConvertibleToInteger<double>::value, WTF_IsConvertibleToInteger_double_true);
COMPILE_ASSERT(IsConvertibleToInteger<long double>::value, WTF_IsConvertibleToInteger_long_double_true);
COMPILE_ASSERT(IsConvertibleToInteger<float>::value, WTF_IsConvertibleToInteger_float_true);
COMPILE_ASSERT(!IsConvertibleToInteger<char*>::value, WTF_IsConvertibleToInteger_char_pointer_false);
COMPILE_ASSERT(!IsConvertibleToInteger<const char*>::value, WTF_IsConvertibleToInteger_const_char_pointer_false);
COMPILE_ASSERT(!IsConvertibleToInteger<volatile char*>::value, WTF_IsConvertibleToInteger_volatile_char_pointer_false);
COMPILE_ASSERT(!IsConvertibleToInteger<IsConvertibleToInteger<bool> >::value, WTF_IsConvertibleToInteger_struct_false);

COMPILE_ASSERT((IsPointerConvertible<int, int>::Value), WTF_IsPointerConvertible_same_type_true);
COMPILE_ASSERT((!IsPointerConvertible<int, unsigned>::Value), WTF_IsPointerConvertible_int_to_unsigned_false);
COMPILE_ASSERT((IsPointerConvertible<int, const int>::Value), WTF_IsPointerConvertible_int_to_const_int_true);
COMPILE_ASSERT((!IsPointerConvertible<const int, int>::Value), WTF_IsPointerConvertible_const_int_to_int_false);
COMPILE_ASSERT((IsPointerConvertible<int, volatile int>::Value), WTF_IsPointerConvertible_int_to_volatile_int_true);

COMPILE_ASSERT((IsSameType<bool, bool>::value), WTF_IsSameType_bool_true);
COMPILE_ASSERT((IsSameType<int*, int*>::value), WTF_IsSameType_int_pointer_true);
COMPILE_ASSERT((!IsSameType<int, int*>::value), WTF_IsSameType_int_int_pointer_false);
COMPILE_ASSERT((!IsSameType<bool, const bool>::value), WTF_IsSameType_const_change_false);
COMPILE_ASSERT((!IsSameType<bool, volatile bool>::value), WTF_IsSameType_volatile_change_false);

template <typename T>
class TestBaseClass {
};

class TestDerivedClass : public TestBaseClass<int> {
};

COMPILE_ASSERT((IsSubclass<TestDerivedClass, TestBaseClass<int> >::value), WTF_Test_IsSubclass_Derived_From_Base);
COMPILE_ASSERT((!IsSubclass<TestBaseClass<int>, TestDerivedClass>::value), WTF_Test_IsSubclass_Base_From_Derived);
COMPILE_ASSERT((IsSubclassOfTemplate<TestDerivedClass, TestBaseClass>::value), WTF_Test_IsSubclassOfTemplate_Base_From_Derived);
COMPILE_ASSERT((IsSameType<RemoveTemplate<TestBaseClass<int>, TestBaseClass>::Type, int>::value), WTF_Test_RemoveTemplate);
COMPILE_ASSERT((IsSameType<RemoveTemplate<int, TestBaseClass>::Type, int>::value), WTF_Test_RemoveTemplate_WithoutTemplate);
COMPILE_ASSERT((IsPointerConvertible<TestDerivedClass, TestBaseClass<int> >::Value), WTF_Test_IsPointerConvertible_Derived_To_Base);
COMPILE_ASSERT((!IsPointerConvertible<TestBaseClass<int>, TestDerivedClass>::Value), WTF_Test_IsPointerConvertible_Base_To_Derived);

COMPILE_ASSERT((IsSameType<bool, RemoveConst<const bool>::Type>::value), WTF_test_RemoveConst_const_bool);
COMPILE_ASSERT((!IsSameType<bool, RemoveConst<volatile bool>::Type>::value), WTF_test_RemoveConst_volatile_bool);

COMPILE_ASSERT((IsSameType<bool, RemoveVolatile<bool>::Type>::value), WTF_test_RemoveVolatile_bool);
COMPILE_ASSERT((!IsSameType<bool, RemoveVolatile<const bool>::Type>::value), WTF_test_RemoveVolatile_const_bool);
COMPILE_ASSERT((IsSameType<bool, RemoveVolatile<volatile bool>::Type>::value), WTF_test_RemoveVolatile_volatile_bool);

COMPILE_ASSERT((IsSameType<bool, RemoveConstVolatile<bool>::Type>::value), WTF_test_RemoveConstVolatile_bool);
COMPILE_ASSERT((IsSameType<bool, RemoveConstVolatile<const bool>::Type>::value), WTF_test_RemoveConstVolatile_const_bool);
COMPILE_ASSERT((IsSameType<bool, RemoveConstVolatile<volatile bool>::Type>::value), WTF_test_RemoveConstVolatile_volatile_bool);
COMPILE_ASSERT((IsSameType<bool, RemoveConstVolatile<const volatile bool>::Type>::value), WTF_test_RemoveConstVolatile_const_volatile_bool);

COMPILE_ASSERT((IsSameType<int, RemovePointer<int>::Type>::value), WTF_Test_RemovePointer_int);
COMPILE_ASSERT((IsSameType<int, RemovePointer<int*>::Type>::value), WTF_Test_RemovePointer_int_pointer);
COMPILE_ASSERT((!IsSameType<int, RemovePointer<int**>::Type>::value), WTF_Test_RemovePointer_int_pointer_pointer);

COMPILE_ASSERT((IsSameType<int, RemoveReference<int>::Type>::value), WTF_Test_RemoveReference_int);
COMPILE_ASSERT((IsSameType<int, RemoveReference<int&>::Type>::value), WTF_Test_RemoveReference_int_reference);


typedef int IntArray[];
typedef int IntArraySized[4];

COMPILE_ASSERT((IsArray<IntArray>::value), WTF_Test_IsArray_int_array);
COMPILE_ASSERT((IsArray<IntArraySized>::value), WTF_Test_IsArray_int_sized_array);

COMPILE_ASSERT((IsSameType<int, RemoveExtent<IntArray>::Type>::value), WTF_Test_RemoveExtent_int_array);
COMPILE_ASSERT((IsSameType<int, RemoveExtent<IntArraySized>::Type>::value), WTF_Test_RemoveReference_int_sized_array);

} // namespace WTF
