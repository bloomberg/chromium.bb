// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Weak handles provides a way to refer to weak pointers from another
// thread.  This is useful because it is not safe to reference a weak
// pointer from a thread other than the thread on which it was
// created.
//
// Weak handles can be passed across threads, so for example, you can
// use them to do the "real" work on one thread and get notified on
// another thread:
//
// class FooIOWorker {
//  public:
//   FooIOWorker(const WeakHandle<Foo>& foo) : foo_(foo) {}
//
//   void OnIOStart() {
//     foo_.Call(FROM_HERE, &Foo::OnIOStart);
//   }
//
//   void OnIOEvent(IOEvent e) {
//     foo_.Call(FROM_HERE, &Foo::OnIOEvent, e);
//   }
//
//   void OnIOError(IOError err) {
//     foo_.Call(FROM_HERE, &Foo::OnIOError, err);
//   }
//
//  private:
//   const WeakHandle<Foo> foo_;
// };
//
// class Foo : public SupportsWeakPtr<Foo>, public NonThreadSafe {
//  public:
//   Foo() {
//     SpawnFooIOWorkerOnIOThread(base::MakeWeakHandle(AsWeakPtr()));
//   }
//
//   /* Will always be called on the correct thread, and only if this
//      object hasn't been destroyed. */
//   void OnIOStart() { DCHECK(CalledOnValidThread(); ... }
//   void OnIOEvent(IOEvent e) { DCHECK(CalledOnValidThread(); ... }
//   void OnIOError(IOError err) { DCHECK(CalledOnValidThread(); ... }
// };

#ifndef SYNC_UTIL_WEAK_HANDLE_H_
#define SYNC_UTIL_WEAK_HANDLE_H_

#include <cstddef>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "sync/base/sync_export.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

template <typename T> class WeakHandle;

namespace internal {
// These classes are part of the WeakHandle implementation.  DO NOT
// USE THESE CLASSES DIRECTLY YOURSELF.

// Adapted from base/callback_internal.h.

template <typename T>
struct ParamTraits {
  typedef const T& ForwardType;
};

template <typename T>
struct ParamTraits<T&> {
  typedef T& ForwardType;
};

template <typename T, size_t n>
struct ParamTraits<T[n]> {
  typedef const T* ForwardType;
};

template <typename T>
struct ParamTraits<T[]> {
  typedef const T* ForwardType;
};

// Base class for WeakHandleCore<T> to avoid template bloat.  Handles
// the interaction with the owner thread and its message loop.
class SYNC_EXPORT WeakHandleCoreBase {
 public:
  // Assumes the current thread is the owner thread.
  WeakHandleCoreBase();

  // May be called on any thread.
  bool IsOnOwnerThread() const;

 protected:
  // May be destroyed on any thread.
  ~WeakHandleCoreBase();

  // May be called on any thread.
  void PostToOwnerThread(const tracked_objects::Location& from_here,
                         const base::Closure& fn) const;

 private:
  // May be used on any thread.
  const scoped_refptr<base::MessageLoopProxy> owner_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(WeakHandleCoreBase);
};

// WeakHandleCore<T> contains all the logic for WeakHandle<T>.
template <typename T>
class WeakHandleCore
    : public WeakHandleCoreBase,
      public base::RefCountedThreadSafe<WeakHandleCore<T> > {
 public:
  // Must be called on |ptr|'s owner thread, which is assumed to be
  // the current thread.
  explicit WeakHandleCore(const base::WeakPtr<T>& ptr) : ptr_(ptr) {}

  // Must be called on |ptr_|'s owner thread.
  base::WeakPtr<T> Get() const {
    CHECK(IsOnOwnerThread());
    return ptr_;
  }

  // Call(...) may be called on any thread, but all its arguments
  // should be safe to be bound and copied across threads.

  template <typename U>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(void)) const {
    PostToOwnerThread(
        from_here,
        Bind(&WeakHandleCore::template DoCall0<U>, this, fn));
  }

  template <typename U, typename A1>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1),
            typename ParamTraits<A1>::ForwardType a1) const {
    PostToOwnerThread(
        from_here,
        Bind(&WeakHandleCore::template DoCall1<U, A1>,
             this, fn, a1));
  }

  template <typename U, typename A1, typename A2>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1, A2),
            typename ParamTraits<A1>::ForwardType a1,
            typename ParamTraits<A2>::ForwardType a2) const {
    PostToOwnerThread(
        from_here,
        Bind(&WeakHandleCore::template DoCall2<U, A1, A2>,
             this, fn, a1, a2));
  }

  template <typename U, typename A1, typename A2, typename A3>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1, A2, A3),
            typename ParamTraits<A1>::ForwardType a1,
            typename ParamTraits<A2>::ForwardType a2,
            typename ParamTraits<A3>::ForwardType a3) const {
    PostToOwnerThread(
        from_here,
        Bind(&WeakHandleCore::template DoCall3<U, A1, A2, A3>,
             this, fn, a1, a2, a3));
  }

  template <typename U, typename A1, typename A2, typename A3, typename A4>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1, A2, A3, A4),
            typename ParamTraits<A1>::ForwardType a1,
            typename ParamTraits<A2>::ForwardType a2,
            typename ParamTraits<A3>::ForwardType a3,
            typename ParamTraits<A4>::ForwardType a4) const {
    PostToOwnerThread(
        from_here,
        Bind(&WeakHandleCore::template DoCall4<U, A1, A2, A3, A4>,
             this, fn, a1, a2, a3, a4));
  }

 private:
  friend class base::RefCountedThreadSafe<WeakHandleCore<T> >;

  // May be destroyed on any thread.
  ~WeakHandleCore() {}

  // GCC 4.2.1 on OS X gets confused if all the DoCall functions are
  // named the same, so we distinguish them.

  template <typename U>
  void DoCall0(void (U::*fn)(void)) const {
    CHECK(IsOnOwnerThread());
    if (!Get()) {
      return;
    }
    (Get().get()->*fn)();
  }

  template <typename U, typename A1>
  void DoCall1(void (U::*fn)(A1),
               typename ParamTraits<A1>::ForwardType a1) const {
    CHECK(IsOnOwnerThread());
    if (!Get()) {
      return;
    }
    (Get().get()->*fn)(a1);
  }

  template <typename U, typename A1, typename A2>
  void DoCall2(void (U::*fn)(A1, A2),
               typename ParamTraits<A1>::ForwardType a1,
               typename ParamTraits<A2>::ForwardType a2) const {
    CHECK(IsOnOwnerThread());
    if (!Get()) {
      return;
    }
    (Get().get()->*fn)(a1, a2);
  }

  template <typename U, typename A1, typename A2, typename A3>
  void DoCall3(void (U::*fn)(A1, A2, A3),
               typename ParamTraits<A1>::ForwardType a1,
               typename ParamTraits<A2>::ForwardType a2,
               typename ParamTraits<A3>::ForwardType a3) const {
    CHECK(IsOnOwnerThread());
    if (!Get()) {
      return;
    }
    (Get().get()->*fn)(a1, a2, a3);
  }

  template <typename U, typename A1, typename A2, typename A3, typename A4>
  void DoCall4(void (U::*fn)(A1, A2, A3, A4),
               typename ParamTraits<A1>::ForwardType a1,
               typename ParamTraits<A2>::ForwardType a2,
               typename ParamTraits<A3>::ForwardType a3,
               typename ParamTraits<A4>::ForwardType a4) const {
    CHECK(IsOnOwnerThread());
    if (!Get()) {
      return;
    }
    (Get().get()->*fn)(a1, a2, a3, a4);
  }

  // Must be dereferenced only on the owner thread.  May be destroyed
  // from any thread.
  base::WeakPtr<T> ptr_;

  DISALLOW_COPY_AND_ASSIGN(WeakHandleCore);
};

}  // namespace internal

// May be destroyed on any thread.
// Copying and assignment are welcome.
template <typename T>
class WeakHandle {
 public:
  // Creates an uninitialized WeakHandle.
  WeakHandle() {}

  // Creates an initialized WeakHandle from |ptr|.
  explicit WeakHandle(const base::WeakPtr<T>& ptr)
      : core_(new internal::WeakHandleCore<T>(ptr)) {}

  // Allow conversion from WeakHandle<U> to WeakHandle<T> if U is
  // convertible to T, but we *must* be on |other|'s owner thread.
  // Note that this doesn't override the regular copy constructor, so
  // that one can be called on any thread.
  template <typename U>
  WeakHandle(const WeakHandle<U>& other)  // NOLINT
      : core_(
          other.IsInitialized() ?
          new internal::WeakHandleCore<T>(other.Get()) :
          NULL) {}

  // Returns true iff this WeakHandle is initialized.  Note that being
  // initialized isn't a guarantee that the underlying object is still
  // alive.
  bool IsInitialized() const {
    return core_.get() != NULL;
  }

  // Resets to an uninitialized WeakHandle.
  void Reset() {
    core_ = NULL;
  }

  // Must be called only on the underlying object's owner thread.
  base::WeakPtr<T> Get() const {
    CHECK(IsInitialized());
    CHECK(core_->IsOnOwnerThread());
    return core_->Get();
  }

  // Call(...) may be called on any thread, but all its arguments
  // should be safe to be bound and copied across threads.

  template <typename U>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(void)) const {
    CHECK(IsInitialized());
    core_->Call(from_here, fn);
  }

  template <typename U, typename A1>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1),
            typename internal::ParamTraits<A1>::ForwardType a1) const {
    CHECK(IsInitialized());
    core_->Call(from_here, fn, a1);
  }

  template <typename U, typename A1, typename A2>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1, A2),
            typename internal::ParamTraits<A1>::ForwardType a1,
            typename internal::ParamTraits<A2>::ForwardType a2) const {
    CHECK(IsInitialized());
    core_->Call(from_here, fn, a1, a2);
  }

  template <typename U, typename A1, typename A2, typename A3>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1, A2, A3),
            typename internal::ParamTraits<A1>::ForwardType a1,
            typename internal::ParamTraits<A2>::ForwardType a2,
            typename internal::ParamTraits<A3>::ForwardType a3) const {
    CHECK(IsInitialized());
    core_->Call(from_here, fn, a1, a2, a3);
  }

  template <typename U, typename A1, typename A2, typename A3, typename A4>
  void Call(const tracked_objects::Location& from_here,
            void (U::*fn)(A1, A2, A3, A4),
            typename internal::ParamTraits<A1>::ForwardType a1,
            typename internal::ParamTraits<A2>::ForwardType a2,
            typename internal::ParamTraits<A3>::ForwardType a3,
            typename internal::ParamTraits<A4>::ForwardType a4) const {
    CHECK(IsInitialized());
    core_->Call(from_here, fn, a1, a2, a3, a4);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WeakHandleTest,
                           TypeConversionConstructor);
  FRIEND_TEST_ALL_PREFIXES(WeakHandleTest,
                           TypeConversionConstructorAssignment);

  scoped_refptr<internal::WeakHandleCore<T> > core_;
};

// Makes a WeakHandle from a WeakPtr.
template <typename T>
WeakHandle<T> MakeWeakHandle(const base::WeakPtr<T>& ptr) {
  return WeakHandle<T>(ptr);
}

}  // namespace syncer

#endif  // SYNC_UTIL_WEAK_HANDLE_H_
