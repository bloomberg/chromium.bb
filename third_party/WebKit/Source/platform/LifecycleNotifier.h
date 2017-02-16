/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef LifecycleNotifier_h
#define LifecycleNotifier_h

#include "platform/heap/Handle.h"
#include "wtf/AutoReset.h"
#include "wtf/HashSet.h"

namespace blink {

template <typename T, typename Observer>
class LifecycleNotifier : public virtual GarbageCollectedMixin {
 public:
  virtual ~LifecycleNotifier();

  void addObserver(Observer*);
  void removeObserver(Observer*);

  // notifyContextDestroyed() should be explicitly dispatched from an
  // observed context to detach its observers, and if the observer kind
  // requires it, notify each observer by invoking contextDestroyed().
  //
  // When contextDestroyed() is called, it is supplied the context as
  // an argument, but the observer's lifecycleContext() is still valid
  // and safe to use while handling the notification.
  virtual void notifyContextDestroyed();

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_observers); }

  bool isIteratingOverObservers() const {
    return m_iterationState != NotIterating;
  }

 protected:
  LifecycleNotifier() : m_iterationState(NotIterating) {}

  T* context() { return static_cast<T*>(this); }

  using ObserverSet = HeapHashSet<WeakMember<Observer>>;

  enum IterationState {
    AllowingNone = 0,
    AllowingAddition = 1,
    AllowingRemoval = 2,
    NotIterating = AllowingAddition | AllowingRemoval,
    AllowPendingRemoval = 4,
  };

  // Iteration state is recorded while iterating the observer set,
  // optionally barring add or remove mutations.
  IterationState m_iterationState;
  ObserverSet m_observers;
};

template <typename T, typename Observer>
inline LifecycleNotifier<T, Observer>::~LifecycleNotifier() {
  // FIXME: Enable the following ASSERT. Also see a FIXME in
  // Document::detachLayoutTree().
  // ASSERT(!m_observers.size());
}

// Determine if |contextDestroyed(Observer*) is a public method on
// class type |Observer|, or any of the class types it derives from.
template <typename Observer, typename T>
class HasContextDestroyed {
  using YesType = char;
  using NoType = int;

  template <typename V>
  static YesType checkHasContextDestroyedMethod(
      V* observer,
      T* context = nullptr,
      typename std::enable_if<
          std::is_same<decltype(observer->contextDestroyed(context)),
                       void>::value>::type* g = nullptr);
  template <typename V>
  static NoType checkHasContextDestroyedMethod(...);

 public:
  static_assert(sizeof(Observer), "Observer's class declaration not in scope");
  static const bool value =
      sizeof(YesType) ==
      sizeof(checkHasContextDestroyedMethod<Observer>(nullptr));
};

// If |Observer::contextDestroyed()| is present, invoke it.
template <typename Observer,
          typename T,
          bool = HasContextDestroyed<Observer, T>::value>
class ContextDestroyedNotifier {
  STATIC_ONLY(ContextDestroyedNotifier);

 public:
  static void call(Observer* observer, T* context) {
    observer->contextDestroyed(context);
  }
};

template <typename Observer, typename T>
class ContextDestroyedNotifier<Observer, T, false> {
  STATIC_ONLY(ContextDestroyedNotifier);

 public:
  static void call(Observer*, T*) {}
};

template <typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::notifyContextDestroyed() {
  // Observer unregistration is allowed, but effectively a no-op.
  AutoReset<IterationState> scope(&m_iterationState, AllowingRemoval);
  ObserverSet observers;
  m_observers.swap(observers);
  for (Observer* observer : observers) {
    DCHECK(observer->lifecycleContext() == context());
    ContextDestroyedNotifier<Observer, T>::call(observer, context());
    observer->clearContext();
  }
}

template <typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::addObserver(Observer* observer) {
  RELEASE_ASSERT(m_iterationState & AllowingAddition);
  m_observers.insert(observer);
}

template <typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::removeObserver(Observer* observer) {
  // If immediate removal isn't currently allowed,
  // |observer| is recorded for pending removal.
  if (m_iterationState & AllowPendingRemoval) {
    m_observers.insert(observer);
    return;
  }
  RELEASE_ASSERT(m_iterationState & AllowingRemoval);
  m_observers.erase(observer);
}

}  // namespace blink

#endif  // LifecycleNotifier_h
