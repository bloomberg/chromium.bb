// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorLogAgent_h
#define InspectorLogAgent_h

#include "core/CoreExport.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Log.h"

namespace v8_inspector {
class V8InspectorSession;
}

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;

class CORE_EXPORT InspectorLogAgent
    : public InspectorBaseAgent<protocol::Log::Metainfo>,
      public PerformanceMonitor::Client {
  USING_GARBAGE_COLLECTED_MIXIN(InspectorLogAgent);
  WTF_MAKE_NONCOPYABLE(InspectorLogAgent);

 public:
  InspectorLogAgent(ConsoleMessageStorage*,
                    PerformanceMonitor*,
                    v8_inspector::V8InspectorSession*);
  ~InspectorLogAgent() override;
  virtual void Trace(blink::Visitor*);

  void Restore() override;

  // Called from InspectorInstrumentation.
  void ConsoleMessageAdded(ConsoleMessage*);

  // Protocol methods.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response clear() override;
  protocol::Response startViolationsReport(
      std::unique_ptr<protocol::Array<protocol::Log::ViolationSetting>>)
      override;
  protocol::Response stopViolationsReport() override;

 private:
  // PerformanceMonitor::Client implementation.
  void ReportLongLayout(double duration) override;
  void ReportGenericViolation(PerformanceMonitor::Violation,
                              const String& text,
                              double time,
                              SourceLocation*) override;

  bool enabled_;
  Member<ConsoleMessageStorage> storage_;
  Member<PerformanceMonitor> performance_monitor_;
  v8_inspector::V8InspectorSession* v8_session_;
};

}  // namespace blink

#endif  // !defined(InspectorLogAgent_h)
