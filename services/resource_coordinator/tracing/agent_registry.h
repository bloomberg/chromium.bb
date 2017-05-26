// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_TRACING_AGENT_REGISTRY_H_
#define SERVICES_RESOURCE_COORDINATOR_TRACING_AGENT_REGISTRY_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace tracing {

class AgentRegistry : public mojom::AgentRegistry {
 public:
  class AgentEntry {
   public:
    AgentEntry(size_t id,
               AgentRegistry* agent_registry,
               mojom::AgentPtr agent,
               const std::string& label,
               mojom::TraceDataType type,
               bool supports_explicit_clock_sync);
    ~AgentEntry();

    // Currently, at most one callback when the tracing agent is disconnected is
    // enough. We can generalize this later if several parts of the service need
    // to get notified when an agent disconnects.
    void SetDisconnectClosure(base::OnceClosure closure);
    bool RemoveDisconnectClosure();

    mojom::Agent* agent() const { return agent_.get(); }
    const std::string& label() const { return label_; }
    mojom::TraceDataType type() const { return type_; }
    bool supports_explicit_clock_sync() const {
      return supports_explicit_clock_sync_;
    }

   private:
    void OnConnectionError();

    const size_t id_;
    AgentRegistry* agent_registry_;
    mojom::AgentPtr agent_;
    const std::string label_;
    const mojom::TraceDataType type_;
    const bool supports_explicit_clock_sync_;
    base::OnceClosure closure_;

    DISALLOW_COPY_AND_ASSIGN(AgentEntry);
  };

  // A function to be run for every agent that registers itself.
  using AgentInitializationCallback =
      base::RepeatingCallback<void(AgentEntry*)>;

  static AgentRegistry* GetInstance();

  AgentRegistry();

  void BindAgentRegistryRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::AgentRegistryRequest request);
  void SetAgentInitializationCallback(
      const AgentInitializationCallback& callback);
  void RemoveAgentInitializationCallback();

  template <typename FunctionType>
  void ForAllAgents(FunctionType function) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (const auto& key_value : agents_) {
      function(key_value.second.get());
    }
  }

 private:
  friend std::default_delete<AgentRegistry>;
  friend class AgentRegistryTest;  // For testing.

  ~AgentRegistry() override;

  // mojom::AgentRegistry
  void RegisterAgent(mojom::AgentPtr agent,
                     const std::string& label,
                     mojom::TraceDataType type,
                     bool supports_explicit_clock_sync) override;

  void UnregisterAgent(size_t agent_id);

  mojo::BindingSet<mojom::AgentRegistry> bindings_;
  size_t next_agent_id_ = 0;
  std::map<size_t, std::unique_ptr<AgentEntry>> agents_;
  AgentInitializationCallback agent_initialization_callback_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AgentRegistry);
};

}  // namespace tracing

#endif  // SERVICES_RESOURCE_COORDINATOR_TRACING_AGENT_REGISTRY_H_
