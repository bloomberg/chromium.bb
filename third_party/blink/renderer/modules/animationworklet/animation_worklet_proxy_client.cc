// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

namespace blink {

namespace {

static const wtf_size_t kMaxMutateCountToSwitch = 10u;
static const wtf_size_t kStatefulGlobalScopeIndex = 0u;

}  // end namespace

/* static */
const char AnimationWorkletProxyClient::kSupplementName[] =
    "AnimationWorkletProxyClient";

/* static */
const wtf_size_t AnimationWorkletProxyClient::kNumStatelessGlobalScopes = 2u;

AnimationWorkletProxyClient::AnimationWorkletProxyClient(
    int worklet_id,
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
        compositor_mutator_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_mutator_runner,
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
        main_thread_mutator_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_mutator_runner)
    : worklet_id_(worklet_id),
      state_(RunState::kUninitialized),
      next_global_scope_switch_countdown_(0),
      current_stateless_global_scope_index_(0) {
  DCHECK(IsMainThread());
  mutator_items_.emplace_back(std::move(compositor_mutator_dispatcher),
                              std::move(compositor_mutator_runner));
  mutator_items_.emplace_back(std::move(main_thread_mutator_dispatcher),
                              std::move(main_thread_mutator_runner));
}

void AnimationWorkletProxyClient::Trace(blink::Visitor* visitor) {
  Supplement<WorkerClients>::Trace(visitor);
  AnimationWorkletMutator::Trace(visitor);
}

void AnimationWorkletProxyClient::SynchronizeAnimatorName(
    const String& animator_name) {
  if (state_ == RunState::kDisposed)
    return;

  // Animator registration is processed before the loading promise being
  // resolved which is also done with a posted task (See
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished). Since both are
  // posted task and a SequencedTaskRunner is used, we are guaranteed that
  // registered names are synced before resolving the load promise therefore it
  // is safe to use a post task here.
  for (auto& mutator_item : mutator_items_) {
    DCHECK(mutator_item.mutator_runner);
    PostCrossThreadTask(
        *mutator_item.mutator_runner, FROM_HERE,
        CrossThreadBind(
            &AnimationWorkletMutatorDispatcherImpl::SynchronizeAnimatorName,
            mutator_item.mutator_dispatcher, animator_name));
  }
}

void AnimationWorkletProxyClient::AddGlobalScope(
    WorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  if (state_ == RunState::kDisposed)
    return;

  global_scopes_.push_back(To<AnimationWorkletGlobalScope>(global_scope));

  if (state_ != RunState::kUninitialized) {
    return;
  }

  // Wait for all global scopes to load before proceeding with registration.
  if (global_scopes_.size() < kNumStatelessGlobalScopes) {
    return;
  }

  // TODO(majidvp): Add an AnimationWorklet task type when the spec is final.
  scoped_refptr<base::SingleThreadTaskRunner> global_scope_runner =
      global_scope->GetThread()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  state_ = RunState::kWorking;

  for (auto& mutator_item : mutator_items_) {
    DCHECK(mutator_item.mutator_runner);
    PostCrossThreadTask(
        *mutator_item.mutator_runner, FROM_HERE,
        CrossThreadBind(&AnimationWorkletMutatorDispatcherImpl::
                            RegisterAnimationWorkletMutator,
                        mutator_item.mutator_dispatcher,
                        WrapCrossThreadPersistent(this), global_scope_runner));
  }
}

void AnimationWorkletProxyClient::Dispose() {
  if (state_ == RunState::kWorking) {
    // At worklet scope termination break the reference to the clients if it is
    // still alive.
    for (auto& mutator_item : mutator_items_) {
      DCHECK(mutator_item.mutator_runner);
      PostCrossThreadTask(
          *mutator_item.mutator_runner, FROM_HERE,
          CrossThreadBind(&AnimationWorkletMutatorDispatcherImpl::
                              UnregisterAnimationWorkletMutator,
                          mutator_item.mutator_dispatcher,
                          WrapCrossThreadPersistent(this)));
    }

    // At worklet scope termination break the reference cycle between
    // AnimationWorkletGlobalScope and AnimationWorkletProxyClient.
    global_scopes_.clear();
  }

  mutator_items_.clear();
  state_ = RunState::kDisposed;
}

std::unique_ptr<AnimationWorkletOutput> AnimationWorkletProxyClient::Mutate(
    std::unique_ptr<AnimationWorkletInput> input) {
  base::ElapsedTimer timer;
  DCHECK(input);
#if DCHECK_IS_ON()
  DCHECK(input->ValidateId(worklet_id_))
      << "Input has state that does not belong to this global scope: "
      << worklet_id_;
#endif

  // Create or destroy instances of animators on each global scope.
  for (auto global_scope : global_scopes_) {
    global_scope->UpdateAnimatorsList(*input);
  }

  std::unique_ptr<AnimationWorkletOutput> output =
      std::make_unique<AnimationWorkletOutput>();

  // Assume animators are stateful.
  // TODO(https://crbug.com/914918): Implement filter for detecting stateless
  // and stateful animators. Call mutate on stateful and stateless global
  // scopes with appropriate predicates.
  AnimationWorkletGlobalScope* stateful_global_scope =
      SelectStatefulGlobalScope();
  DCHECK(stateful_global_scope);
  stateful_global_scope->UpdateAnimators(*input, output.get(),
                                         [](Animator*) { return true; });

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Animation.AnimationWorklet.MutateDuration", timer.Elapsed(),
      base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
      50);

  return output;
}

AnimationWorkletGlobalScope*
AnimationWorkletProxyClient::SelectStatelessGlobalScope() {
  if (--next_global_scope_switch_countdown_ < 0) {
    current_stateless_global_scope_index_ =
        (++current_stateless_global_scope_index_ % global_scopes_.size());
    // Introduce an element of randomness in the switching interval to make
    // stateful dependences easier to spot.
    next_global_scope_switch_countdown_ =
        base::RandInt(0, kMaxMutateCountToSwitch - 1);
  }
  return global_scopes_[current_stateless_global_scope_index_];
}

AnimationWorkletGlobalScope*
AnimationWorkletProxyClient::SelectStatefulGlobalScope() {
  return global_scopes_[kStatefulGlobalScopeIndex];
}

void AnimationWorkletProxyClient::AddGlobalScopeForTesting(
    WorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  global_scopes_.push_back(To<AnimationWorkletGlobalScope>(global_scope));
}

// static
AnimationWorkletProxyClient* AnimationWorkletProxyClient::FromDocument(
    Document* document,
    int worklet_id) {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());

  scoped_refptr<base::SingleThreadTaskRunner> compositor_host_queue;
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
      compositor_mutator_dispatcher =
          local_frame->LocalRootFrameWidget()
              ->EnsureCompositorMutatorDispatcher(&compositor_host_queue);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_host_queue;
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
      main_thread_mutator_dispatcher =
          document->GetWorkletAnimationController()
              .EnsureMainThreadMutatorDispatcher(&main_thread_host_queue);

  return MakeGarbageCollected<AnimationWorkletProxyClient>(
      worklet_id, std::move(compositor_mutator_dispatcher),
      std::move(compositor_host_queue),
      std::move(main_thread_mutator_dispatcher),
      std::move(main_thread_host_queue));
}

AnimationWorkletProxyClient* AnimationWorkletProxyClient::From(
    WorkerClients* clients) {
  return Supplement<WorkerClients>::From<AnimationWorkletProxyClient>(clients);
}

void ProvideAnimationWorkletProxyClientTo(WorkerClients* clients,
                                          AnimationWorkletProxyClient* client) {
  clients->ProvideSupplement(client);
}

}  // namespace blink
