// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/AnimationWorkletProxyClientImpl.h"

#include "core/dom/Document.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/CompositorMutatorImpl.h"

namespace blink {

AnimationWorkletProxyClientImpl::AnimationWorkletProxyClientImpl(
    base::WeakPtr<CompositorMutatorImpl> mutator,
    scoped_refptr<base::SingleThreadTaskRunner> mutator_runner)
    : mutator_(std::move(mutator)),
      mutator_runner_(std::move(mutator_runner)),
      state_(RunState::kUninitialized) {
  DCHECK(IsMainThread());
}

void AnimationWorkletProxyClientImpl::Trace(blink::Visitor* visitor) {
  AnimationWorkletProxyClient::Trace(visitor);
  CompositorAnimator::Trace(visitor);
}

void AnimationWorkletProxyClientImpl::SetGlobalScope(
    WorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  if (state_ == RunState::kDisposed)
    return;
  DCHECK(state_ == RunState::kUninitialized);

  global_scope_ = static_cast<AnimationWorkletGlobalScope*>(global_scope);
  // TODO(majidvp): Add an AnimationWorklet task type when the spec is final.
  scoped_refptr<base::SingleThreadTaskRunner> global_runner_ =
      global_scope_->GetThread()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  state_ = RunState::kWorking;
  DCHECK(mutator_runner_);
  PostCrossThreadTask(
      *mutator_runner_, FROM_HERE,
      CrossThreadBind(&CompositorMutatorImpl::RegisterCompositorAnimator,
                      mutator_, WrapCrossThreadPersistent(this),
                      global_runner_));
}

void AnimationWorkletProxyClientImpl::Dispose() {
  // At worklet scope termination break the reference to theClient from
  // the comositor if it is still alive.
  DCHECK(mutator_runner_);
  PostCrossThreadTask(
      *mutator_runner_, FROM_HERE,
      CrossThreadBind(&CompositorMutatorImpl::UnregisterCompositorAnimator,
                      mutator_, WrapCrossThreadPersistent(this)));
  mutator_runner_ = nullptr;
  DCHECK(state_ != RunState::kDisposed);
  state_ = RunState::kDisposed;

  DCHECK(global_scope_);
  DCHECK(global_scope_->IsContextThread());

  // At worklet scope termination break the reference cycle between
  // AnimationWorkletGlobalScope and AnimationWorkletProxyClientImpl.
  global_scope_ = nullptr;
}

std::unique_ptr<CompositorMutatorOutputState>
AnimationWorkletProxyClientImpl::Mutate(
    const CompositorMutatorInputState& input_state) {
  if (!global_scope_)
    return nullptr;

  auto output_state = global_scope_->Mutate(input_state);

  // TODO(petermayo): https://crbug.com/791280 PostCrossThreadTask to supply
  // this rather than return it.
  return output_state;
}

// static
AnimationWorkletProxyClientImpl* AnimationWorkletProxyClientImpl::FromDocument(
    Document* document) {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  scoped_refptr<base::SingleThreadTaskRunner> mutator_queue;
  base::WeakPtr<CompositorMutatorImpl> mutator =
      local_frame->LocalRootFrameWidget()->EnsureCompositorMutator(
          &mutator_queue);
  return new AnimationWorkletProxyClientImpl(std::move(mutator),
                                             std::move(mutator_queue));
}

}  // namespace blink
