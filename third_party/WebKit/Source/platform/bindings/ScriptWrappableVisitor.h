// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/VisitorImpl.h"
#include "platform/heap/WrapperVisitor.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

class HeapObjectHeader;
template <typename T>
class Member;
class ScriptWrappable;
template <typename T>
class TraceWrapperV8Reference;

class WrapperMarkingData {
 public:
  WrapperMarkingData(
      void (*trace_wrappers_callback)(const WrapperVisitor*, const void*),
      HeapObjectHeader* (*heap_object_header_callback)(const void*),
      const void* object)
      : trace_wrappers_callback_(trace_wrappers_callback),
        heap_object_header_callback_(heap_object_header_callback),
        raw_object_pointer_(object) {
    DCHECK(trace_wrappers_callback_);
    DCHECK(heap_object_header_callback_);
    DCHECK(raw_object_pointer_);
  }

  inline void TraceWrappers(WrapperVisitor* visitor) {
    if (raw_object_pointer_) {
      trace_wrappers_callback_(visitor, raw_object_pointer_);
    }
  }

  // Returns true when object was marked. Ignores (returns true) invalidated
  // objects.
  inline bool IsWrapperHeaderMarked() {
    return !raw_object_pointer_ ||
           GetHeapObjectHeader()->IsWrapperHeaderMarked();
  }

  inline const void* RawObjectPointer() { return raw_object_pointer_; }

 private:
  inline bool ShouldBeInvalidated() {
    return raw_object_pointer_ && !GetHeapObjectHeader()->IsMarked();
  }

  inline void Invalidate() { raw_object_pointer_ = nullptr; }

  inline const HeapObjectHeader* GetHeapObjectHeader() {
    DCHECK(raw_object_pointer_);
    return heap_object_header_callback_(raw_object_pointer_);
  }

  void (*trace_wrappers_callback_)(const WrapperVisitor*, const void*);
  HeapObjectHeader* (*heap_object_header_callback_)(const void*);
  const void* raw_object_pointer_;

  friend class ScriptWrappableVisitor;
};

// ScriptWrappableVisitor is able to trace through the objects to get all
// wrappers. It is used during V8 garbage collection.  When this visitor is
// set to the v8::Isolate as its embedder heap tracer, V8 will call it during
// its garbage collection. At the beginning, it will call TracePrologue, then
// repeatedly it will call AdvanceTracing, and at the end it will call
// TraceEpilogue. Everytime V8 finds new wrappers, it will let the tracer
// know using RegisterV8References.
class PLATFORM_EXPORT ScriptWrappableVisitor : public v8::EmbedderHeapTracer,
                                               public WrapperVisitor {
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScriptWrappableVisitor);

 public:
  ScriptWrappableVisitor(v8::Isolate* isolate) : isolate_(isolate){};
  ~ScriptWrappableVisitor() override;

  // Replace all dead objects in the marking deque with nullptr after oilpan
  // gc.
  static void InvalidateDeadObjectsInMarkingDeque(v8::Isolate*);

  // Immediately clean up all wrappers.
  static void PerformCleanup(v8::Isolate*);

  void TracePrologue() override;

  static WrapperVisitor* CurrentVisitor(v8::Isolate*);

  static void WriteBarrier(v8::Isolate*,
                           const TraceWrapperV8Reference<v8::Value>*);

  // TODO(mlippautz): Remove once ScriptWrappable is converted to
  // TraceWrapperV8Reference.
  static void WriteBarrier(v8::Isolate*, const v8::Persistent<v8::Object>*);

  template <typename T>
  static void WriteBarrier(const Member<T>& value) {
    WriteBarrier(value.Get());
  }

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
    static_assert(!NeedsAdjustAndMark<T>::value,
                  "wrapper tracing is not supported within mixins");
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

  void RegisterV8References(const std::vector<std::pair<void*, void*>>&
                                internal_fields_of_potential_wrappers) override;
  void RegisterV8Reference(const std::pair<void*, void*>& internal_fields);
  bool AdvanceTracing(double deadline_in_ms,
                      v8::EmbedderHeapTracer::AdvanceTracingActions) override;
  void TraceEpilogue() override;
  void AbortTracing() override;
  void EnterFinalPause() override;
  size_t NumberOfWrappersToTrace() override;

  void DispatchTraceWrappers(const TraceWrapperBase*) const override;

  void TraceWrappers(const TraceWrapperV8Reference<v8::Value>&) const override;
  void MarkWrapper(const v8::PersistentBase<v8::Value>*) const override;

  void InvalidateDeadObjectsInMarkingDeque();

  bool MarkWrapperHeader(HeapObjectHeader*) const;

  // Mark wrappers in all worlds for the given script wrappable as alive in
  // V8.
  void MarkWrappersInAllWorlds(const ScriptWrappable*) const override;

  WTF::Deque<WrapperMarkingData>* GetMarkingDeque() { return &marking_deque_; }
  WTF::Deque<WrapperMarkingData>* GetVerifierDeque() {
    return &verifier_deque_;
  }
  WTF::Vector<HeapObjectHeader*>* GetHeadersToUnmark() {
    return &headers_to_unmark_;
  }

  // Immediately cleans up all wrappers if necessary.
  void PerformCleanup();

 protected:
  bool PushToMarkingDeque(
      void (*trace_wrappers_callback)(const WrapperVisitor*, const void*),
      HeapObjectHeader* (*heap_object_header_callback)(const void*),
      void (*missed_write_barrier_callback)(void),
      const void* object) const override {
    DCHECK(tracing_in_progress_);
    marking_deque_.push_back(WrapperMarkingData(
        trace_wrappers_callback, heap_object_header_callback, object));
#if DCHECK_IS_ON()
    if (!advancing_tracing_) {
      verifier_deque_.push_back(WrapperMarkingData(
          trace_wrappers_callback, heap_object_header_callback, object));
    }
#endif
    return true;
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

  // Schedule an idle task to perform a lazy (incremental) clean up of
  // wrappers.
  void ScheduleIdleLazyCleanup();
  void PerformLazyCleanup(double deadline_seconds);

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
};

}  // namespace blink

#endif  // ScriptWrappableVisitor_h
