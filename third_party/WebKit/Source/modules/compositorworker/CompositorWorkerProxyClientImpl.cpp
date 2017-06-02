// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerProxyClientImpl.h"

#include "core/animation/CompositorMutatorImpl.h"
#include "core/dom/CompositorProxy.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/graphics/CompositorMutableStateProvider.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

// A helper class that updates proxies mutable state on creation and reset it
// on destruction. This can be used with rAF callback to ensure no mutation is
// allowed outside rAF.
class ScopedCompositorMutableState final {
  WTF_MAKE_NONCOPYABLE(ScopedCompositorMutableState);
  STACK_ALLOCATED();

 public:
  ScopedCompositorMutableState(
      CompositorProxyClientImpl* compositor_proxy_client,
      CompositorMutableStateProvider* state_provider)
      : compositor_proxy_client_(compositor_proxy_client) {
    DCHECK(!IsMainThread());
    DCHECK(compositor_proxy_client_);
    for (CompositorProxy* proxy : compositor_proxy_client_->Proxies()) {
      proxy->TakeCompositorMutableState(
          state_provider->GetMutableStateFor(proxy->ElementId()));
    }
  }
  ~ScopedCompositorMutableState() {
    for (CompositorProxy* proxy : compositor_proxy_client_->Proxies())
      proxy->TakeCompositorMutableState(nullptr);
  }

 private:
  Member<CompositorProxyClientImpl> compositor_proxy_client_;
};

CompositorWorkerProxyClientImpl::CompositorWorkerProxyClientImpl(
    CompositorMutatorImpl* mutator)
    : mutator_(mutator) {
  DCHECK(IsMainThread());
}

DEFINE_TRACE(CompositorWorkerProxyClientImpl) {
  CompositorAnimator::Trace(visitor);
  CompositorWorkerProxyClient::Trace(visitor);
}

void CompositorWorkerProxyClientImpl::SetGlobalScope(WorkerGlobalScope* scope) {
  DCHECK(!IsMainThread());
  TRACE_EVENT0("compositor-worker",
               "CompositorWorkerProxyClientImpl::setGlobalScope");
  DCHECK(!global_scope_);
  DCHECK(!compositor_proxy_client_);
  DCHECK(scope);
  global_scope_ = static_cast<CompositorWorkerGlobalScope*>(scope);
  mutator_->RegisterCompositorAnimator(this);
  compositor_proxy_client_ = new CompositorProxyClientImpl();
}

void CompositorWorkerProxyClientImpl::Dispose() {
  DCHECK(!IsMainThread());
  // CompositorWorkerProxyClientImpl and CompositorMutatorImpl form a reference
  // cycle. CompositorWorkerGlobalScope and CompositorWorkerProxyClientImpl
  // also form another big reference cycle. So dispose needs to be called on
  // Worker termination to break these cycles. If not, layout test leak
  // detection will report a WorkerGlobalScope leak.
  mutator_->UnregisterCompositorAnimator(this);
  global_scope_ = nullptr;
}

void CompositorWorkerProxyClientImpl::RequestAnimationFrame() {
  DCHECK(!IsMainThread());
  TRACE_EVENT0("compositor-worker",
               "CompositorWorkerProxyClientImpl::requestAnimationFrame");
  requested_animation_frame_callbacks_ = true;
  mutator_->SetNeedsMutate();
}

bool CompositorWorkerProxyClientImpl::Mutate(
    double monotonic_time_now,
    CompositorMutableStateProvider* state_provider) {
  DCHECK(!IsMainThread());
  if (!global_scope_)
    return false;

  TRACE_EVENT0("compositor-worker", "CompositorWorkerProxyClientImpl::mutate");
  if (!requested_animation_frame_callbacks_)
    return false;

  {
    ScopedCompositorMutableState mutable_state(compositor_proxy_client_,
                                               state_provider);
    requested_animation_frame_callbacks_ =
        ExecuteAnimationFrameCallbacks(monotonic_time_now);
  }

  return requested_animation_frame_callbacks_;
}

bool CompositorWorkerProxyClientImpl::ExecuteAnimationFrameCallbacks(
    double monotonic_time_now) {
  DCHECK(!IsMainThread());
  TRACE_EVENT0(
      "compositor-worker",
      "CompositorWorkerProxyClientImpl::executeAnimationFrameCallbacks");

  DCHECK(global_scope_);
  // Convert to zero based document time in milliseconds consistent with
  // requestAnimationFrame.
  double high_res_time_ms =
      1000.0 * (monotonic_time_now - global_scope_->TimeOrigin());
  return global_scope_->ExecuteAnimationFrameCallbacks(high_res_time_ms);
}

}  // namespace blink
