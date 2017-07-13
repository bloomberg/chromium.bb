// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WrapperVisitor_h
#define WrapperVisitor_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"

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

// Declares non-virtual TraceWrappers method. Should be used on
// non-ScriptWrappable classes which should participate in wrapper tracing (e.g.
// StyleEngine):
//
//     class StyleEngine: public TraceWrapperBase {
//      public:
//       DECLARE_TRACE_WRAPPERS();
//     };
//
#define DECLARE_TRACE_WRAPPERS() \
  void TraceWrappers(const WrapperVisitor* visitor) const

// Declares virtual TraceWrappers method. It is used in ScriptWrappable, can be
// used to override the method in the subclasses, and can be used by
// non-ScriptWrappable classes which expect to be inherited.
#define DECLARE_VIRTUAL_TRACE_WRAPPERS() virtual DECLARE_TRACE_WRAPPERS()

// Provides definition of TraceWrappers method. Custom code will usually call
// visitor->TraceWrappers with all objects which could contribute to the set of
// reachable wrappers:
//
//     DEFINE_TRACE_WRAPPERS(NodeRareData)
//     {
//         visitor->TraceWrappers(node_lists_);
//         visitor->TraceWrappers(mutation_observer_data_);
//     }
//
#define DEFINE_TRACE_WRAPPERS(T) \
  void T::TraceWrappers(const WrapperVisitor* visitor) const

#define DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH() \
  void TraceWrappersAfterDispatch(const WrapperVisitor*) const

#define DEFINE_TRACE_WRAPPERS_AFTER_DISPATCH(T) \
  void T::TraceWrappersAfterDispatch(const WrapperVisitor* visitor) const

#define DEFINE_INLINE_TRACE_WRAPPERS() DECLARE_TRACE_WRAPPERS()
#define DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() DECLARE_VIRTUAL_TRACE_WRAPPERS()

#define DEFINE_TRAIT_FOR_TRACE_WRAPPERS(ClassName)           \
  template <>                                                \
  inline void TraceTrait<ClassName>::TraceMarkedWrapper(     \
      const WrapperVisitor* visitor, const void* t) {        \
    const ClassName* traceable = ToWrapperTracingType(t);    \
    DCHECK(GetHeapObjectHeader(t)->IsWrapperHeaderMarked()); \
    traceable->TraceWrappers(visitor);                       \
  }

// ###########################################################################
// TODO(hlopko): Get rid of virtual calls using CRTP
class PLATFORM_EXPORT WrapperVisitor {
  USING_FAST_MALLOC(WrapperVisitor);

 public:
  template <typename T>
  static NOINLINE void MissedWriteBarrier() {
    NOTREACHED();
  }

  // Trace all wrappers of |t|.
  //
  // If you cannot use TraceWrapperMember & the corresponding TraceWrappers()
  // for some reason (e.g., due to sizeof(TraceWrapperMember)), you can use
  // Member and |TraceWrappersWithManualWriteBarrier()|. See below.
  template <typename T>
  void TraceWrappers(const TraceWrapperMember<T>& t) const {
    TraceWrappers(t.Get());
  }

  // Require all users of manual write barriers to make this explicit in their
  // |TraceWrappers| definition. Be sure to add
  // |ScriptWrappableVisitor::writeBarrier(this, new_value)| after all
  // assignments to the field. Otherwise, the objects may be collected
  // prematurely.
  template <typename T>
  void TraceWrappersWithManualWriteBarrier(const Member<T>& t) const {
    TraceWrappers(t.Get());
  }
  template <typename T>
  void TraceWrappersWithManualWriteBarrier(const WeakMember<T>& t) const {
    TraceWrappers(t.Get());
  }
  template <typename T>
  void TraceWrappersWithManualWriteBarrier(const T* traceable) const {
    TraceWrappers(traceable);
  }

  // TODO(mlippautz): Add |template <typename T> TraceWrappers(const
  // TraceWrapperV8Reference<T>&)|.
  virtual void TraceWrappers(
      const TraceWrapperV8Reference<v8::Value>&) const = 0;
  virtual void MarkWrapper(const v8::PersistentBase<v8::Value>*) const = 0;

  virtual void DispatchTraceWrappers(const TraceWrapperBase*) const = 0;

  virtual bool MarkWrapperHeader(HeapObjectHeader*) const = 0;

  virtual void MarkWrappersInAllWorlds(const ScriptWrappable*) const = 0;
  void MarkWrappersInAllWorlds(const TraceWrapperBase*) const {
    // TraceWrapperBase cannot point to V8 and thus doesn't need to
    // mark wrappers.
  }

  template <typename T>
  ALWAYS_INLINE void MarkAndPushToMarkingDeque(const T* traceable) const {
    if (PushToMarkingDeque(TraceTrait<T>::TraceMarkedWrapper,
                           TraceTrait<T>::GetHeapObjectHeader,
                           WrapperVisitor::MissedWriteBarrier<T>, traceable)) {
      TraceTrait<T>::MarkWrapperNoTracing(this, traceable);
    }
  }

 protected:
  template <typename T>
  void TraceWrappers(const T* traceable) const {
    static_assert(sizeof(T), "T must be fully defined");

    if (!traceable) {
      return;
    }

    if (TraceTrait<T>::GetHeapObjectHeader(traceable)
            ->IsWrapperHeaderMarked()) {
      return;
    }

    MarkAndPushToMarkingDeque(traceable);
  }

  // Returns true if pushing to the marking deque was successful.
  virtual bool PushToMarkingDeque(
      void (*trace_wrappers_callback)(const WrapperVisitor*, const void*),
      HeapObjectHeader* (*heap_object_header_callback)(const void*),
      void (*missed_write_barrier_callback)(void),
      const void*) const = 0;
};

}  // namespace blink

#endif  // WrapperVisitor_h
