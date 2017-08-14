// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorPerformanceAgent_h
#define InspectorPerformanceAgent_h

#include "core/CoreExport.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Performance.h"

namespace blink {

class InspectedFrames;

class CORE_EXPORT InspectorPerformanceAgent final
    : public InspectorBaseAgent<protocol::Performance::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorPerformanceAgent);

 public:
  DECLARE_VIRTUAL_TRACE();

  static InspectorPerformanceAgent* Create(InspectedFrames* inspected_frames) {
    return new InspectorPerformanceAgent(inspected_frames);
  }
  ~InspectorPerformanceAgent() override;

  // Performance protocol domain implementation.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getMetrics(
      std::unique_ptr<protocol::Array<protocol::Performance::Metric>>*
          out_result) override;

  // PerformanceMetrics probes implementation.
  void ConsoleTimeStamp(const String& title);

 private:
  InspectorPerformanceAgent(InspectedFrames*);

  Member<PerformanceMonitor> performance_monitor_;
  bool enabled_ = false;
  static const char* metric_names_[];
};

}  // namespace blink

#endif  // !defined(InspectorPerformanceAgent_h)
