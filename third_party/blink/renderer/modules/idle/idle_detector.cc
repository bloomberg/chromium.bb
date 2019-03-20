// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/idle/idle_detector.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/modules/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/idle/idle_options.h"
#include "third_party/blink/renderer/modules/idle/idle_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {
namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"idle-detection\" is disallowed by feature policy.";

const uint32_t kDefaultThresholdSeconds = 60;

}  // namespace

IdleDetector* IdleDetector::Create(ScriptState* script_state,
                                   const IdleOptions* options,
                                   ExceptionState& exception_state) {
  int32_t threshold_seconds =
      options->hasThreshold() ? options->threshold() : kDefaultThresholdSeconds;

  if (threshold_seconds <= 0) {
    exception_state.ThrowTypeError("Invalid threshold");
    return nullptr;
  }

  base::TimeDelta threshold = base::TimeDelta::FromSeconds(threshold_seconds);

  auto* detector = MakeGarbageCollected<IdleDetector>(
      ExecutionContext::From(script_state), threshold);
  return detector;
}

// static
IdleDetector* IdleDetector::Create(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  return Create(script_state, IdleOptions::Create(), exception_state);
}

IdleDetector::IdleDetector(ExecutionContext* context, base::TimeDelta threshold)
    : ContextClient(context), threshold_(threshold), binding_(this) {}

void IdleDetector::Init(mojom::blink::IdleStatePtr state) {
  state_ = MakeGarbageCollected<blink::IdleState>(std::move(state));
}

IdleDetector::~IdleDetector() = default;

void IdleDetector::Dispose() {
  StopMonitoring();
}

const AtomicString& IdleDetector::InterfaceName() const {
  return event_target_names::kIdleDetector;
}

ExecutionContext* IdleDetector::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

bool IdleDetector::HasPendingActivity() const {
  return binding_.is_bound();
}

ScriptPromise IdleDetector::start(ScriptState* script_state) {
  // Validate options.
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->GetSecurityContext().IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kIdleDetection,
          ReportOptions::kReportOnFailure)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlocked));
  }

  StartMonitoring();

  return ScriptPromise::CastUndefined(script_state);
}

void IdleDetector::stop() {
  StopMonitoring();
}

void IdleDetector::StartMonitoring() {
  if (binding_.is_bound()) {
    return;
  }

  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);

  if (!service_) {
    GetExecutionContext()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&service_, task_runner));
  }

  mojom::blink::IdleMonitorPtr monitor_ptr;
  binding_.Bind(mojo::MakeRequest(&monitor_ptr, task_runner), task_runner);

  service_->AddMonitor(
      threshold_, std::move(monitor_ptr),
      WTF::Bind(&IdleDetector::OnAddMonitor, WrapPersistent(this)));
}

void IdleDetector::StopMonitoring() {
  binding_.Close();
}

void IdleDetector::OnAddMonitor(mojom::blink::IdleStatePtr state) {
  Init(std::move(state));
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

blink::IdleState* IdleDetector::state() const {
  return state_;
}

void IdleDetector::Update(mojom::blink::IdleStatePtr state) {
  DCHECK(binding_.is_bound());
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  if (state.get()->Equals(state_->state()))
    return;

  Init(std::move(state));
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void IdleDetector::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
  visitor->Trace(state_);
}

}  // namespace blink
