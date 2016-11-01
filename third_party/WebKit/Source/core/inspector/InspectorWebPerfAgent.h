// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorWebPerfAgent_h
#define InspectorWebPerfAgent_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebThread.h"
#include "public/platform/scheduler/base/task_time_observer.h"

namespace blink {

class DOMWindow;
class ExecutionContext;
class Frame;
class LocalFrame;
class Location;
class Performance;

// Inspector Agent for Web Performance APIs
// The agent is maintained per local root.
// Long task notifications are delivered to observing Performance* instances
// (in the local frame tree) in m_webPerformanceObservers.
//
// Usage:
// To create agent:
//     - instantiate and enable the agent:
//           inspectorWebPerfAgent = new InspectorWebPerfAgent(localFrame);
//           inspectorWebPerfAgent->enable();
//     - add first Performanace* instance:
//     inspectorWebPerfAgent->addWebPerformanceObserver(performance)
//
// To add additional Performance* observers, within the local frame tree:
//     inspectorWebPerfAgent->addWebPerformanceObserver(performance);
//
// To remove Performance* observer:
//     inspectorWebPerfAgent->removeWebPerformanceObserver(performance);
//
// To delete the agent:
//     - remove last Performance* observer
//           inspectorWebPerfAgent->removeWebPerformanceObserver(performance);
//     - ensure all Performance* observers have been removed:
//           if (!inspectorWebPerfAgent->hasWebPerformanceObservers()) { ..
//     - delete the agent:
//           inspectorWebPerfAgent->disable();
//           inspectorWebPerfAgent = nullptr; // depending on lifetime
//           management
//
class CORE_EXPORT InspectorWebPerfAgent final
    : public GarbageCollectedFinalized<InspectorWebPerfAgent>,
      public WebThread::TaskObserver,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(InspectorWebPerfAgent);
  friend class InspectorWebPerfAgentTest;

 public:
  explicit InspectorWebPerfAgent(LocalFrame*);
  ~InspectorWebPerfAgent();
  DECLARE_VIRTUAL_TRACE();

  void enable();
  void disable();
  bool isEnabled();

  void addWebPerformanceObserver(Performance*);
  void removeWebPerformanceObserver(Performance*);
  bool hasWebPerformanceObservers();

  void willExecuteScript(ExecutionContext*);
  void didExecuteScript();

  // WebThread::TaskObserver implementation.
  void willProcessTask() override;
  void didProcessTask() override;

  // scheduler::TaskTimeObserver implementation
  void ReportTaskTime(scheduler::TaskQueue*,
                      double startTime,
                      double endTime) override;

 private:
  bool m_enabled;
  std::pair<String, DOMWindow*> sanitizedAttribution(
      const HeapHashSet<Member<Frame>>& frameContexts,
      Frame* observerFrame);

  Member<LocalFrame> m_localRoot;
  HeapHashSet<Member<Frame>> m_frameContexts;
  HeapHashSet<Member<Performance>> m_webPerformanceObservers;
};

}  // namespace blink

#endif  // InspectorWebPerfAgent_h
