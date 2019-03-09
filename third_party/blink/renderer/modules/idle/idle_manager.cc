// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/idle/idle_manager.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/idle/idle_status.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

const uint32_t kDefaultThresholdSeconds = 60;

IdleManager::IdleManager(ExecutionContext* context) {}

ScriptPromise IdleManager::query(ScriptState* script_state,
                                 const IdleOptions* options,
                                 ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  // Validate options.
  int32_t threshold_seconds =
      options->hasThreshold() ? options->threshold() : kDefaultThresholdSeconds;

  if (threshold_seconds <= 0) {
    exception_state.ThrowTypeError("Invalid threshold");
    return ScriptPromise();
  }

  base::TimeDelta threshold = base::TimeDelta::FromSeconds(threshold_seconds);

  // TODO: Permission check.

  if (!service_) {
    // NOTE(goto): what are the benefits of initializing this here
    // as opposed to the constructor? lazy loading?
    context->GetInterfaceProvider()->GetInterface(mojo::MakeRequest(&service_));
    service_.set_connection_error_handler(WTF::Bind(
        &IdleManager::OnIdleManagerConnectionError, WrapWeakPersistent(this)));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::IdleMonitorPtr monitor_ptr;
  IdleStatus* status =
      IdleStatus::Create(ExecutionContext::From(script_state), threshold,
                         mojo::MakeRequest(&monitor_ptr));

  requests_.insert(resolver);
  service_->AddMonitor(
      threshold, std::move(monitor_ptr),
      WTF::Bind(&IdleManager::OnAddMonitor, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(status)));

  return promise;
}

IdleManager* IdleManager::Create(ExecutionContext* context) {
  IdleManager* idle_manager = MakeGarbageCollected<IdleManager>(context);
  return idle_manager;
}

void IdleManager::OnAddMonitor(ScriptPromiseResolver* resolver,
                               IdleStatus* status,
                               mojom::blink::IdleStatePtr state) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  status->Init(std::move(state));
  resolver->Resolve(status);
}

void IdleManager::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(requests_);
}

void IdleManager::OnIdleManagerConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                         "Idle detection not available"));
  }
  requests_.clear();
  service_.reset();
}

}  // namespace blink
