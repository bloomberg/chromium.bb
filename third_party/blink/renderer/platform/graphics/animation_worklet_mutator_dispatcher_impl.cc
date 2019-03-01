// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/main_thread_mutator_client.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

namespace {

int g_next_async_mutation_id = 0;
int GetNextAsyncMutationId() {
  return g_next_async_mutation_id++;
}

}  // end namespace

// Wrap output vector in a thread safe and ref-counted object since it is
// accessed from animation worklet threads and its lifetime must be guaranteed
// to outlive the mutation update cycle.
class AnimationWorkletMutatorDispatcherImpl::OutputVectorRef
    : public ThreadSafeRefCounted<OutputVectorRef> {
 public:
  static scoped_refptr<OutputVectorRef> Create() {
    return base::AdoptRef(new OutputVectorRef());
  }
  Vector<std::unique_ptr<AnimationWorkletDispatcherOutput>>& get() {
    return vector_;
  }

 private:
  OutputVectorRef() = default;
  Vector<std::unique_ptr<AnimationWorkletDispatcherOutput>> vector_;
};

AnimationWorkletMutatorDispatcherImpl::AnimationWorkletMutatorDispatcherImpl(
    bool main_thread_task_runner)
    : client_(nullptr),
      outputs_(OutputVectorRef::Create()),
      weak_factory_(this) {
  // By default web tests run without threaded compositing. See
  // https://crbug.com/770028 For these situations we run on the Main thread.
  host_queue_ = main_thread_task_runner || !Thread::CompositorThread()
                    ? Thread::MainThread()->GetTaskRunner()
                    : Thread::CompositorThread()->GetTaskRunner();
}

AnimationWorkletMutatorDispatcherImpl::
    ~AnimationWorkletMutatorDispatcherImpl() {}

// static
template <typename ClientType>
std::unique_ptr<ClientType> AnimationWorkletMutatorDispatcherImpl::CreateClient(
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue,
    bool main_thread_client) {
  DCHECK(IsMainThread());
  auto mutator = std::make_unique<AnimationWorkletMutatorDispatcherImpl>(
      main_thread_client);
  // This is allowed since we own the class for the duration of creation.
  *weak_interface = mutator->weak_factory_.GetWeakPtr();
  *queue = mutator->GetTaskRunner();

  return std::make_unique<ClientType>(std::move(mutator));
}

// static
std::unique_ptr<CompositorMutatorClient>
AnimationWorkletMutatorDispatcherImpl::CreateCompositorThreadClient(
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue) {
  return CreateClient<CompositorMutatorClient>(weak_interface, queue, false);
}

// static
std::unique_ptr<MainThreadMutatorClient>
AnimationWorkletMutatorDispatcherImpl::CreateMainThreadClient(
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue) {
  return CreateClient<MainThreadMutatorClient>(weak_interface, queue, true);
}

void AnimationWorkletMutatorDispatcherImpl::MutateSynchronously(
    std::unique_ptr<AnimationWorkletDispatcherInput> mutator_input) {
  TRACE_EVENT0("cc", "AnimationWorkletMutatorDispatcherImpl::mutate");
  if (mutator_map_.IsEmpty() || !mutator_input)
    return;
  base::ElapsedTimer timer;
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  DCHECK(mutator_input_map_.IsEmpty());
  DCHECK(outputs_->get().IsEmpty());

  mutator_input_map_ = CreateInputMap(*mutator_input);
  if (mutator_input_map_.IsEmpty())
    return;

  base::WaitableEvent event;
  WTF::CrossThreadClosure on_done = CrossThreadBind(
      &base::WaitableEvent::Signal, WTF::CrossThreadUnretained(&event));
  RequestMutations(std::move(on_done));
  event.Wait();

  ApplyMutationsOnHostThread();

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Animation.AnimationWorklet.Dispatcher.SynchronousMutateDuration",
      timer.Elapsed(), base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMilliseconds(100), 50);
}

bool AnimationWorkletMutatorDispatcherImpl::MutateAsynchronously(
    std::unique_ptr<AnimationWorkletDispatcherInput> mutator_input,
    MutateQueuingStrategy queuing_strategy,
    AsyncMutationCompleteCallback done_callback) {
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  if (mutator_map_.IsEmpty() || !mutator_input)
    return false;

  if (!mutator_input_map_.IsEmpty()) {
    // Still running mutations from a previous frame.
    if (queuing_strategy == MutateQueuingStrategy::kDrop) {
      // Skip this frame to avoid lagging behind.
      return false;
    }
    DCHECK(queuing_strategy == MutateQueuingStrategy::kQueueAndReplace);
    DCHECK(!queued_mutator_input_);
    // Preemptive queue.
    queued_mutator_input_.reset(mutator_input.release());
    queued_on_async_mutation_complete_ = std::move(done_callback);
    return true;
  }

  mutator_input_map_ = CreateInputMap(*mutator_input);
  if (mutator_input_map_.IsEmpty())
    return false;

  MutateAsynchronouslyInternal(std::move(done_callback));
  return true;
}

void AnimationWorkletMutatorDispatcherImpl::MutateAsynchronouslyInternal(
    AsyncMutationCompleteCallback done_callback) {
  DCHECK(host_queue_->BelongsToCurrentThread());
  on_async_mutation_complete_ = std::move(done_callback);
  int next_async_mutation_id = GetNextAsyncMutationId();
  TRACE_EVENT_ASYNC_BEGIN0("cc",
                           "AnimationWorkletMutatorDispatcherImpl::MutateAsync",
                           next_async_mutation_id);

  WTF::CrossThreadClosure on_done = CrossThreadBind(
      [](scoped_refptr<base::SingleThreadTaskRunner> host_queue,
         base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> dispatcher,
         int next_async_mutation_id) {
        PostCrossThreadTask(
            *host_queue, FROM_HERE,
            CrossThreadBind(
                &AnimationWorkletMutatorDispatcherImpl::AsyncMutationsDone,
                dispatcher, next_async_mutation_id));
      },
      host_queue_, weak_factory_.GetWeakPtr(), next_async_mutation_id);

  RequestMutations(std::move(on_done));
}

void AnimationWorkletMutatorDispatcherImpl::AsyncMutationsDone(
    int async_mutation_id) {
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  bool update_applied = ApplyMutationsOnHostThread();
  auto done_callback = std::move(on_async_mutation_complete_);
  if (queued_mutator_input_.get()) {
    mutator_input_map_ = CreateInputMap(*queued_mutator_input_);
    queued_mutator_input_.reset();
    // Trigger queued mutation request.
    MutateAsynchronouslyInternal(std::move(queued_on_async_mutation_complete_));
  }
  TRACE_EVENT_ASYNC_END0("cc",
                         "AnimationWorkletMutatorDispatcherImpl::MutateAsync",
                         async_mutation_id);
  std::move(done_callback)
      .Run(update_applied ? MutateStatus::kCompletedWithUpdate
                          : MutateStatus::kCompletedNoUpdate);
  // TODO(kevers): Add UMA metric to track the asynchronous mutate duration.
}

void AnimationWorkletMutatorDispatcherImpl::RegisterAnimationWorkletMutator(
    CrossThreadPersistent<AnimationWorkletMutator> mutator,
    scoped_refptr<base::SingleThreadTaskRunner> mutator_runner) {
  TRACE_EVENT0(
      "cc",
      "AnimationWorkletMutatorDispatcherImpl::RegisterAnimationWorkletMutator");

  DCHECK(mutator);
  DCHECK(host_queue_->BelongsToCurrentThread());

  mutator_map_.insert(mutator, mutator_runner);
}

void AnimationWorkletMutatorDispatcherImpl::UnregisterAnimationWorkletMutator(
    CrossThreadPersistent<AnimationWorkletMutator> mutator) {
  TRACE_EVENT0("cc",
               "AnimationWorkletMutatorDispatcherImpl::"
               "UnregisterAnimationWorkletMutator");
  DCHECK(mutator);
  DCHECK(host_queue_->BelongsToCurrentThread());

  mutator_map_.erase(mutator);
}

void AnimationWorkletMutatorDispatcherImpl::SynchronizeAnimatorName(
    const String& animator_name) {
  client_->SynchronizeAnimatorName(animator_name);
}

bool AnimationWorkletMutatorDispatcherImpl::HasMutators() {
  return !mutator_map_.IsEmpty();
}

AnimationWorkletMutatorDispatcherImpl::InputMap
AnimationWorkletMutatorDispatcherImpl::CreateInputMap(
    AnimationWorkletDispatcherInput& mutator_input) const {
  InputMap input_map;
  for (const auto& pair : mutator_map_) {
    AnimationWorkletMutator* mutator = pair.key;
    const int worklet_id = mutator->GetWorkletId();
    std::unique_ptr<AnimationWorkletInput> input =
        mutator_input.TakeWorkletState(worklet_id);
    if (input) {
      input_map.insert(worklet_id, std::move(input));
    }
  }
  return input_map;
}

void AnimationWorkletMutatorDispatcherImpl::RequestMutations(
    WTF::CrossThreadClosure done_callback) {
  DCHECK(client_);
  DCHECK(outputs_->get().IsEmpty());

  int num_requests = mutator_map_.size();
  if (num_requests == 0) {
    std::move(done_callback).Run();
    return;
  }

  int next_request_index = 0;
  outputs_->get().Grow(num_requests);
  base::RepeatingClosure on_mutator_done = base::BarrierClosure(
      num_requests, ConvertToBaseCallback(std::move(done_callback)));

  for (const auto& pair : mutator_map_) {
    AnimationWorkletMutator* mutator = pair.key;
    scoped_refptr<base::SingleThreadTaskRunner> worklet_queue = pair.value;
    int worklet_id = mutator->GetWorkletId();
    DCHECK(!worklet_queue->BelongsToCurrentThread());

    // Wrap the barrier closure in a ScopedClosureRunner to guarantee it runs
    // even if the posted task does not run.
    auto on_done_runner =
        std::make_unique<base::ScopedClosureRunner>(on_mutator_done);

    auto it = mutator_input_map_.find(worklet_id);
    if (it == mutator_input_map_.end()) {
      // Here the on_done_runner goes out of scope which causes the barrier
      // closure to run.
      continue;
    }

    PostCrossThreadTask(
        *worklet_queue, FROM_HERE,
        CrossThreadBind(
            [](AnimationWorkletMutator* mutator,
               std::unique_ptr<AnimationWorkletInput> input,
               scoped_refptr<OutputVectorRef> outputs, int index,
               std::unique_ptr<base::ScopedClosureRunner> on_done_runner) {
              std::unique_ptr<AnimationWorkletOutput> output =
                  mutator ? mutator->Mutate(std::move(input)) : nullptr;
              outputs->get()[index] = std::move(output);
              on_done_runner->RunAndReset();
            },
            // The mutator is created and destroyed on the worklet thread.
            WrapCrossThreadWeakPersistent(mutator),
            // The worklet input is not required after the Mutate call.
            WTF::Passed(std::move(it->value)),
            // The vector of outputs is wrapped in a scoped_refptr initialized
            // on the host thread. It can outlive the dispatcher during shutdown
            // of a process with a running animation.
            outputs_, next_request_index++,
            WTF::Passed(std::move(on_done_runner))));
  }
}

bool AnimationWorkletMutatorDispatcherImpl::ApplyMutationsOnHostThread() {
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  bool update_applied = false;
  for (auto& output : outputs_->get()) {
    if (output) {
      client_->SetMutationUpdate(std::move(output));
      update_applied = true;
    }
  }
  mutator_input_map_.clear();
  outputs_->get().clear();
  return update_applied;
}

}  // namespace blink
