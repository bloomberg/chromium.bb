// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "services/tracing/public/cpp/base_agent.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace tracing {

// Agent used to interface with the legacy tracing system.
// When Perfetto is used for the backend instead of TraceLog,
// most of the mojom::Agent functions will never be used
// as the control signals will go through the Perfetto
// interface instead.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventAgent : public BaseAgent {
 public:
  using MetadataGeneratorFunction =
      base::RepeatingCallback<std::unique_ptr<base::DictionaryValue>()>;

  static std::unique_ptr<TraceEventAgent> Create(
      service_manager::Connector* connector,
      bool request_clock_sync_marker_on_android);

  TraceEventAgent(service_manager::Connector* connector,
                        bool request_clock_sync_marker_on_android);

  void AddMetadataGeneratorFunction(MetadataGeneratorFunction generator);

 private:
  friend std::default_delete<TraceEventAgent>;      // For Testing
  friend class TraceEventAgentTest;                 // For Testing

  ~TraceEventAgent() override;

  // mojom::Agent
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    StartTracingCallback callback) override;
  void StopAndFlush(mojom::RecorderPtr recorder) override;

  void RequestClockSyncMarker(
      const std::string& sync_id,
      Agent::RequestClockSyncMarkerCallback callback) override;

  void GetCategories(GetCategoriesCallback callback) override;

  void RequestBufferStatus(RequestBufferStatusCallback callback) override;

  void OnTraceLogFlush(const scoped_refptr<base::RefCountedString>& events_str,
                       bool has_more_events);

  uint8_t enabled_tracing_modes_;
  mojom::RecorderPtr recorder_;
  std::vector<MetadataGeneratorFunction> metadata_generator_functions_;
  bool trace_log_needs_me_ = false;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(TraceEventAgent);
};

}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_
