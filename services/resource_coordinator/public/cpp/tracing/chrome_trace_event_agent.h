// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_TRACING_CHROME_TRACE_EVENT_AGENT_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_TRACING_CHROME_TRACE_EVENT_AGENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_export.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace tracing {

class SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT ChromeTraceEventAgent
    : public mojom::Agent {
 public:
  using MetadataGeneratorFunction =
      base::RepeatingCallback<std::unique_ptr<base::DictionaryValue>()>;

  static ChromeTraceEventAgent* GetInstance();

  explicit ChromeTraceEventAgent(service_manager::Connector* connector,
                                 const std::string& service_name);

  void AddMetadataGeneratorFunction(MetadataGeneratorFunction generator);

 private:
  friend std::default_delete<ChromeTraceEventAgent>;  // For Testing
  friend class ChromeTraceEventAgentTest;             // For Testing

  ~ChromeTraceEventAgent() override;

  // mojom::Agent
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    const StartTracingCallback& callback) override;
  void StopAndFlush(mojom::RecorderPtr recorder) override;
  void RequestClockSyncMarker(
      const std::string& sync_id,
      const RequestClockSyncMarkerCallback& callback) override;
  void RequestBufferStatus(
      const RequestBufferStatusCallback& callback) override;
  void GetCategories(const GetCategoriesCallback& callback) override;

  void OnTraceLogFlush(const scoped_refptr<base::RefCountedString>& events_str,
                       bool has_more_events);

  mojo::Binding<mojom::Agent> binding_;
  uint8_t enabled_tracing_modes_;
  mojom::RecorderPtr recorder_;
  std::vector<MetadataGeneratorFunction> metadata_generator_functions_;
  bool trace_log_needs_me_ = false;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ChromeTraceEventAgent);
};

}  // namespace tracing
#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_TRACING_CHROME_TRACE_EVENT_AGENT_H_
