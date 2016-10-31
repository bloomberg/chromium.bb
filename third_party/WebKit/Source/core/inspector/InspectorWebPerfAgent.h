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

// Inspector Agent for Web Performance APIs
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
      const HeapHashSet<Member<Location>>& frameContextLocations,
      Frame* observerFrame);

  Member<LocalFrame> m_localFrame;
  HeapHashSet<Member<Location>> m_frameContextLocations;
};

}  // namespace blink

#endif  // InspectorWebPerfAgent_h
