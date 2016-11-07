// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceMonitor_h
#define PerformanceMonitor_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebThread.h"
#include "public/platform/scheduler/base/task_time_observer.h"

namespace blink {

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
      public WebThread::TaskObserver,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(PerformanceMonitor);

 public:
  // Instrumenting methods, TODO: codegen those.
  static void performanceObserverAdded(Performance*);
  static void performanceObserverRemoved(Performance*);
  static void willExecuteScript(ExecutionContext*);
  static void didExecuteScript(ExecutionContext*);
  static void willUpdateLayout(Document*);
  static void didUpdateLayout(Document*);
  static void willRecalculateStyle(Document*);
  static void didRecalculateStyle(Document*);

  // Direct logging API for core.
  static bool enabled(ExecutionContext*);
  static void logViolation(MessageLevel, ExecutionContext*, const String&);
  static void logViolation(MessageLevel,
                           ExecutionContext*,
                           const String&,
                           std::unique_ptr<SourceLocation>);

  explicit PerformanceMonitor(LocalFrame*);
  ~PerformanceMonitor();

  void setLoggingEnabled(bool);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PerformanceMonitorTest;
  friend class PerformanceTest;

  static PerformanceMonitor* instrumentingMonitor(ExecutionContext*);

  void updateInstrumentation();

  void innerWillExecuteScript(ExecutionContext*);
  void didExecuteScript();
  void willUpdateLayout();
  void didUpdateLayout();
  void willRecalculateStyle();
  void didRecalculateStyle();

  void logViolation(MessageLevel, const String&);
  void logViolation(MessageLevel,
                    const String&,
                    std::unique_ptr<SourceLocation>);

  // WebThread::TaskObserver implementation.
  void willProcessTask() override;
  void didProcessTask() override;

  // scheduler::TaskTimeObserver implementation
  void ReportTaskTime(scheduler::TaskQueue*,
                      double startTime,
                      double endTime) override;

  std::pair<String, DOMWindow*> sanitizedAttribution(
      const HeapHashSet<Member<Frame>>& frameContexts,
      Frame* observerFrame);

  bool m_enabled = false;
  bool m_loggingEnabled = false;
  bool m_isExecutingScript = false;
  double m_layoutStartTime = 0;
  double m_styleStartTime = 0;
  double m_perTaskStyleAndLayoutTime = 0;
  Member<LocalFrame> m_localRoot;
  HeapHashSet<Member<Frame>> m_frameContexts;
  HeapHashSet<Member<Performance>> m_webPerformanceObservers;
};

}  // namespace blink

#endif  // PerformanceMonitor_h
