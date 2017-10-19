// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceObserver_h
#define PerformanceObserver_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/timing/PerformanceEntry.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class PerformanceBase;
class PerformanceObserver;
class PerformanceObserverInit;
class V8PerformanceObserverCallback;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;

class CORE_EXPORT PerformanceObserver final
    : public GarbageCollected<PerformanceObserver>,
      public ScriptWrappable,
      public ActiveScriptWrappable<PerformanceObserver>,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceObserver);
  friend class PerformanceBase;
  friend class PerformanceBaseTest;
  friend class PerformanceObserverTest;

 public:
  static PerformanceObserver* Create(ScriptState*,
                                     V8PerformanceObserverCallback*);
  static void ResumeSuspendedObservers();

  void observe(const PerformanceObserverInit&, ExceptionState&);
  void disconnect();
  void EnqueuePerformanceEntry(PerformanceEntry&);
  PerformanceEntryTypeMask FilterOptions() const { return filter_options_; }

  // ScriptWrappable
  bool HasPendingActivity() const final;

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  PerformanceObserver(ExecutionContext*,
                      PerformanceBase*,
                      V8PerformanceObserverCallback*);
  void Deliver();
  bool ShouldBeSuspended() const;

  Member<ExecutionContext> execution_context_;
  TraceWrapperMember<V8PerformanceObserverCallback> callback_;
  WeakMember<PerformanceBase> performance_;
  PerformanceEntryVector performance_entries_;
  PerformanceEntryTypeMask filter_options_;
  bool is_registered_;
};

}  // namespace blink

#endif  // PerformanceObserver_h
