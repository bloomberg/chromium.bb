// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerProxyClientImpl.h"

#include "core/animation/CompositorMutatorImpl.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameBase.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

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
  DCHECK(scope);
  global_scope_ = static_cast<CompositorWorkerGlobalScope*>(scope);
  mutator_->RegisterCompositorAnimator(this);
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

bool CompositorWorkerProxyClientImpl::Mutate(double monotonic_time_now) {
  DCHECK(!IsMainThread());
  if (!global_scope_)
    return false;

  TRACE_EVENT0("compositor-worker", "CompositorWorkerProxyClientImpl::mutate");
  if (!requested_animation_frame_callbacks_)
    return false;

  requested_animation_frame_callbacks_ =
      ExecuteAnimationFrameCallbacks(monotonic_time_now);

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

// static
CompositorWorkerProxyClientImpl* CompositorWorkerProxyClientImpl::FromDocument(
    Document* document) {
  WebLocalFrameBase* local_frame_base =
      WebLocalFrameBase::FromFrame(document->GetFrame());
  return new CompositorWorkerProxyClientImpl(
      local_frame_base->LocalRootFrameWidget()->CompositorMutator());
}

}  // namespace blink
