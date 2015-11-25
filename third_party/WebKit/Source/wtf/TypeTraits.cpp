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
#include "Noncopyable.h"

#include <type_traits>

namespace WTF {

struct VirtualClass {
    virtual void A() { }
};
static_assert(!IsTriviallyMoveAssignable<VirtualClass>::value, "VirtualClass should not be trivially move assignable");

struct DestructorClass {
    ~DestructorClass() { }
};
static_assert(IsTriviallyMoveAssignable<DestructorClass>::value, "DestructorClass should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<DestructorClass>::value, "DestructorClass should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<DestructorClass>::value, "DestructorClass should have a trivial default constructor");

struct MixedPrivate {
    int M2() { return m2; }
    int m1;
private:
    int m2;
};
static_assert(IsTriviallyMoveAssignable<MixedPrivate>::value, "MixedPrivate should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<MixedPrivate>::value, "MixedPrivate should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<MixedPrivate>::value, "MixedPrivate should have a trivial default constructor");
struct JustPrivate {
    int M2() { return m2; }
private:
    int m2;
};
static_assert(IsTriviallyMoveAssignable<JustPrivate>::value, "JustPrivate should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<JustPrivate>::value, "JustPrivate should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<JustPrivate>::value, "JustPrivate should have a trivial default constructor");
struct JustPublic {
    int m2;
};
static_assert(IsTriviallyMoveAssignable<JustPublic>::value, "JustPublic should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<JustPublic>::value, "JustPublic should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<JustPublic>::value, "JustPublic should have a trivial default constructor");
struct NestedInherited : public JustPublic, JustPrivate {
    float m3;
};
static_assert(IsTriviallyMoveAssignable<NestedInherited>::value, "NestedInherited should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<NestedInherited>::value, "NestedInherited should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<NestedInherited>::value, "NestedInherited should have a trivial default constructor");
struct NestedOwned {
    JustPublic m1;
    JustPrivate m2;
    float m3;
};

static_assert(IsTriviallyMoveAssignable<NestedOwned>::value, "NestedOwned should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<NestedOwned>::value, "NestedOwned should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<NestedOwned>::value, "NestedOwned should have a trivial default constructor");

class NonCopyableClass {
    WTF_MAKE_NONCOPYABLE(NonCopyableClass);
};
#if 0 // Compilers don't get this "right" yet if using = delete.
static_assert(!IsTriviallyMoveAssignable<NonCopyableClass>::value, "NonCopyableClass should not be trivially move assignable");
static_assert(!IsTriviallyCopyAssignable<NonCopyableClass>::value, "NonCopyableClass should not be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<NonCopyableClass>::value, "NonCopyableClass should have a trivial default constructor");
#endif // 0

enum IsConvertibleToIntegerCheck { };
static_assert(IsConvertibleToInteger<IsConvertibleToIntegerCheck>::value, "enum should be convertible to integer");
static_assert(IsConvertibleToInteger<bool>::value, "bool should be convertible to integer");
static_assert(IsConvertibleToInteger<char>::value, "char should be convertible to integer");
static_assert(IsConvertibleToInteger<signed char>::value, "signed char should be convertible to integer");
static_assert(IsConvertibleToInteger<unsigned char>::value, "unsigned char should be convertible to integer");
static_assert(IsConvertibleToInteger<short>::value, "short should be convertible to integer");
static_assert(IsConvertibleToInteger<unsigned short>::value, "unsigned short should be convertible to integer");
static_assert(IsConvertibleToInteger<int>::value, "int should be convertible to integer");
static_assert(IsConvertibleToInteger<unsigned>::value, "unsigned int should be convertible to integer");
static_assert(IsConvertibleToInteger<long>::value, "long should be convertible to integer");
static_assert(IsConvertibleToInteger<unsigned long>::value, "unsigned long should be convertible to integer");
static_assert(IsConvertibleToInteger<long long>::value, "long long should be convertible to integer");
static_assert(IsConvertibleToInteger<unsigned long long>::value, "unsigned long long should be convertible to integer");
#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
static_assert(IsConvertibleToInteger<wchar_t>::value, "whcar_t should be convertible to integer");
#endif
static_assert(IsConvertibleToInteger<double>::value, "double should be convertible to integer");
static_assert(IsConvertibleToInteger<long double>::value, "long double should be convertible to integer");
static_assert(IsConvertibleToInteger<float>::value, "float should be convertible to integer");
static_assert(!IsConvertibleToInteger<char*>::value, "char* should not be convertible to integer");
static_assert(!IsConvertibleToInteger<const char*>::value, "const char* should not be convertible to integer");
static_assert(!IsConvertibleToInteger<volatile char*>::value, "volatile char* should not be convertible to integer");
static_assert(!IsConvertibleToInteger<IsConvertibleToInteger<bool>>::value, "struct should not be convertible to integer");

static_assert((IsPointerConvertible<int, int>::Value), "pointers to the same type should be convertible");
static_assert((!IsPointerConvertible<int, unsigned>::Value), "int pointers should not be convertible to unsigned pointers");
static_assert((IsPointerConvertible<int, const int>::Value), "int pointers should be convertible to const int pointers");
static_assert((!IsPointerConvertible<const int, int>::Value), "const int* should not be convertible to int*");
static_assert((IsPointerConvertible<int, volatile int>::Value), "int* should be convertible to volatile int*");

static_assert((IsSameType<bool, bool>::value), "bool should be the same type as itself");
static_assert((IsSameType<int*, int*>::value), "int* should be the same type as itself");
static_assert((!IsSameType<int, int*>::value), "int should not be the same type as int*");
static_assert((!IsSameType<bool, const bool>::value), "T should not be the same type as const T");
static_assert((!IsSameType<bool, volatile bool>::value), "T should not be the same type as volatile T");

template <typename T>
class TestBaseClass {
};

class TestDerivedClass : public TestBaseClass<int> {
};

static_assert((IsSubclass<TestDerivedClass, TestBaseClass<int>>::value), "Derived class should be a subclass of its base");
static_assert((!IsSubclass<TestBaseClass<int>, TestDerivedClass>::value), "Base class should not be a sublass of a derived class");
static_assert((IsSubclassOfTemplate<TestDerivedClass, TestBaseClass>::value), "Derived class should be a subclass of template from its base");
static_assert((IsSameType<RemoveTemplate<TestBaseClass<int>, TestBaseClass>::Type, int>::value), "RemoveTemplate should remove the template typename from the type");
static_assert((IsSameType<RemoveTemplate<int, TestBaseClass>::Type, int>::value), "RemoveTemplate should not alter non-template types");
static_assert((IsPointerConvertible<TestDerivedClass, TestBaseClass<int>>::Value), "Derived class pointers should be convertible to base class pointers");
static_assert((!IsPointerConvertible<TestBaseClass<int>, TestDerivedClass>::Value), "Base class pointers should not be convertible to derived class pointers");

typedef int IntArray[];
typedef int IntArraySized[4];

} // namespace WTF
