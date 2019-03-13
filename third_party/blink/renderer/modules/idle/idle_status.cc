// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/idle/idle_status.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/modules/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/idle/idle_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

IdleStatus* IdleStatus::Create(ExecutionContext* context,
                               base::TimeDelta threshold,
                               mojom::blink::IdleMonitorRequest request) {
  auto* status =
      MakeGarbageCollected<IdleStatus>(context, threshold, std::move(request));
  status->UpdateStateIfNeeded();
  return status;
}

IdleStatus::IdleStatus(ExecutionContext* context,
                       base::TimeDelta threshold,
                       mojom::blink::IdleMonitorRequest request)
    : ContextLifecycleStateObserver(context),
      threshold_(threshold),
      // See https://bit.ly/2S0zRAS for task types.
      binding_(this,
               std::move(request),
               context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {}

void IdleStatus::Init(mojom::blink::IdleStatePtr state) {
  state_ = MakeGarbageCollected<blink::IdleState>(std::move(state));
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

void IdleStatus::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning)
    StartMonitoring();
  else
    StopMonitoring();
}

void IdleStatus::ContextDestroyed(ExecutionContext*) {
  StopMonitoring();
}

void IdleStatus::StartMonitoring() {
  DCHECK(!binding_.is_bound());

  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);

  mojom::blink::IdleManagerPtr service;
  GetExecutionContext()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service, task_runner));

  mojom::blink::IdleMonitorPtr monitor_ptr;
  binding_.Bind(mojo::MakeRequest(&monitor_ptr, task_runner), task_runner);

  service->AddMonitor(
      threshold_, std::move(monitor_ptr),
      WTF::Bind(
          [](IdleStatus* status, mojom::blink::IdleStatePtr state) {
            if (state.get()->Equals(status->state_->state()))
              status->Update(std::move(state));
          },
          WrapWeakPersistent(this)));
}

void IdleStatus::StopMonitoring() {
  binding_.Close();
}

blink::IdleState* IdleStatus::state() const {
  return state_;
}

void IdleStatus::Update(mojom::blink::IdleStatePtr state) {
  DCHECK(binding_.is_bound());
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  Init(std::move(state));

  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void IdleStatus::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleStateObserver::Trace(visitor);
  visitor->Trace(state_);
}

}  // namespace blink
