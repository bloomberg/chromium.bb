// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/tracing/agent_registry.h"

#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace {
tracing::AgentRegistry* g_agent_registry;
}

namespace tracing {

AgentRegistry::AgentEntry::AgentEntry(size_t id,
                                      AgentRegistry* agent_registry,
                                      mojom::AgentPtr agent,
                                      const std::string& label,
                                      mojom::TraceDataType type,
                                      bool supports_explicit_clock_sync)
    : id_(id),
      agent_registry_(agent_registry),
      agent_(std::move(agent)),
      label_(label),
      type_(type),
      supports_explicit_clock_sync_(supports_explicit_clock_sync) {
  DCHECK(!label.empty());
  agent_.set_connection_error_handler(base::BindRepeating(
      &AgentRegistry::AgentEntry::OnConnectionError, base::Unretained(this)));
}

AgentRegistry::AgentEntry::~AgentEntry() = default;

void AgentRegistry::AgentEntry::SetDisconnectClosure(
    base::OnceClosure closure) {
  DCHECK(closure_.is_null());
  closure_ = std::move(closure);
}

bool AgentRegistry::AgentEntry::RemoveDisconnectClosure() {
  bool closure_was_set = !closure_.is_null();
  closure_.Reset();
  return closure_was_set;
}

void AgentRegistry::AgentEntry::OnConnectionError() {
  // Run the disconnect closure if it is set. We should mark |closure_| as
  // movable so that the version of |Run| that takes an rvalue reference is
  // selected not the version that takes a const reference. The former is for
  // once callbacks and the latter is for repeating callbacks.
  if (!closure_.is_null())
    std::move(closure_).Run();
  agent_registry_->UnregisterAgent(id_);
}

// static
AgentRegistry* AgentRegistry::GetInstance() {
  return g_agent_registry;
}

AgentRegistry::AgentRegistry() {
  DCHECK(!g_agent_registry);
  g_agent_registry = this;
}

AgentRegistry::~AgentRegistry() {
  // For testing only.
  g_agent_registry = nullptr;
}

void AgentRegistry::BindAgentRegistryRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::AgentRegistryRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void AgentRegistry::SetAgentInitializationCallback(
    const AgentInitializationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  agent_initialization_callback_ = callback;
  ForAllAgents([this](AgentEntry* agent_entry) {
    agent_initialization_callback_.Run(agent_entry);
  });
}

void AgentRegistry::RemoveAgentInitializationCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  agent_initialization_callback_.Reset();
}

void AgentRegistry::RegisterAgent(mojom::AgentPtr agent,
                                  const std::string& label,
                                  mojom::TraceDataType type,
                                  bool supports_explicit_clock_sync) {
  auto id = next_agent_id_++;
  auto entry = base::MakeUnique<AgentEntry>(id, this, std::move(agent), label,
                                            type, supports_explicit_clock_sync);
  if (!agent_initialization_callback_.is_null())
    agent_initialization_callback_.Run(entry.get());
  auto result = agents_.insert(std::make_pair(id, std::move(entry)));
  DCHECK(result.second);
}

void AgentRegistry::UnregisterAgent(size_t agent_id) {
  size_t num_deleted = agents_.erase(agent_id);
  DCHECK_EQ(1u, num_deleted);
}

}  // namespace tracing
