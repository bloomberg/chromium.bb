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
#include <utility>

namespace WTF {

template<typename T> class RawPtr;

// The following are provided in this file:
//
//   IsConvertibleToInteger<T>::value
//
//   IsSameType<T, U>::value
//
//   static_assert's in TypeTraits.cpp illustrate their usage and what they do.

template <bool Predicate, class T = void> struct EnableIf;
template <class T> struct EnableIf<true, T> { typedef T Type; };

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

template <typename T> class IsConvertibleToInteger {
    // Avoid "possible loss of data" warning when using Microsoft's C++ compiler
    // by not converting int's to doubles.
    template <bool performCheck, typename U> class IsConvertibleToDouble;
    template <typename U> class IsConvertibleToDouble<false, U> {
    public:
        static const bool value = false;
    };

    template <typename U> class IsConvertibleToDouble<true, U> {
        typedef char YesType;
        struct NoType {
            char padding[8];
        };

        static YesType floatCheck(long double);
        static NoType floatCheck(...);
        static T& t;
    public:
        static const bool value = sizeof(floatCheck(t)) == sizeof(YesType);
    };

public:
    static const bool value = std::is_integral<T>::value || IsConvertibleToDouble<!std::is_integral<T>::value, T>::value;
};

template <typename From, typename To> class IsPointerConvertible {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    static YesType convertCheck(To* x);
    static NoType convertCheck(...);
public:
    enum {
        Value = (sizeof(YesType) == sizeof(convertCheck(static_cast<From*>(0))))
    };
};

template <typename T, typename U> struct IsSameType {
    static const bool value = false;
};

template <typename T> struct IsSameType<T, T> {
    static const bool value = true;
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

// Determines whether this type has a vtable.
template <typename T> struct IsPolymorphic {
    static const bool value = __is_polymorphic(T);
};

#define EnsurePtrConvertibleArgDecl(From, To) \
    typename WTF::EnableIf<WTF::IsPointerConvertible<From, To>::Value, bool>::Type = true
#define EnsurePtrConvertibleArgDefn(From, To) \
    typename WTF::EnableIf<WTF::IsPointerConvertible<From, To>::Value, bool>::Type

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
    template <typename V> static YesType checkHasTraceMethod(V* v, blink::Visitor* p = nullptr, typename EnableIf<IsSameType<decltype(v->trace(p)), void>::value>::Type* g = nullptr);
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
