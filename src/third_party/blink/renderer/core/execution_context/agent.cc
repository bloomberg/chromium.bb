// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

namespace {
bool is_cross_origin_isolated = false;
bool is_direct_socket_potentially_available = false;

#if DCHECK_IS_ON()
bool is_cross_origin_isolated_set = false;
bool is_direct_socket_potentially_available_set = false;
#endif
}  // namespace

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : Agent(isolate, cluster_id, std::move(microtask_queue), false, true) {}

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue,
             bool is_origin_agent_cluster,
             bool origin_agent_cluster_left_as_default)
    : event_loop_(base::AdoptRef(
          new scheduler::EventLoop(isolate, std::move(microtask_queue)))),
      cluster_id_(cluster_id),
      origin_keyed_because_of_inheritance_(false),
      is_origin_agent_cluster_(is_origin_agent_cluster),
      origin_agent_cluster_left_as_default_(
          origin_agent_cluster_left_as_default) {}

Agent::~Agent() = default;

void Agent::Trace(Visitor* visitor) const {}

void Agent::AttachContext(ExecutionContext* context) {
  event_loop_->AttachScheduler(context->GetScheduler());
}

void Agent::DetachContext(ExecutionContext* context) {
  event_loop_->DetachScheduler(context->GetScheduler());
}

// static
bool Agent::IsCrossOriginIsolated() {
  return is_cross_origin_isolated;
}

// static
void Agent::SetIsCrossOriginIsolated(bool value) {
#if DCHECK_IS_ON()
  if (is_cross_origin_isolated_set)
    DCHECK_EQ(is_cross_origin_isolated, value);
  is_cross_origin_isolated_set = true;
#endif
  is_cross_origin_isolated = value;
}

// static
bool Agent::IsDirectSocketEnabled() {
  return is_direct_socket_potentially_available;
}

// static
void Agent::SetIsDirectSocketEnabled(bool value) {
#if DCHECK_IS_ON()
  if (is_direct_socket_potentially_available_set)
    DCHECK_EQ(is_direct_socket_potentially_available, value);
  is_direct_socket_potentially_available_set = true;
#endif
  is_direct_socket_potentially_available = value;
}

bool Agent::IsOriginKeyed() const {
  return IsCrossOriginIsolated() || IsOriginKeyedForInheritance();
}

bool Agent::IsOriginKeyedForInheritance() const {
  return is_origin_agent_cluster_ || origin_keyed_because_of_inheritance_;
}

bool Agent::IsOriginOrSiteKeyedBasedOnDefault() const {
  return origin_agent_cluster_left_as_default_;
}

void Agent::ForceOriginKeyedBecauseOfInheritance() {
  origin_keyed_because_of_inheritance_ = true;
}

}  // namespace blink
