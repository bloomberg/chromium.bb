/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_Functional_h
#define WTF_Functional_h

#include "base/bind.h"
#include "base/threading/thread_checker.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/TypeTraits.h"
#include <utility>

namespace blink {
template <typename T>
class Member;
template <typename T>
class WeakMember;
}

namespace WTF {

// Functional.h provides a very simple way to bind a function pointer and
// arguments together into a function object that can be stored, copied and
// invoked, similar to boost::bind and std::bind in C++11.

// Thread Safety:
//
// WTF::Bind() and WTF::Closure should be used for same-thread closures
// only, i.e. the closures must be created, executed and destructed on
// the same thread.
// Use crossThreadBind() and CrossThreadClosure if the function/task is called
// or destructed on a (potentially) different thread from the current thread.

// WTF::bind() and move semantics
// ==============================
//
// For unbound parameters, there are two ways to pass movable arguments:
//
//     1) Pass by rvalue reference.
//
//            void YourFunction(Argument&& argument) { ... }
//            Function<void(Argument&&)> functor = Bind(&YourFunction);
//
//     2) Pass by value.
//
//            void YourFunction(Argument argument) { ... }
//            Function<void(Argument)> functor = Bind(YourFunction);
//
// Note that with the latter there will be *two* move constructions happening,
// because there needs to be at least one intermediary function call taking an
// argument of type "Argument" (i.e. passed by value). The former case does not
// require any move constructions inbetween.
//
// For bound parameters (arguments supplied on the creation of a functor), you
// can move your argument into the internal storage of the functor by supplying
// an rvalue to that argument (this is done in wrap() of ParamStorageTraits).
// However, to make the functor be able to get called multiple times, the
// stored object does not get moved out automatically when the underlying
// function is actually invoked. If you want to make an argument "auto-passed",
// you can do so by wrapping your bound argument with WTF::Passed() function, as
// shown below:
//
//     void YourFunction(Argument argument)
//     {
//         // |argument| is passed from the internal storage of functor.
//         ...
//     }
//
//     ...
//     Function<void()> functor = Bind(&YourFunction, WTF::Passed(Argument()));
//     ...
//     functor();
//
// The underlying function must receive the argument wrapped by WTF::Passed() by
// rvalue reference or by value.
//
// Obviously, if you create a functor this way, you shouldn't call the functor
// twice or more; after the second call, the passed argument may be invalid.

enum FunctionThreadAffinity { kCrossThreadAffinity, kSameThreadAffinity };

template <typename T>
class PassedWrapper final {
 public:
  explicit PassedWrapper(T&& scoper) : scoper_(std::move(scoper)) {}
  PassedWrapper(PassedWrapper&& other) : scoper_(std::move(other.scoper_)) {}
  T MoveOut() const { return std::move(scoper_); }

 private:
  mutable T scoper_;
};

template <typename T>
PassedWrapper<T> Passed(T&& value) {
  static_assert(
      !std::is_reference<T>::value,
      "You must pass an rvalue to WTF::passed() so it can be moved. Add "
      "std::move() if necessary.");
  static_assert(!std::is_const<T>::value,
                "|value| must not be const so it can be moved.");
  return PassedWrapper<T>(std::move(value));
}

template <typename T, FunctionThreadAffinity threadAffinity>
class UnretainedWrapper final {
 public:
  explicit UnretainedWrapper(T* ptr) : ptr_(ptr) {}
  T* Value() const { return ptr_; }

 private:
  T* ptr_;
};

template <typename T>
UnretainedWrapper<T, kSameThreadAffinity> Unretained(T* value) {
  static_assert(!WTF::IsGarbageCollectedType<T>::value,
                "WTF::Unretained() + GCed type is forbidden");
  return UnretainedWrapper<T, kSameThreadAffinity>(value);
}

template <typename T>
UnretainedWrapper<T, kCrossThreadAffinity> CrossThreadUnretained(T* value) {
  static_assert(!WTF::IsGarbageCollectedType<T>::value,
                "CrossThreadUnretained() + GCed type is forbidden");
  return UnretainedWrapper<T, kCrossThreadAffinity>(value);
}

template <typename T>
struct ParamStorageTraits {
  typedef T StorageType;

  static_assert(!std::is_pointer<T>::value,
                "Raw pointers are not allowed to bind into WTF::Function. Wrap "
                "it with either WrapPersistent, WrapWeakPersistent, "
                "WrapCrossThreadPersistent, WrapCrossThreadWeakPersistent, "
                "RefPtr or unretained.");
  static_assert(!IsSubclassOfTemplate<T, blink::Member>::value &&
                    !IsSubclassOfTemplate<T, blink::WeakMember>::value,
                "Member and WeakMember are not allowed to bind into "
                "WTF::Function. Wrap it with either WrapPersistent, "
                "WrapWeakPersistent, WrapCrossThreadPersistent or "
                "WrapCrossThreadWeakPersistent.");
  static_assert(!WTF::IsGarbageCollectedType<T>::value,
                "GCed type is forbidden as a bound parameters.");
};

template <typename T>
struct ParamStorageTraits<PassRefPtr<T>> {
  typedef RefPtr<T> StorageType;
};

template <typename T>
struct ParamStorageTraits<RefPtr<T>> {
  typedef RefPtr<T> StorageType;
};

template <typename>
class RetainPtr;

template <typename T>
struct ParamStorageTraits<RetainPtr<T>> {
  typedef RetainPtr<T> StorageType;
};

template <typename T>
struct ParamStorageTraits<PassedWrapper<T>> {
  typedef PassedWrapper<T> StorageType;
};

template <typename T, FunctionThreadAffinity threadAffinity>
struct ParamStorageTraits<UnretainedWrapper<T, threadAffinity>> {
  typedef UnretainedWrapper<T, threadAffinity> StorageType;
};

template <typename Signature,
          FunctionThreadAffinity threadAffinity = kSameThreadAffinity>
class Function;

template <typename R, typename... Args, FunctionThreadAffinity threadAffinity>
class Function<R(Args...), threadAffinity> {
  USING_FAST_MALLOC(Function);

 public:
  Function() {}
  explicit Function(base::Callback<R(Args...)> callback)
      : callback_(std::move(callback)) {}
  ~Function() { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

  Function(const Function&) = delete;
  Function& operator=(const Function&) = delete;

  Function(Function&& other) : callback_(std::move(other.callback_)) {
    DCHECK_CALLED_ON_VALID_THREAD(other.thread_checker_);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DETACH_FROM_THREAD(other.thread_checker_);
  }

  Function& operator=(Function&& other) {
    DCHECK_CALLED_ON_VALID_THREAD(other.thread_checker_);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DETACH_FROM_THREAD(other.thread_checker_);
    callback_ = std::move(other.callback_);
    return *this;
  }

  R operator()(Args... args) const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return callback_.Run(std::forward<Args>(args)...);
  }

  bool IsCancelled() const { return callback_.IsCancelled(); }
  explicit operator bool() const { return static_cast<bool>(callback_); }

  friend base::Callback<R(Args...)> ConvertToBaseCallback(Function function) {
    return std::move(function.callback_);
  }

 private:
  using MaybeThreadChecker =
      typename std::conditional<threadAffinity == kSameThreadAffinity,
                                base::ThreadChecker,
                                base::ThreadCheckerDoNothing>::type;
#if DCHECK_IS_ON()
  MaybeThreadChecker thread_checker_;
#endif
  base::Callback<R(Args...)> callback_;
};

template <FunctionThreadAffinity threadAffinity,
          typename FunctionType,
          typename... BoundParameters>
Function<base::MakeUnboundRunType<FunctionType, BoundParameters...>,
         threadAffinity>
BindInternal(FunctionType function, BoundParameters&&... bound_parameters) {
  using UnboundRunType =
      base::MakeUnboundRunType<FunctionType, BoundParameters...>;
  return Function<UnboundRunType, threadAffinity>(base::Bind(
      function,
      typename ParamStorageTraits<typename std::decay<BoundParameters>::type>::
          StorageType(std::forward<BoundParameters>(bound_parameters))...));
}

template <typename FunctionType, typename... BoundParameters>
Function<base::MakeUnboundRunType<FunctionType, BoundParameters...>,
         kSameThreadAffinity>
Bind(FunctionType function, BoundParameters&&... bound_parameters) {
  return BindInternal<kSameThreadAffinity>(
      function, std::forward<BoundParameters>(bound_parameters)...);
}

typedef Function<void(), kSameThreadAffinity> Closure;
typedef Function<void(), kCrossThreadAffinity> CrossThreadClosure;

}  // namespace WTF

namespace base {

template <typename T>
struct BindUnwrapTraits<WTF::RefPtr<T>> {
  static T* Unwrap(const WTF::RefPtr<T>& wrapped) { return wrapped.Get(); }
};

template <typename T>
struct BindUnwrapTraits<WTF::PassedWrapper<T>> {
  static T Unwrap(const WTF::PassedWrapper<T>& wrapped) {
    return wrapped.MoveOut();
  }
};

template <typename T, WTF::FunctionThreadAffinity threadAffinity>
struct BindUnwrapTraits<WTF::UnretainedWrapper<T, threadAffinity>> {
  static T* Unwrap(const WTF::UnretainedWrapper<T, threadAffinity>& wrapped) {
    return wrapped.Value();
  }
};

}  // namespace base

using WTF::CrossThreadUnretained;

using WTF::Function;
using WTF::CrossThreadClosure;

#endif  // WTF_Functional_h
