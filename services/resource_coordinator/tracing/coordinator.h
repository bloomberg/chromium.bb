// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_TRACING_COORDINATOR_H_
#define SERVICES_RESOURCE_COORDINATOR_TRACING_COORDINATOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"
#include "services/resource_coordinator/tracing/agent_registry.h"
#include "services/resource_coordinator/tracing/recorder.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace tracing {

// Note that this implementation of mojom::Coordinator assumes that agents
// either respond to messages that expect a response or disconnect. Mojo
// verifies this to some extend by DCHECKing if the callback is deleted by the
// agent before being run. However, the agent should not store the callback and
// never run it.
//
// If we see that the above-mentioned assumption does not hold in some cases, we
// should guard against it using timeouts.
class Coordinator : public mojom::Coordinator {
 public:
  static Coordinator* GetInstance();

  Coordinator();

  void BindCoordinatorRequest(
      mojom::CoordinatorRequest request,
      const service_manager::BindSourceInfo& source_info);

 private:
  friend std::default_delete<Coordinator>;
  friend class CoordinatorTest;  // For testing.

  class TraceStreamer;

  ~Coordinator() override;

  // mojom::Coordinator
  void StartTracing(const std::string& config,
                    const StartTracingCallback& callback) override;
  void StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                    const StopAndFlushCallback& callback) override;
  void StopAndFlushAgent(mojo::ScopedDataPipeProducerHandle stream,
                         const std::string& agent_label,
                         const StopAndFlushCallback& callback) override;
  void IsTracing(const IsTracingCallback& callback) override;
  void RequestBufferUsage(const RequestBufferUsageCallback& callback) override;
  void GetCategories(const GetCategoriesCallback& callback) override;

  // Internal methods for collecting events from agents.
  void SendStartTracingToAgent(AgentRegistry::AgentEntry* agent_entry);
  void OnTracingStarted(AgentRegistry::AgentEntry* agent_entry, bool success);
  void StopAndFlushInternal();
  void OnRequestClockSyncMarkerResponse(AgentRegistry::AgentEntry* agent_entry,
                                        const std::string& sync_id,
                                        base::TimeTicks issue_ts,
                                        base::TimeTicks issue_end_ts);
  void StopAndFlushAfterClockSync();
  void SendRecorder(base::WeakPtr<AgentRegistry::AgentEntry> agent_entry,
                    mojom::RecorderPtr recorder);
  void OnFlushDone();

  void OnRequestBufferStatusResponse(AgentRegistry::AgentEntry* agent_entry,
                                     uint32_t capacity,
                                     uint32_t count);

  void OnGetCategoriesResponse(AgentRegistry::AgentEntry* agent_entry,
                               const std::string& categories);

  mojo::Binding<mojom::Coordinator> binding_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  AgentRegistry* agent_registry_;
  std::string config_;
  bool is_tracing_ = false;

  std::unique_ptr<TraceStreamer> trace_streamer_;
  StartTracingCallback start_tracing_callback_;
  StopAndFlushCallback stop_and_flush_callback_;

  // For computing trace buffer usage.
  float maximum_trace_buffer_usage_ = 0;
  uint32_t approximate_event_count_ = 0;
  RequestBufferUsageCallback request_buffer_usage_callback_;

  // For getting categories.
  std::set<std::string> category_set_;
  GetCategoriesCallback get_categories_callback_;

  base::WeakPtrFactory<Coordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Coordinator);
};

}  // namespace tracing
#endif  // SERVICES_RESOURCE_COORDINATOR_TRACING_COORDINATOR_H_
