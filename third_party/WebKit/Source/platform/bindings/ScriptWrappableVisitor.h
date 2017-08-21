// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "platform/PlatformExport.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/VisitorImpl.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

class HeapObjectHeader;
class ScriptWrappable;
class ScriptWrappableVisitor;
class TraceWrapperBase;
template <typename T>
class TraceWrapperV8Reference;

using HeapObjectHeaderCallback = HeapObjectHeader* (*)(const void*);
using MissedWriteBarrierCallback = void (*)();
using TraceWrappersCallback = void (*)(const ScriptWrappableVisitor*,
                                       const void* self);

#define DECLARE_TRACE_WRAPPERS() \
  void TraceWrappers(const ScriptWrappableVisitor* visitor) const

#define DECLARE_VIRTUAL_TRACE_WRAPPERS() virtual DECLARE_TRACE_WRAPPERS()

#define DEFINE_TRACE_WRAPPERS(T) \
  void T::TraceWrappers(const ScriptWrappableVisitor* visitor) const

#define DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH() \
  void TraceWrappersAfterDispatch(const ScriptWrappableVisitor*) const

#define DEFINE_TRACE_WRAPPERS_AFTER_DISPATCH(T)                             \
  void T::TraceWrappersAfterDispatch(const ScriptWrappableVisitor* visitor) \
      const

#define DEFINE_INLINE_TRACE_WRAPPERS() DECLARE_TRACE_WRAPPERS()
#define DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() DECLARE_VIRTUAL_TRACE_WRAPPERS()

#define DEFINE_TRAIT_FOR_TRACE_WRAPPERS(ClassName)                   \
  template <>                                                        \
  inline void TraceTrait<ClassName>::TraceMarkedWrapper(             \
      const ScriptWrappableVisitor* visitor, const void* t) {        \
    const ClassName* traceable = ToWrapperTracingType(t);            \
    DCHECK(GetHeapObjectHeader(traceable)->IsWrapperHeaderMarked()); \
    traceable->TraceWrappers(visitor);                               \
  }

class WrapperMarkingData {
 public:
  WrapperMarkingData(TraceWrappersCallback trace_wrappers_callback,
                     HeapObjectHeaderCallback heap_object_header_callback,
                     const void* object)
      : trace_wrappers_callback_(trace_wrappers_callback),
        heap_object_header_callback_(heap_object_header_callback),
        raw_object_pointer_(object) {
    DCHECK(trace_wrappers_callback_);
    DCHECK(heap_object_header_callback_);
    DCHECK(raw_object_pointer_);
  }

  // Traces wrappers if the underlying object has not yet been invalidated.
  inline void TraceWrappers(ScriptWrappableVisitor* visitor) {
    if (raw_object_pointer_) {
      trace_wrappers_callback_(visitor, raw_object_pointer_);
    }
  }

  inline const void* RawObjectPointer() { return raw_object_pointer_; }

  // Returns true if the object is currently marked in Oilpan and false
  // otherwise.
  inline bool ShouldBeInvalidated() {
    return raw_object_pointer_ && !GetHeapObjectHeader()->IsMarked();
  }

  // Invalidates the current wrapper marking data, i.e., calling TraceWrappers
  // will result in a noop.
  inline void Invalidate() { raw_object_pointer_ = nullptr; }

 private:
  inline const HeapObjectHeader* GetHeapObjectHeader() {
    DCHECK(raw_object_pointer_);
    return heap_object_header_callback_(raw_object_pointer_);
  }

  TraceWrappersCallback trace_wrappers_callback_;
  HeapObjectHeaderCallback heap_object_header_callback_;
  const void* raw_object_pointer_;
};

// ScriptWrappableVisitor is used to trace through Blink's heap to find all
// reachable wrappers. V8 calls this visitor during its garbage collection,
// see v8::EmbedderHeapTracer.
class PLATFORM_EXPORT ScriptWrappableVisitor : public v8::EmbedderHeapTracer {
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScriptWrappableVisitor);

 public:
  static ScriptWrappableVisitor* CurrentVisitor(v8::Isolate*);

  // Replace all dead objects in the marking deque with nullptr after Oilpan
  // garbage collection.
  static void InvalidateDeadObjectsInMarkingDeque(v8::Isolate*);

  // Immediately clean up all wrappers.
  static void PerformCleanup(v8::Isolate*);

  // Conservative Dijkstra barrier.
  //
  // On assignment 'x.a = y' during incremental marking the Dijkstra barrier
  // suggests checking the color of 'x' and only mark 'y' if 'x' is marked.

  // Since checking 'x' is expensive in the current setting, as it requires
  // either a back pointer or expensive lookup logic due to large objects and
  // multiple inheritance, just assume that 'x' is black. We assume here that
  // since an object 'x' is referenced for a write, it will generally also be
  // alive in the current GC cycle.
  template <typename T>
  static void WriteBarrier(const T* dst_object) {
    if (!dst_object) {
      return;
    }

    const ThreadState* thread_state = ThreadState::Current();
    DCHECK(thread_state);
    // Bail out if tracing is not in progress.
    if (!thread_state->WrapperTracingInProgress())
      return;

    // If the wrapper is already marked we can bail out here.
    if (TraceTrait<T>::GetHeapObjectHeader(dst_object)->IsWrapperHeaderMarked())
      return;

    // Otherwise, eagerly mark the wrapper header and put the object on the
    // marking deque for further processing.
    CurrentVisitor(thread_state->GetIsolate())
        ->MarkAndPushToMarkingDeque(dst_object);
  }

  static void WriteBarrier(v8::Isolate*,
                           const TraceWrapperV8Reference<v8::Value>*);

  // TODO(mlippautz): Remove once ScriptWrappable is converted to
  // TraceWrapperV8Reference.
  static void WriteBarrier(v8::Isolate*, const v8::Persistent<v8::Object>*);

  ScriptWrappableVisitor(v8::Isolate* isolate) : isolate_(isolate){};
  virtual ~ScriptWrappableVisitor();

  // Trace all wrappers of |t|.
  //
  // If you cannot use TraceWrapperMember & the corresponding TraceWrappers()
  // for some reason (e.g., unions using raw pointers), see
  // |TraceWrappersWithManualWriteBarrier()| below.
  template <typename T>
  void TraceWrappers(const TraceWrapperMember<T>& t) const {
    MarkAndTraceWrappers(t.Get());
  }

  // Enable partial tracing of objects. This is used when tracing interior
  // objects without their own header.
  template <typename T>
  void TraceWrappers(const T& traceable) const {
    static_assert(sizeof(T), "T must be fully defined");
    traceable.TraceWrappers(this);
  }

  // Only called from automatically generated bindings code.
  template <typename T>
  void TraceWrappersFromGeneratedCode(const T* traceable) const {
    MarkAndTraceWrappers(traceable);
  }

  // Require all users of manual write barriers to make this explicit in their
  // |TraceWrappers| definition. Be sure to add
  // |ScriptWrappableVisitor::WriteBarrier(new_value)| after all assignments to
  // the field. Otherwise, the objects may be collected prematurely.
  template <typename T>
  void TraceWrappersWithManualWriteBarrier(const T* traceable) const {
    MarkAndTraceWrappers(traceable);
  }

  virtual void DispatchTraceWrappers(const TraceWrapperBase*) const;
  virtual void TraceWrappers(const TraceWrapperV8Reference<v8::Value>&) const;
  virtual void MarkWrapper(const v8::PersistentBase<v8::Value>*) const;
  virtual bool MarkWrapperHeader(HeapObjectHeader*) const;
  // Mark wrappers in all worlds for the given ScriptWrappable as alive in V8.
  virtual void MarkWrappersInAllWorlds(const ScriptWrappable*) const;

  void MarkWrappersInAllWorlds(const TraceWrapperBase*) const {
    // TraceWrapperBase cannot point to V8 and thus doesn't need to
    // mark wrappers.
  }

  // Catch all handlers needed because of mixins.
  void DispatchTraceWrappers(const void*) const { CHECK(false); }

  // Catch all handlers needed because of mixins.
  void MarkWrappersInAllWorlds(const void*) const { CHECK(false); }

  // v8::EmbedderHeapTracer interface.

  void TracePrologue() override;
  void RegisterV8References(const std::vector<std::pair<void*, void*>>&
                                internal_fields_of_potential_wrappers) override;
  void RegisterV8Reference(const std::pair<void*, void*>& internal_fields);
  bool AdvanceTracing(double deadline_in_ms,
                      v8::EmbedderHeapTracer::AdvanceTracingActions) override;
  void TraceEpilogue() override;
  void AbortTracing() override;
  void EnterFinalPause() override;
  size_t NumberOfWrappersToTrace() override;

 protected:
  template <typename T>
  static NOINLINE void MissedWriteBarrier() {
    NOTREACHED();
  }

  template <typename T>
  void MarkAndTraceWrappers(const T* traceable) const {
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

  template <typename T>
  ALWAYS_INLINE void MarkAndPushToMarkingDeque(const T* traceable) const {
    TraceTrait<T>::MarkWrapperNoTracing(this, traceable);
    PushToMarkingDeque(
        TraceTrait<T>::TraceMarkedWrapper, TraceTrait<T>::GetHeapObjectHeader,
        ScriptWrappableVisitor::MissedWriteBarrier<T>, traceable);
  }

  virtual void PushToMarkingDeque(
      TraceWrappersCallback trace_wrappers_callback,
      HeapObjectHeaderCallback heap_object_header_callback,
      MissedWriteBarrierCallback missed_write_barrier_callback,
      const void* object) const {
    DCHECK(tracing_in_progress_);
    DCHECK(heap_object_header_callback(object)->IsWrapperHeaderMarked());
    marking_deque_.push_back(WrapperMarkingData(
        trace_wrappers_callback, heap_object_header_callback, object));
#if DCHECK_IS_ON()
    if (!advancing_tracing_) {
      verifier_deque_.push_back(WrapperMarkingData(
          trace_wrappers_callback, heap_object_header_callback, object));
    }
#endif
  }

  // Schedule an idle task to perform a lazy (incremental) clean up of
  // wrappers.
  void ScheduleIdleLazyCleanup();
  void PerformLazyCleanup(double deadline_seconds);

  void InvalidateDeadObjectsInMarkingDeque();

  // Immediately cleans up all wrappers if necessary.
  void PerformCleanup();

  WTF::Deque<WrapperMarkingData>* MarkingDeque() const {
    return &marking_deque_;
  }
  WTF::Vector<HeapObjectHeader*>* HeadersToUnmark() const {
    return &headers_to_unmark_;
  }

  // Returns true if wrapper tracing is currently in progress, i.e.,
  // TracePrologue has been called, and TraceEpilogue has not yet been called.
  bool tracing_in_progress_ = false;

  // Is AdvanceTracing currently running? If not, we know that all calls of
  // pushToMarkingDeque are from V8 or new wrapper associations. And this
  // information is used by the verifier feature.
  bool advancing_tracing_ = false;

  // Indicates whether an idle task for a lazy cleanup has already been
  // scheduled. The flag is used to avoid scheduling multiple idle tasks for
  // cleaning up.
  bool idle_cleanup_task_scheduled_ = false;

  // Indicates whether cleanup should currently happen. The flag is used to
  // avoid cleaning up in the next GC cycle.
  bool should_cleanup_ = false;

  // Collection of objects we need to trace from. We assume it is safe to hold
  // on to the raw pointers because:
  // - oilpan object cannot move
  // - oilpan gc will call invalidateDeadObjectsInMarkingDeque to delete all
  //   obsolete objects
  mutable WTF::Deque<WrapperMarkingData> marking_deque_;

  // Collection of objects we started tracing from. We assume it is safe to
  // hold on to the raw pointers because:
  // - oilpan object cannot move
  // - oilpan gc will call invalidateDeadObjectsInMarkingDeque to delete
  //   all obsolete objects
  //
  // These objects are used when TraceWrappablesVerifier feature is enabled to
  // verify that all objects reachable in the atomic pause were marked
  // incrementally. If not, there is one or multiple write barriers missing.
  mutable WTF::Deque<WrapperMarkingData> verifier_deque_;

  // Collection of headers we need to unmark after the tracing finished. We
  // assume it is safe to hold on to the headers because:
  // - oilpan objects cannot move
  // - objects this headers belong to are invalidated by the oilpan GC in
  //   invalidateDeadObjectsInMarkingDeque.
  mutable WTF::Vector<HeapObjectHeader*> headers_to_unmark_;
  v8::Isolate* isolate_;

  FRIEND_TEST_ALL_PREFIXES(ScriptWrappableVisitorTest, MixinTracing);
  FRIEND_TEST_ALL_PREFIXES(ScriptWrappableVisitorTest,
                           OilpanClearsMarkingDequeWhenObjectDied);
  FRIEND_TEST_ALL_PREFIXES(ScriptWrappableVisitorTest,
                           ScriptWrappableVisitorTracesWrappers);
  FRIEND_TEST_ALL_PREFIXES(ScriptWrappableVisitorTest,
                           OilpanClearsHeadersWhenObjectDied);
  FRIEND_TEST_ALL_PREFIXES(
      ScriptWrappableVisitorTest,
      MarkedObjectDoesNothingOnWriteBarrierHitWhenDependencyIsMarkedToo);
  FRIEND_TEST_ALL_PREFIXES(
      ScriptWrappableVisitorTest,
      MarkedObjectMarksDependencyOnWriteBarrierHitWhenNotMarked);
  FRIEND_TEST_ALL_PREFIXES(ScriptWrappableVisitorTest,
                           WriteBarrierOnHeapVectorSwap1);
  FRIEND_TEST_ALL_PREFIXES(ScriptWrappableVisitorTest,
                           WriteBarrierOnHeapVectorSwap2);
};

}  // namespace blink

#endif  // ScriptWrappableVisitor_h
