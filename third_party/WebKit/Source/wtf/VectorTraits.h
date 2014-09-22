/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef WTF_VectorTraits_h
#define WTF_VectorTraits_h

#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/TypeTraits.h"
#include <utility>

using std::pair;

namespace WTF {

    class AtomicString;

    template<typename T>
    struct VectorTraitsBase
    {
        static const bool needsDestruction = !IsTriviallyDestructible<T>::value;
        static const bool canInitializeWithMemset = IsTriviallyDefaultConstructible<T>::value;
        static const bool canMoveWithMemcpy = IsTriviallyMoveAssignable<T>::value;
        static const bool canCopyWithMemcpy = IsTriviallyCopyAssignable<T>::value;
        static const bool canFillWithMemset = IsTriviallyDefaultConstructible<T>::value && (sizeof(T) == sizeof(char));
        static const bool canCompareWithMemcmp = IsScalar<T>::value; // Types without padding.
        template<typename U = void>
        struct NeedsTracingLazily {
            static const bool value = NeedsTracing<T>::value;
        };
        static const WeakHandlingFlag weakHandlingFlag = NoWeakHandlingInCollections; // We don't support weak handling in vectors.
    };

    template<typename T>
    struct VectorTraits : VectorTraitsBase<T> { };

    // Classes marked with SimpleVectorTraits will use memmov, memcpy, memcmp
    // instead of constructors, copy operators, etc for initialization, move
    // and comparison.
    template<typename T>
    struct SimpleClassVectorTraits : VectorTraitsBase<T>
    {
        static const bool canInitializeWithMemset = true;
        static const bool canMoveWithMemcpy = true;
        static const bool canCompareWithMemcmp = true;
    };

    // We know OwnPtr and RefPtr are simple enough that initializing to 0 and
    // moving with memcpy (and then not destructing the original) will totally
    // work.
    template<typename P>
    struct VectorTraits<RefPtr<P> > : SimpleClassVectorTraits<RefPtr<P> > { };

    template<typename P>
    struct VectorTraits<OwnPtr<P> > : SimpleClassVectorTraits<OwnPtr<P> > {
        // OwnPtr -> PassOwnPtr has a very particular structure that
        // tricks the normal type traits into thinking that the class
        // is "trivially copyable".
        static const bool canCopyWithMemcpy = false;
    };
    COMPILE_ASSERT(VectorTraits<RefPtr<int> >::canInitializeWithMemset, inefficientRefPtrVector);
    COMPILE_ASSERT(VectorTraits<RefPtr<int> >::canMoveWithMemcpy, inefficientRefPtrVector);
    COMPILE_ASSERT(VectorTraits<RefPtr<int> >::canCompareWithMemcmp, inefficientRefPtrVector);
    COMPILE_ASSERT(VectorTraits<OwnPtr<int> >::canInitializeWithMemset, inefficientOwnPtrVector);
    COMPILE_ASSERT(VectorTraits<OwnPtr<int> >::canMoveWithMemcpy, inefficientOwnPtrVector);
    COMPILE_ASSERT(VectorTraits<OwnPtr<int> >::canCompareWithMemcmp, inefficientOwnPtrVector);

    template<typename First, typename Second>
    struct VectorTraits<pair<First, Second> >
    {
        typedef VectorTraits<First> FirstTraits;
        typedef VectorTraits<Second> SecondTraits;

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;
        static const bool canInitializeWithMemset = FirstTraits::canInitializeWithMemset && SecondTraits::canInitializeWithMemset;
        static const bool canMoveWithMemcpy = FirstTraits::canMoveWithMemcpy && SecondTraits::canMoveWithMemcpy;
        static const bool canCopyWithMemcpy = FirstTraits::canCopyWithMemcpy && SecondTraits::canCopyWithMemcpy;
        static const bool canFillWithMemset = false;
        static const bool canCompareWithMemcmp = FirstTraits::canCompareWithMemcmp && SecondTraits::canCompareWithMemcmp;
        template <typename U = void>
        struct NeedsTracingLazily {
            static const bool value = ShouldBeTraced<FirstTraits>::value || ShouldBeTraced<SecondTraits>::value;
        };
        static const WeakHandlingFlag weakHandlingFlag = NoWeakHandlingInCollections; // We don't support weak handling in vectors.
    };

} // namespace WTF

#define WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(ClassName) \
namespace WTF { \
COMPILE_ASSERT(!IsTriviallyDefaultConstructible<ClassName>::value || !IsTriviallyCopyAssignable<ClassName>::value, macro_not_needed); \
    template<> \
    struct VectorTraits<ClassName> : SimpleClassVectorTraits<ClassName> { }; \
}

#define WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(ClassName) \
namespace WTF { \
COMPILE_ASSERT(!WTF::IsTriviallyDefaultConstructible<ClassName>::value || !IsTriviallyCopyAssignable<ClassName>::value, macro_not_needed); \
    template<> \
    struct VectorTraits<ClassName> : VectorTraitsBase<ClassName> \
    { \
        static const bool canInitializeWithMemset = true; \
        static const bool canMoveWithMemcpy = true; \
    }; \
}

#define WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(ClassName) \
namespace WTF { \
COMPILE_ASSERT(!WTF::IsTriviallyDefaultConstructible<ClassName>::value, macro_not_needed); \
    template<> \
    struct VectorTraits<ClassName> : VectorTraitsBase<ClassName> \
    { \
        static const bool canInitializeWithMemset = true; \
    }; \
}

using WTF::VectorTraits;
using WTF::SimpleClassVectorTraits;

#endif // WTF_VectorTraits_h
