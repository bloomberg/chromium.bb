// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

#include "base/barrier_closure.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/main_thread_mutator_client.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/waitable_event.h"

namespace blink {

namespace {

int next_async_mutation_id = 0;
int GetNextAsyncMutationId() {
  return next_async_mutation_id++;
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

  WaitableEvent event;
  WTF::CrossThreadClosure on_done = CrossThreadBind(
      &WaitableEvent::Signal, WTF::CrossThreadUnretained(&event));
  RequestMutations(std::move(on_done));
  event.Wait();

  ApplyMutationsOnHostThread();

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Animation.AnimationWorklet.Dispatcher.SynchronousMutateDuration",
      timer.Elapsed(), base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMilliseconds(100), 50);
}

void AnimationWorkletMutatorDispatcherImpl::MutateAsynchronously(
    std::unique_ptr<AnimationWorkletDispatcherInput> mutator_input) {
  if (mutator_map_.IsEmpty() || !mutator_input)
    return;
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  if (!mutator_input_map_.IsEmpty()) {
    // Still running mutations from a previous frame. Skip this frame to avoid
    // lagging behind.
    // TODO(kevers): Consider queuing pending mutation cycle. A pending tree
    // mutation should likely be queued an active tree mutation cycle is still
    // running.
    return;
  }

  mutator_input_map_ = CreateInputMap(*mutator_input);
  if (mutator_input_map_.IsEmpty())
    return;

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

  client_->NotifyAnimationsPending();
  RequestMutations(std::move(on_done));
}

void AnimationWorkletMutatorDispatcherImpl::AsyncMutationsDone(
    int async_mutation_id) {
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  ApplyMutationsOnHostThread();
  client_->NotifyAnimationsReady();
  TRACE_EVENT_ASYNC_END0("cc",
                         "AnimationWorkletMutatorDispatcherImpl::MutateAsync",
                         async_mutation_id);
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
  int next_request_index = 0;
  outputs_->get().Grow(num_requests);
  base::RepeatingClosure on_mutator_done = base::BarrierClosure(
      num_requests, ConvertToBaseCallback(std::move(done_callback)));

  for (const auto& pair : mutator_map_) {
    AnimationWorkletMutator* mutator = pair.key;
    scoped_refptr<base::SingleThreadTaskRunner> worklet_queue = pair.value;
    int worklet_id = mutator->GetWorkletId();
    DCHECK(!worklet_queue->BelongsToCurrentThread());
    auto it = mutator_input_map_.find(worklet_id);
    if (it == mutator_input_map_.end()) {
      // No input to process.
      on_mutator_done.Run();
      continue;
    }
    PostCrossThreadTask(
        *worklet_queue, FROM_HERE,
        CrossThreadBind(
            [](AnimationWorkletMutator* mutator,
               std::unique_ptr<AnimationWorkletInput> input,
               scoped_refptr<OutputVectorRef> outputs, int index,
               WTF::CrossThreadClosure on_mutator_done) {
              std::unique_ptr<AnimationWorkletOutput> output =
                  mutator ? mutator->Mutate(std::move(input)) : nullptr;
              outputs->get()[index] = std::move(output);
              on_mutator_done.Run();
            },
            // The mutator is created and destroyed on the worklet thread.
            WrapCrossThreadWeakPersistent(mutator),
            // The worklet input is not required after the Mutate call.
            WTF::Passed(std::move(it->value)),
            // The vector of outputs is wrapped in a scoped_refptr initialized
            // on the host thread. It can outlive the dispatcher during shutdown
            // of a process with a running animation.
            outputs_, next_request_index++,
            WTF::Passed(WTF::CrossThreadClosure(on_mutator_done))));
  }
}

void AnimationWorkletMutatorDispatcherImpl::ApplyMutationsOnHostThread() {
  DCHECK(client_);
  DCHECK(host_queue_->BelongsToCurrentThread());
  for (auto& output : outputs_->get()) {
    if (output)
      client_->SetMutationUpdate(std::move(output));
  }
  mutator_input_map_.clear();
  outputs_->get().clear();
}

}  // namespace blink
