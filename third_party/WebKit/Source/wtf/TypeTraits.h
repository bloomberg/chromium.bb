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

#ifndef TypeTraits_h
#define TypeTraits_h

#include <cstddef>
#include <type_traits>
#include <utility>

#include "wtf/Compiler.h"

namespace WTF {

// Returns a string that contains the type name of |T| as a substring.
template<typename T>
inline const char* getStringWithTypeName()
{
    return WTF_PRETTY_FUNCTION;
}

template<typename T> class RawPtr;

template <typename T> struct IsWeak {
    static const bool value = false;
};

enum WeakHandlingFlag {
    NoWeakHandlingInCollections,
    WeakHandlingInCollections
};

template <typename T> struct IsTriviallyCopyAssignable {
    static const bool value = __has_trivial_assign(T);
};

template <typename T> struct IsTriviallyMoveAssignable {
    static const bool value = __has_trivial_assign(T);
};

template <typename T> struct IsTriviallyDefaultConstructible {
    static const bool value = __has_trivial_constructor(T);
};

template <typename T> struct IsTriviallyDestructible {
    static const bool value = __has_trivial_destructor(T);
};

template <typename T, typename U> class IsSubclass {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    static YesType subclassCheck(U*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <typename... V> class U> class IsSubclassOfTemplate {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename... W> static YesType subclassCheck(U<W...>*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W> class U>
class IsSubclassOfTemplateTypenameSize {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename X, size_t Y> static YesType subclassCheck(U<X, Y>*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W, typename X> class U>
class IsSubclassOfTemplateTypenameSizeTypename {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename Y, size_t Z, typename A> static YesType subclassCheck(U<Y, Z, A>*);
    static NoType subclassCheck(...);
    static T* t;
public:
    static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
};

template <typename T, template <class V> class OuterTemplate>
struct RemoveTemplate {
    typedef T Type;
};

template <typename T, template <class V> class OuterTemplate>
struct RemoveTemplate<OuterTemplate<T>, OuterTemplate> {
    typedef T Type;
};

#if (COMPILER(MSVC) || !GCC_VERSION_AT_LEAST(4, 9, 0)) && !COMPILER(CLANG)
// FIXME: MSVC bug workaround. Remove once MSVC STL is fixed.
// FIXME: GCC before 4.9.0 seems to have the same issue.
// C++ 2011 Spec (ISO/IEC 14882:2011(E)) 20.9.6.2 Table 51 states that
// the template parameters shall be a complete type if they are different types.
// However, MSVC checks for type completeness even if they are the same type.
// Here, we use a template specialization for same type case to allow incomplete
// types.

template <typename T, typename U> class IsBaseOf {
public:
    static const bool value = std::is_base_of<T, U>::value;
};

template <typename T> class IsBaseOf<T, T> {
public:
    static const bool value = true;
};

#define EnsurePtrConvertibleArgDecl(From, To) \
    typename std::enable_if<WTF::IsBaseOf<To, From>::value>::type* = nullptr
#define EnsurePtrConvertibleArgDefn(From, To) \
    typename std::enable_if<WTF::IsBaseOf<To, From>::value>::type*
#else
#define EnsurePtrConvertibleArgDecl(From, To) \
    typename std::enable_if<std::is_base_of<To, From>::value>::type* = nullptr
#define EnsurePtrConvertibleArgDefn(From, To) \
    typename std::enable_if<std::is_base_of<To, From>::value>::type*
#endif

} // namespace WTF

namespace blink {

class Visitor;

} // namespace blink

namespace WTF {

template <typename T>
class NeedsTracing {
    typedef char YesType;
    typedef struct NoType {
        char padding[8];
    } NoType;

    // Note that this also checks if a superclass of V has a trace method.
    template <typename V> static YesType checkHasTraceMethod(V* v, blink::Visitor* p = nullptr, typename std::enable_if<std::is_same<decltype(v->trace(p)), void>::value>::type* g = nullptr);
    template <typename V> static NoType checkHasTraceMethod(...);
public:
    // We add sizeof(T) to both sides here, because we want it to fail for
    // incomplete types. Otherwise it just assumes that incomplete types do not
    // have a trace method, which may not be true.
    static const bool value = sizeof(YesType) + sizeof(T) == sizeof(checkHasTraceMethod<T>(nullptr)) + sizeof(T);
};

// Convenience template wrapping the NeedsTracingLazily template in
// Collection Traits. It helps make the code more readable.
template <typename Traits>
class NeedsTracingTrait {
public:
    static const bool value = Traits::template NeedsTracingLazily<>::value;
};

template <typename T, typename U>
struct NeedsTracing<std::pair<T, U>> {
    static const bool value = NeedsTracing<T>::value || NeedsTracing<U>::value || IsWeak<T>::value || IsWeak<U>::value;
};

// This is used to check that DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects are not
// stored in off-heap Vectors, HashTables etc.
template <typename T>
struct AllowsOnlyPlacementNew {
private:
    using YesType = char;
    struct NoType {
        char padding[8];
    };

    template <typename U> static YesType checkMarker(typename U::IsAllowOnlyPlacementNew*);
    template <typename U> static NoType checkMarker(...);
public:
    static const bool value = sizeof(checkMarker<T>(nullptr)) == sizeof(YesType);
};

template<typename T>
class IsGarbageCollectedType {
    typedef char YesType;
    typedef struct NoType {
        char padding[8];
    } NoType;

    template <typename U> static YesType checkGarbageCollectedType(typename U::IsGarbageCollectedTypeMarker*);
    template <typename U> static NoType checkGarbageCollectedType(...);
public:
    static const bool value = (sizeof(YesType) == sizeof(checkGarbageCollectedType<T>(nullptr)));
};

template<typename T>
class IsPersistentReferenceType {
    typedef char YesType;
    typedef struct NoType {
        char padding[8];
    } NoType;

    template <typename U> static YesType checkPersistentReferenceType(typename U::IsPersistentReferenceTypeMarker*);
    template <typename U> static NoType checkPersistentReferenceType(...);
public:
    static const bool value = (sizeof(YesType) == sizeof(checkPersistentReferenceType<T>(nullptr)));
};

template<typename T>
class IsPointerToGarbageCollectedType {
public:
    static const bool value = false;
};
template<typename T>
class IsPointerToGarbageCollectedType<T*> {
public:
    static const bool value = IsGarbageCollectedType<T>::value;
};
template<typename T>
class IsPointerToGarbageCollectedType<RawPtr<T>> {
public:
    static const bool value = IsGarbageCollectedType<T>::value;
};

} // namespace WTF

#endif // TypeTraits_h
