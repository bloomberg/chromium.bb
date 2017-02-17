/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_StdLibExtras_h
#define WTF_StdLibExtras_h

#include <cstddef>
#include "base/numerics/safe_conversions.h"
#include "wtf/Assertions.h"
#include "wtf/CPU.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/Noncopyable.h"
#include "wtf/TypeTraits.h"

#if DCHECK_IS_ON()
#include "wtf/Threading.h"
#endif

// Use |DEFINE_STATIC_LOCAL()| to declare and define a static local variable
// (|static T;|) so that it is leaked and its destructors are not called at
// exit. T may also be a Blink garbage collected object, in which case it is
// wrapped up by an off-heap |Persistent<T>| reference to the object, keeping
// it alive across GCs.
//
// A |DEFINE_STATIC_LOCAL()| static should only be used on the thread it was
// created on.
//
#define DEFINE_STATIC_LOCAL(Type, Name, Arguments)            \
  static WTF::StaticSingleton<Type> s_##Name(                 \
      new WTF::StaticSingleton<Type>::WrapperType Arguments); \
  Type& Name = s_##Name.get(false)

// |DEFINE_THREAD_SAFE_STATIC_LOCAL()| is the cross-thread accessible variant
// of |DEFINE_STATIC_LOCAL()|; use it if the singleton can be accessed by
// multiple threads.
//
// TODO: rename as DEFINE_CROSS_THREAD_STATIC_LOCAL() ?
#define DEFINE_THREAD_SAFE_STATIC_LOCAL(Type, Name, Initializer) \
  static WTF::StaticSingleton<Type> s_##Name(Initializer);       \
  Type& Name = s_##Name.get(true)

namespace blink {
template <typename T>
class Persistent;

}  // namespace blink

namespace WTF {

template <typename Type>
class StaticSingleton final {
  WTF_MAKE_NONCOPYABLE(StaticSingleton);

 public:
  template <typename T,
            bool = WTF::IsGarbageCollectedType<T>::value &&
                   !WTF::IsPersistentReferenceType<T>::value>
  struct Wrapper {
    using type = T;

    static T& unwrap(T* singleton) { return *singleton; }
  };

  template <typename T>
  struct Wrapper<T, true> {
    using type = blink::Persistent<T>;

    static T& unwrap(blink::Persistent<T>* singleton) {
      DCHECK(singleton);
      // If this assert triggers, you're supplying an empty ("()") 'Arguments'
      // argument to DEFINE_STATIC_LOCAL() - it must be the heap object you wish
      // to create as a static singleton and wrapped up with a Persistent
      // reference.
      DCHECK(*singleton);
      return **singleton;
    }
  };

  using WrapperType = typename Wrapper<Type>::type;

  // To cooperate with leak detection(LSan) for Blink garbage collected objects,
  // the objects owned by persistent local statics will in some cases have to be
  // finalized prior to leak checking. This only applies to static references to
  // Blink heap objects and what they transitively hold on to. Hence the
  // LEAK_SANITIZER_REGISTER_STATIC_LOCAL() use, it taking care of the grungy
  // details.

  explicit StaticSingleton(WrapperType* instance)
      : m_instance(LEAK_SANITIZER_REGISTER_STATIC_LOCAL(WrapperType, instance))
#if DCHECK_IS_ON()
        ,
        m_safelyInitialized(WTF::isBeforeThreadCreated()),
        m_thread(WTF::internal::currentThreadSyscall())
#endif
  {
  }

  Type& get(bool allowCrossThreadUse) const {
#if DCHECK_IS_ON()
    DCHECK(isNotRacy(allowCrossThreadUse));
#endif
    ALLOW_UNUSED_LOCAL(allowCrossThreadUse);
    return Wrapper<Type>::unwrap(m_instance);
  }

  operator Type&() { return get(); }

 private:
#if DCHECK_IS_ON()

  bool isNotRacy(bool allowCrossThreadUse) const {
    // Make sure that singleton is safely initialized, or
    // keeps being called on the same thread if cross-thread
    // use is not permitted.
    return allowCrossThreadUse || m_safelyInitialized ||
           m_thread == WTF::internal::currentThreadSyscall();
  }
#endif

  WrapperType* m_instance;
#if DCHECK_IS_ON()
  bool m_safelyInitialized;
  ThreadIdentifier m_thread;
#endif
};

}  // namespace WTF

// Use this to declare and define a static local pointer to a ref-counted object
// so that it is leaked so that the object's destructors are not called at
// exit.  This macro should be used with ref-counted objects rather than
// DEFINE_STATIC_LOCAL macro, as this macro does not lead to an extra memory
// allocation.
#define DEFINE_STATIC_REF(type, name, arguments) \
  static type* name = PassRefPtr<type>(arguments).leakRef();

/*
 * The reinterpret_cast<Type1*>([pointer to Type2]) expressions - where
 * sizeof(Type1) > sizeof(Type2) - cause the following warning on ARM with GCC:
 * increases required alignment of target type.
 *
 * An implicit or an extra static_cast<void*> bypasses the warning.
 * For more info see the following bugzilla entries:
 * - https://bugs.webkit.org/show_bug.cgi?id=38045
 * - http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43976
 */
#if CPU(ARM) && COMPILER(GCC)
template <typename Type>
bool isPointerTypeAlignmentOkay(Type* ptr) {
  return !(reinterpret_cast<intptr_t>(ptr) % __alignof__(Type));
}

template <typename TypePtr>
TypePtr reinterpret_cast_ptr(void* ptr) {
  DCHECK(isPointerTypeAlignmentOkay(reinterpret_cast<TypePtr>(ptr)));
  return reinterpret_cast<TypePtr>(ptr);
}

template <typename TypePtr>
TypePtr reinterpret_cast_ptr(const void* ptr) {
  DCHECK(isPointerTypeAlignmentOkay(reinterpret_cast<TypePtr>(ptr)));
  return reinterpret_cast<TypePtr>(ptr);
}
#else
template <typename Type>
bool isPointerTypeAlignmentOkay(Type*) {
  return true;
}
#define reinterpret_cast_ptr reinterpret_cast
#endif

namespace WTF {

/*
 * C++'s idea of a reinterpret_cast lacks sufficient cojones.
 */
template <typename TO, typename FROM>
inline TO bitwiseCast(FROM from) {
  static_assert(sizeof(TO) == sizeof(FROM),
                "WTF::bitwiseCast sizeof casted types should be equal");
  union {
    FROM from;
    TO to;
  } u;
  u.from = from;
  return u.to;
}

template <typename To, typename From>
inline To safeCast(From value) {
  return base::checked_cast<To>(value);
}

// Use the following macros to prevent errors caused by accidental
// implicit casting of function arguments.  For example, this can
// be used to prevent overflows from non-promoting conversions.
//
// Example:
//
// HAS_STRICTLY_TYPED_ARG
// void sendData(void* data, STRICTLY_TYPED_ARG(size))
// {
//    ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(size_t);
//    ...
// }
//
// The previous example will prevent callers from passing, for example, an
// 'int'. On a 32-bit build, it will prevent use of an 'unsigned long long'.
#define HAS_STRICTLY_TYPED_ARG template <typename ActualArgType>
#define STRICTLY_TYPED_ARG(argName) ActualArgType argName
#define STRICT_ARG_TYPE(ExpectedArgType)                                     \
  static_assert(std::is_same<ActualArgType, ExpectedArgType>::value,         \
                "Strictly typed argument must be of type '" #ExpectedArgType \
                "'.")
#define ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(ExpectedArgType)              \
  static_assert(                                                            \
      std::numeric_limits<ExpectedArgType>::is_integer ==                   \
          std::numeric_limits<ActualArgType>::is_integer,                   \
      "Conversion between integer and non-integer types not allowed.");     \
  static_assert(sizeof(ExpectedArgType) >= sizeof(ActualArgType),           \
                "Truncating conversions not allowed.");                     \
  static_assert(!std::numeric_limits<ActualArgType>::is_signed ||           \
                    std::numeric_limits<ExpectedArgType>::is_signed,        \
                "Signed to unsigned conversion not allowed.");              \
  static_assert((sizeof(ExpectedArgType) != sizeof(ActualArgType)) ||       \
                    (std::numeric_limits<ActualArgType>::is_signed ==       \
                     std::numeric_limits<ExpectedArgType>::is_signed),      \
                "Unsigned to signed conversion not allowed for types with " \
                "identical size (could overflow).");

// Macro that returns a compile time constant with the length of an array, but
// gives an error if passed a non-array.
template <typename T, size_t Size>
char (&ArrayLengthHelperFunction(T (&)[Size]))[Size];
// GCC needs some help to deduce a 0 length array.
#if COMPILER(GCC)
template <typename T>
char (&ArrayLengthHelperFunction(T (&)[0]))[0];
#endif
#define WTF_ARRAY_LENGTH(array) sizeof(::WTF::ArrayLengthHelperFunction(array))

}  // namespace WTF

// This version of placement new omits a 0 check.
enum NotNullTag { NotNull };
inline void* operator new(size_t, NotNullTag, void* location) {
  DCHECK(location);
  return location;
}

using WTF::bitwiseCast;
using WTF::safeCast;

#endif  // WTF_StdLibExtras_h
