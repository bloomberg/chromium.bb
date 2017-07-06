// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_TRACING_TEST_UTIL_H_
#define SERVICES_RESOURCE_COORDINATOR_TRACING_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace tracing {

class MockAgent : public mojom::Agent {
 public:
  MockAgent();
  ~MockAgent() override;

  mojom::AgentPtr CreateAgentPtr();

  std::vector<std::string> call_stat() const { return call_stat_; }

  // Set these variables to configure the agent.
  std::vector<std::string> data_;
  base::DictionaryValue metadata_;
  std::string categories_;
  base::trace_event::TraceLogStatus trace_log_status_;

 private:
  // mojom::Agent
  void StartTracing(const std::string& config,
                    const StartTracingCallback& cb) override;
  void StopAndFlush(mojom::RecorderPtr recorder) override;
  void RequestClockSyncMarker(
      const std::string& sync_id,
      const RequestClockSyncMarkerCallback& cb) override;
  void GetCategories(const GetCategoriesCallback& cb) override;
  void RequestBufferStatus(const RequestBufferStatusCallback& cb) override;

  mojo::Binding<mojom::Agent> binding_;
  std::vector<std::string> call_stat_;
};

}  // namespace tracing

#endif  // SERVICES_RESOURCE_COORDINATOR_TRACING_TEST_UTIL_H_
