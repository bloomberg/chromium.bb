/*
 * Copyright (C) 2012 Google, Inc. All Rights Reserved.
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

#ifndef Supplementable_h
#define Supplementable_h

#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"

#if DCHECK_IS_ON()
#include "platform/wtf/Threading.h"
#endif

namespace blink {

// What you should know about Supplementable and Supplement
// ========================================================
// Supplementable and Supplement instances are meant to be thread local. They
// should only be accessed from within the thread that created them. The
// 2 classes are not designed for safe access from another thread. Violating
// this design assumption can result in memory corruption and unpredictable
// behavior.
//
// What you should know about the Supplement keys
// ==============================================
// The Supplement is expected to use the same const char* string instance
// as its key. The Supplementable's SupplementMap will use the address of the
// string as the key and not the characters themselves. Hence, 2 strings with
// the same characters will be treated as 2 different keys.
//
// In practice, it is recommended that Supplements implements a static method
// for returning its key to use. For example:
//
//     class MyClass : public Supplement<MySupplementable> {
//         ...
//         static const char* supplementName();
//     }
//
//     const char* MyClass::supplementName()
//     {
//         return "MyClass";
//     }
//
// An example of the using the key:
//
//     MyClass* MyClass::from(MySupplementable* host)
//     {
//         return static_cast<MyClass*>(
//             Supplement<MySupplementable>::from(host, supplementName()));
//     }
//
// What you should know about thread checks
// ========================================
// When assertion is enabled this class performs thread-safety check so that
// supplements are provided to and from the same thread.
// If you want to provide some value for Workers, this thread check may be too
// strict, since in you'll be providing the value while worker preparation is
// being done on the main thread, even before the worker thread has started.
// If that's the case you can explicitly call reattachThread() when the
// Supplementable object is passed to the final destination thread (i.e.
// worker thread). This will allow supplements to be accessed on that thread.
// Please be extremely careful to use the method though, as randomly calling
// the method could easily cause racy condition.
//
// Note that reattachThread() does nothing if assertion is not enabled.

template <typename T>
class Supplementable;

// Supplement<T>-specific version of TraceWrapperBase class. In order to support
// wrapper-tracing from Supplementable<T> to Supplement<T> (especially when
// crossing core/modules boundary), Supplement<T> needs to be wrapper-traceable.
// This class provides a common API for all subclasses of Supplement<T> to
// support wrapper-tracing.
class PLATFORM_EXPORT TraceWrapperBaseForSupplement {
 public:
  virtual void TraceWrappers(const ScriptWrappableVisitor* visitor) const {}
};

template <typename T>
class Supplement : public GarbageCollectedMixin,
                   public TraceWrapperBaseForSupplement {
 public:
  // TODO(haraken): Remove the default constructor.
  // All Supplement objects should be instantiated with |supplementable_|.
  Supplement() {}

  explicit Supplement(T& supplementable) : supplementable_(&supplementable) {}

  // Supplementable and its supplements live and die together.
  // Thus supplementable() should never return null (if the default constructor
  // is completely removed).
  T* GetSupplementable() const { return supplementable_; }

  static void ProvideTo(Supplementable<T>& supplementable,
                        const char* key,
                        Supplement<T>* supplement) {
    supplementable.ProvideSupplement(key, supplement);
  }

  static Supplement<T>* From(const Supplementable<T>& supplementable,
                             const char* key) {
    return supplementable.RequireSupplement(key);
  }

  static Supplement<T>* From(const Supplementable<T>* supplementable,
                             const char* key) {
    return supplementable ? supplementable->RequireSupplement(key) : nullptr;
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(supplementable_);
  }

 private:
  Member<T> supplementable_;
};

template <typename T>
class Supplementable : public GarbageCollectedMixin {
  WTF_MAKE_NONCOPYABLE(Supplementable);

 public:
  void ProvideSupplement(const char* key, Supplement<T>* supplement) {
#if DCHECK_IS_ON()
    DCHECK_EQ(creation_thread_id_, CurrentThread());
#endif
    this->supplements_.Set(key, supplement);
  }

  void RemoveSupplement(const char* key) {
#if DCHECK_IS_ON()
    DCHECK_EQ(creation_thread_id_, CurrentThread());
#endif
    this->supplements_.erase(key);
  }

  Supplement<T>* RequireSupplement(const char* key) const {
#if DCHECK_IS_ON()
    DCHECK_EQ(attached_thread_id_, CurrentThread());
#endif
    return this->supplements_.at(key);
  }

  void ReattachThread() {
#if DCHECK_IS_ON()
    attached_thread_id_ = CurrentThread();
#endif
  }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(supplements_); }
  virtual void TraceWrappers(const ScriptWrappableVisitor* visitor) const {
    for (const auto& supplement : supplements_.Values())
      visitor->TraceWrappers(supplement);
  }

 protected:
  using SupplementMap = HeapHashMap<const char*,
                                    TraceWrapperMember<Supplement<T>>,
                                    PtrHash<const char>>;
  SupplementMap supplements_;

  Supplementable()
#if DCHECK_IS_ON()
      : attached_thread_id_(CurrentThread()),
        creation_thread_id_(CurrentThread())
#endif
  {
  }

#if DCHECK_IS_ON()
 private:
  ThreadIdentifier attached_thread_id_;
  ThreadIdentifier creation_thread_id_;
#endif
};

template <typename T>
struct ThreadingTrait<Supplement<T>> {
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
struct ThreadingTrait<Supplementable<T>> {
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::Affinity;
};

}  // namespace blink

#endif  // Supplementable_h
