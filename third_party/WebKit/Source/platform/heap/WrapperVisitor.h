// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WrapperVisitor_h
#define WrapperVisitor_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"

namespace v8 {
class Value;
template <class T>
class PersistentBase;
}

namespace blink {

template <typename T>
class TraceTrait;
template <typename T>
class Member;
class ScriptWrappable;
template <typename T>
class TraceWrapperV8Reference;
class TraceWrapperBase;
template <typename T>
class TraceWrapperMember;

/**
 * Declares non-virtual traceWrappers method. Should be used on
 * non-ScriptWrappable classes which should participate in wrapper tracing (e.g.
 * StyleEngine):
 *
 *     class StyleEngine: public TraceWrapperBase {
 *      public:
 *       DECLARE_TRACE_WRAPPERS();
 *     };
 */
#define DECLARE_TRACE_WRAPPERS() \
  void traceWrappers(const WrapperVisitor* visitor) const

/**
 * Declares virtual traceWrappers method. It is used in ScriptWrappable, can be
 * used to override the method in the subclasses, and can be used by
 * non-ScriptWrappable classes which expect to be inherited.
 */
#define DECLARE_VIRTUAL_TRACE_WRAPPERS() virtual DECLARE_TRACE_WRAPPERS()

/**
 * Provides definition of traceWrappers method. Custom code will usually call
 * visitor->traceWrappers with all objects which could contribute to the set of
 * reachable wrappers:
 *
 *     DEFINE_TRACE_WRAPPERS(NodeRareData)
 *     {
 *         visitor->traceWrappers(m_nodeLists);
 *         visitor->traceWrappers(m_mutationObserverData);
 *     }
 */
#define DEFINE_TRACE_WRAPPERS(T) \
  void T::traceWrappers(const WrapperVisitor* visitor) const

#define DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH() \
  void traceWrappersAfterDispatch(const WrapperVisitor*) const

#define DEFINE_TRACE_WRAPPERS_AFTER_DISPATCH(T) \
  void T::traceWrappersAfterDispatch(const WrapperVisitor* visitor) const

#define DEFINE_INLINE_TRACE_WRAPPERS() DECLARE_TRACE_WRAPPERS()
#define DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() DECLARE_VIRTUAL_TRACE_WRAPPERS()

#define DEFINE_TRAIT_FOR_TRACE_WRAPPERS(ClassName)        \
  template <>                                             \
  inline void TraceTrait<ClassName>::traceMarkedWrapper(  \
      const WrapperVisitor* visitor, const void* t) {     \
    const ClassName* traceable = ToWrapperTracingType(t); \
    DCHECK(heapObjectHeader(t)->isWrapperHeaderMarked()); \
    traceable->traceWrappers(visitor);                    \
  }

// ###########################################################################
// TODO(hlopko): Get rid of virtual calls using CRTP
class PLATFORM_EXPORT WrapperVisitor {
  USING_FAST_MALLOC(WrapperVisitor);

 public:
  template <typename T>
  static NOINLINE void missedWriteBarrier() {
    NOTREACHED();
  }

  template <typename T>
  void traceWrappers(const T* traceable) const {
    static_assert(sizeof(T), "T must be fully defined");

    if (!traceable) {
      return;
    }

    if (TraceTrait<T>::heapObjectHeader(traceable)->isWrapperHeaderMarked()) {
      return;
    }

    markAndPushToMarkingDeque(traceable);
  }

  /**
   * Trace all wrappers of |t|.
   *
   * If you cannot use TraceWrapperMember & the corresponding traceWrappers()
   * for some reason (e.g., due to sizeof(TraceWrapperMember)), you can use
   * Member and |traceWrappersWithManualWriteBarrier()|. See below.
   */
  template <typename T>
  void traceWrappers(const TraceWrapperMember<T>& t) const {
    traceWrappers(t.get());
  }

  /**
   * Require all users of manual write barriers to make this explicit in their
   * |traceWrappers| definition. Be sure to add
   * |ScriptWrappableVisitor::writeBarrier(this, new_value)| after all
   * assignments to the field. Otherwise, the objects may be collected
   * prematurely.
   */
  template <typename T>
  void traceWrappersWithManualWriteBarrier(const Member<T>& t) const {
    traceWrappers(t.get());
  }
  template <typename T>
  void traceWrappersWithManualWriteBarrier(const WeakMember<T>& t) const {
    traceWrappers(t.get());
  }
  template <typename T>
  void traceWrappersWithManualWriteBarrier(const T* traceable) const {
    traceWrappers(traceable);
  }

  virtual void traceWrappers(
      const TraceWrapperV8Reference<v8::Value>&) const = 0;
  virtual void markWrapper(const v8::PersistentBase<v8::Value>*) const = 0;

  virtual void dispatchTraceWrappers(const TraceWrapperBase*) const = 0;

  virtual bool markWrapperHeader(HeapObjectHeader*) const = 0;

  virtual void markWrappersInAllWorlds(const ScriptWrappable*) const = 0;
  void markWrappersInAllWorlds(const TraceWrapperBase*) const {
    // TraceWrapperBase cannot point to V8 and thus doesn't need to
    // mark wrappers.
  }

  template <typename T>
  ALWAYS_INLINE void markAndPushToMarkingDeque(const T* traceable) const {
    if (pushToMarkingDeque(TraceTrait<T>::traceMarkedWrapper,
                           TraceTrait<T>::heapObjectHeader,
                           WrapperVisitor::missedWriteBarrier<T>, traceable)) {
      TraceTrait<T>::markWrapperNoTracing(this, traceable);
    }
  }

 protected:
  // Returns true if pushing to the marking deque was successful.
  virtual bool pushToMarkingDeque(
      void (*traceWrappersCallback)(const WrapperVisitor*, const void*),
      HeapObjectHeader* (*heapObjectHeaderCallback)(const void*),
      void (*missedWriteBarrierCallback)(void),
      const void*) const = 0;
};

}  // namespace blink

#endif  // WrapperVisitor_h
