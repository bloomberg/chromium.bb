// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceObserver_h
#define PerformanceObserver_h

#include "core/CoreExport.h"
#include "core/timing/PerformanceEntry.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class PerformanceBase;
class PerformanceObserver;
class PerformanceObserverCallback;
class PerformanceObserverInit;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;

class CORE_EXPORT PerformanceObserver final
    : public GarbageCollected<PerformanceObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  friend class PerformanceBase;
  friend class PerformanceBaseTest;
  friend class PerformanceObserverTest;

 public:
  static PerformanceObserver* Create(ScriptState*,
                                     PerformanceObserverCallback*);
  static void ResumeSuspendedObservers();

  void observe(const PerformanceObserverInit&, ExceptionState&);
  void disconnect();
  void EnqueuePerformanceEntry(PerformanceEntry&);
  PerformanceEntryTypeMask FilterOptions() const { return filter_options_; }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  PerformanceObserver(ScriptState*,
                      PerformanceBase*,
                      PerformanceObserverCallback*);
  void Deliver();
  bool ShouldBeSuspended() const;

  Member<ExecutionContext> execution_context_;
  TraceWrapperMember<PerformanceObserverCallback> callback_;
  WeakMember<PerformanceBase> performance_;
  PerformanceEntryVector performance_entries_;
  PerformanceEntryTypeMask filter_options_;
  bool is_registered_;
};

}  // namespace blink

#endif  // PerformanceObserver_h
