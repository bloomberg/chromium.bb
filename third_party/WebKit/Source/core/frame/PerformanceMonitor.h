// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceMonitor_h
#define PerformanceMonitor_h

#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebThread.h"
#include "v8/include/v8.h"

namespace blink {

namespace probe {
class CallFunction;
class ExecuteScript;
class RecalculateStyle;
class UpdateLayout;
class UserCallback;
class V8Compile;
}

class DOMWindow;
class Document;
class ExecutionContext;
class Performance;
class SourceLocation;

#define PERF_METRICS_LIST(V) \
  V(DocumentCount)           \
  V(JSEventListenerCount)    \
  V(NodeCount)

// Performance monitor for Web Performance APIs and logging.
// The monitor is maintained per local root.
// Long task notifications are delivered to observing Performance* instances
// (in the local frame tree) in m_webPerformanceObservers.
class CORE_EXPORT PerformanceMonitor final
    : public GarbageCollectedFinalized<PerformanceMonitor>,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(PerformanceMonitor);

 public:
  enum MetricsType {
#define DECLARE_PERF_METRIC_NAME(name) k##name,
    PERF_METRICS_LIST(DECLARE_PERF_METRIC_NAME)
#undef DECLARE_PERF_METRIC_NAME
        kMaxMetricType
  };

  enum Violation : size_t {
    kLongTask,
    kLongLayout,
    kBlockedEvent,
    kBlockedParser,
    kDiscouragedAPIUse,
    kHandler,
    kRecurringHandler,
    kAfterLast
  };

  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void ReportLongTask(double start_time,
                                double end_time,
                                ExecutionContext* task_context,
                                bool has_multiple_contexts){};
    virtual void ReportLongLayout(double duration){};
    virtual void ReportGenericViolation(Violation,
                                        const String& text,
                                        double time,
                                        SourceLocation*){};
    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  static void ReportGenericViolation(ExecutionContext*,
                                     Violation,
                                     const String& text,
                                     double time,
                                     std::unique_ptr<SourceLocation>);
  static double Threshold(ExecutionContext*, Violation);

  // Instrumenting methods.
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);

  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);

  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  void Will(const probe::UserCallback&);
  void Did(const probe::UserCallback&);

  void Will(const probe::V8Compile&);
  void Did(const probe::V8Compile&);

  void DocumentWriteFetchScript(Document*);

  static inline void IncrementCounter(LocalFrame* frame, MetricsType type) {
    if (frame)
      ++frame->GetPerformanceMonitor()->metric_values_[type];
  }

  static inline void DecrementCounter(LocalFrame* frame, MetricsType type) {
    if (frame)
      --frame->GetPerformanceMonitor()->metric_values_[type];
  }

  int PerfMetricValue(int id) const { return metric_values_[id]; }

  // Direct API for core.
  void Subscribe(Violation, double threshold, Client*);
  void UnsubscribeAll(Client*);
  void Shutdown();

  explicit PerformanceMonitor(LocalFrame*);
  ~PerformanceMonitor();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PerformanceMonitorTest;
  friend class PerformanceTest;

  static PerformanceMonitor* Monitor(const ExecutionContext*);
  static PerformanceMonitor* InstrumentingMonitor(const ExecutionContext*);

  void UpdateInstrumentation();

  void InnerReportGenericViolation(ExecutionContext*,
                                   Violation,
                                   const String& text,
                                   double time,
                                   std::unique_ptr<SourceLocation>);

  // scheduler::TaskTimeObserver implementation
  void WillProcessTask(double start_time) override;
  void DidProcessTask(double start_time, double end_time) override;
  void OnBeginNestedRunLoop() override {}
  void WillExecuteScript(ExecutionContext*);
  void DidExecuteScript();

  std::pair<String, DOMWindow*> SanitizedAttribution(
      const HeapHashSet<Member<Frame>>& frame_contexts,
      Frame* observer_frame);

  bool enabled_ = false;
  double per_task_style_and_layout_time_ = 0;
  unsigned script_depth_ = 0;
  unsigned layout_depth_ = 0;
  unsigned user_callback_depth_ = 0;
  const void* user_callback_;

  double thresholds_[kAfterLast];

  Member<LocalFrame> local_root_;
  Member<ExecutionContext> task_execution_context_;
  bool task_has_multiple_contexts_ = false;
  using ClientThresholds = HeapHashMap<WeakMember<Client>, double>;
  HeapHashMap<Violation,
              Member<ClientThresholds>,
              typename DefaultHash<size_t>::Hash,
              WTF::UnsignedWithZeroKeyHashTraits<size_t>>
      subscriptions_;
  int metric_values_[kMaxMetricType]{};
};

}  // namespace blink

#endif  // PerformanceMonitor_h
