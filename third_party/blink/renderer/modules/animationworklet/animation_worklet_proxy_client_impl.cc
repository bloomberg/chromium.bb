// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client_impl.h"

#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/worklet_mutator_impl.h"

namespace blink {

AnimationWorkletProxyClientImpl::AnimationWorkletProxyClientImpl(
    int scope_id,
    base::WeakPtr<WorkletMutatorImpl> compositor_mutator,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_mutator_runner,
    base::WeakPtr<WorkletMutatorImpl> main_thread_mutator,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_mutator_runner)
    : scope_id_(scope_id), state_(RunState::kUninitialized) {
  DCHECK(IsMainThread());
  mutator_items_.emplace_back(std::move(compositor_mutator),
                              std::move(compositor_mutator_runner));
  mutator_items_.emplace_back(std::move(main_thread_mutator),
                              std::move(main_thread_mutator_runner));
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
  scoped_refptr<base::SingleThreadTaskRunner> global_scope_runner =
      global_scope_->GetThread()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  state_ = RunState::kWorking;

  for (auto& mutator_item : mutator_items_) {
    DCHECK(mutator_item.mutator_runner);
    PostCrossThreadTask(
        *mutator_item.mutator_runner, FROM_HERE,
        CrossThreadBind(&WorkletMutatorImpl::RegisterCompositorAnimator,
                        mutator_item.mutator, WrapCrossThreadPersistent(this),
                        global_scope_runner));
  }
}

void AnimationWorkletProxyClientImpl::Dispose() {
  if (state_ == RunState::kWorking) {
    // At worklet scope termination break the reference to the clients if it is
    // still alive.
    for (auto& mutator_item : mutator_items_) {
      DCHECK(mutator_item.mutator_runner);
      PostCrossThreadTask(
          *mutator_item.mutator_runner, FROM_HERE,
          CrossThreadBind(&WorkletMutatorImpl::UnregisterCompositorAnimator,
                          mutator_item.mutator,
                          WrapCrossThreadPersistent(this)));
    }

    DCHECK(global_scope_);
    DCHECK(global_scope_->IsContextThread());

    // At worklet scope termination break the reference cycle between
    // AnimationWorkletGlobalScope and AnimationWorkletProxyClientImpl.
    global_scope_ = nullptr;
  }

  mutator_items_.clear();

  DCHECK(state_ != RunState::kDisposed);
  state_ = RunState::kDisposed;
}

std::unique_ptr<AnimationWorkletOutput> AnimationWorkletProxyClientImpl::Mutate(
    std::unique_ptr<AnimationWorkletInput> input) {
  DCHECK(input);
#if DCHECK_IS_ON()
  DCHECK(input->ValidateScope(scope_id_))
      << "Input has state that does not belong to this global scope: "
      << scope_id_;
#endif

  if (!global_scope_)
    return nullptr;

  auto output = global_scope_->Mutate(*input);

  // TODO(petermayo): https://crbug.com/791280 PostCrossThreadTask to supply
  // this rather than return it.
  return output;
}

// static
AnimationWorkletProxyClientImpl* AnimationWorkletProxyClientImpl::FromDocument(
    Document* document,
    int scope_id) {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());

  scoped_refptr<base::SingleThreadTaskRunner> compositor_mutator_queue;
  base::WeakPtr<WorkletMutatorImpl> compositor_mutator =
      local_frame->LocalRootFrameWidget()->EnsureCompositorMutator(
          &compositor_mutator_queue);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_mutator_queue;
  base::WeakPtr<WorkletMutatorImpl> main_thread_mutator =
      document->GetWorkletAnimationController().EnsureMainThreadMutator(
          &main_thread_mutator_queue);

  return new AnimationWorkletProxyClientImpl(
      scope_id, std::move(compositor_mutator),
      std::move(compositor_mutator_queue), std::move(main_thread_mutator),
      std::move(main_thread_mutator_queue));
}

}  // namespace blink
