// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_status.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

IdleStatus::IdleStatus(ExecutionContext* context,
                       uint32_t threshold,
                       mojom::blink::IdleMonitorRequest request)
    : PausableObject(context),
      threshold_(threshold),
      binding_(this, std::move(request)) {
  PauseIfNeeded();
}

void IdleStatus::Init(IdleState state) {
  state_ = state;
}

IdleStatus::~IdleStatus() = default;

void IdleStatus::Dispose() {
  StopMonitoring();
}

const AtomicString& IdleStatus::InterfaceName() const {
  return event_target_names::kIdleStatus;
}

ExecutionContext* IdleStatus::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool IdleStatus::HasPendingActivity() const {
  return binding_.is_bound();
}

void IdleStatus::ContextUnpaused() {
  StartMonitoring();
}

void IdleStatus::ContextPaused(PauseState) {
  StopMonitoring();
}

void IdleStatus::ContextDestroyed(ExecutionContext*) {
  StopMonitoring();
}

void IdleStatus::StartMonitoring() {
  DCHECK(!binding_.is_bound());

  mojom::blink::IdleManagerPtr service;
  GetExecutionContext()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service));

  mojom::blink::IdleMonitorPtr monitor_ptr;
  binding_.Bind(mojo::MakeRequest(&monitor_ptr));

  service->AddMonitor(threshold_, std::move(monitor_ptr),
                      WTF::Bind(
                          [](IdleStatus* status, IdleState state) {
                            if (state != status->state_)
                              status->Update(state);
                          },
                          WrapWeakPersistent(this)));
}

void IdleStatus::StopMonitoring() {
  binding_.Close();
}

String IdleStatus::state() const {
  switch (state_) {
    case IdleState::ACTIVE:
      return "active";
    case IdleState::IDLE:
      return "idle";
    case IdleState::LOCKED:
      return "locked";
  }
}

void IdleStatus::Update(IdleState state) {
  DCHECK(binding_.is_bound());
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  state_ = state;
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void IdleStatus::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
}

}  // namespace blink
