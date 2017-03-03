// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceMonitor_h
#define PerformanceMonitor_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebThread.h"
#include "public/platform/scheduler/base/task_time_observer.h"
#include "v8/include/v8.h"
#include "wtf/text/AtomicString.h"

namespace blink {

namespace probe {
class RecalculateStyle;
}

class DOMWindow;
class Document;
class ExecutionContext;
class Frame;
class LocalFrame;
class Performance;
class SourceLocation;

// Performance monitor for Web Performance APIs and logging.
// The monitor is maintained per local root.
// Long task notifications are delivered to observing Performance* instances
// (in the local frame tree) in m_webPerformanceObservers.
class CORE_EXPORT PerformanceMonitor final
    : public GarbageCollectedFinalized<PerformanceMonitor>,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(PerformanceMonitor);

 public:
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

  class CORE_EXPORT HandlerCall {
    STACK_ALLOCATED();
   public:
    HandlerCall(ExecutionContext*, bool recurring);
    HandlerCall(ExecutionContext*, const char* name, bool recurring);
    HandlerCall(ExecutionContext*, const AtomicString& name, bool recurring);
    ~HandlerCall();

   private:
    Member<PerformanceMonitor> m_performanceMonitor;
  };

  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void reportLongTask(double startTime,
                                double endTime,
                                ExecutionContext* taskContext,
                                bool hasMultipleContexts){};
    virtual void reportLongLayout(double duration){};
    virtual void reportGenericViolation(Violation,
                                        const String& text,
                                        double time,
                                        SourceLocation*){};
    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  static void reportGenericViolation(ExecutionContext*,
                                     Violation,
                                     const String& text,
                                     double time,
                                     std::unique_ptr<SourceLocation>);
  static double threshold(ExecutionContext*, Violation);

  // Instrumenting methods.
  void willExecuteScript(ExecutionContext*);
  void didExecuteScript();
  void willCallFunction(ExecutionContext*);
  void didCallFunction(ExecutionContext*, v8::Local<v8::Function>);
  void willUpdateLayout();
  void didUpdateLayout();
  void will(const probe::RecalculateStyle&);
  void did(const probe::RecalculateStyle&);
  void documentWriteFetchScript(Document*);

  // Direct API for core.
  void subscribe(Violation, double threshold, Client*);
  void unsubscribeAll(Client*);
  void shutdown();

  explicit PerformanceMonitor(LocalFrame*);
  ~PerformanceMonitor();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PerformanceMonitorTest;
  friend class PerformanceTest;

  static PerformanceMonitor* monitor(const ExecutionContext*);
  static PerformanceMonitor* instrumentingMonitor(const ExecutionContext*);

  void updateInstrumentation();

  void innerReportGenericViolation(ExecutionContext*,
                                   Violation,
                                   const String& text,
                                   double time,
                                   std::unique_ptr<SourceLocation>);

  // scheduler::TaskTimeObserver implementation
  void willProcessTask(scheduler::TaskQueue*, double startTime) override;
  void didProcessTask(scheduler::TaskQueue*,
                      double startTime,
                      double endTime) override;

  std::pair<String, DOMWindow*> sanitizedAttribution(
      const HeapHashSet<Member<Frame>>& frameContexts,
      Frame* observerFrame);

  bool m_enabled = false;
  double m_scriptStartTime = 0;
  double m_layoutStartTime = 0;
  double m_styleStartTime = 0;
  double m_perTaskStyleAndLayoutTime = 0;
  unsigned m_scriptDepth = 0;
  unsigned m_layoutDepth = 0;
  unsigned m_handlerDepth = 0;
  Violation m_handlerType = Violation::kAfterLast;

  const char* m_handlerName = nullptr;
  AtomicString m_handlerAtomicName;

  double m_thresholds[kAfterLast];

  Member<LocalFrame> m_localRoot;
  Member<ExecutionContext> m_taskExecutionContext;
  bool m_taskHasMultipleContexts = false;
  using ClientThresholds = HeapHashMap<Member<Client>, double>;
  HeapHashMap<Violation,
              Member<ClientThresholds>,
              typename DefaultHash<size_t>::Hash,
              WTF::UnsignedWithZeroKeyHashTraits<size_t>>
      m_subscriptions;
};

}  // namespace blink

#endif  // PerformanceMonitor_h
