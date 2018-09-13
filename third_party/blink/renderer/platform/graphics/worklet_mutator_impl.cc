// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/worklet_mutator_impl.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/compositor_animator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/main_thread_mutator_client.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

WorkletMutatorImpl::WorkletMutatorImpl(bool main_thread_task_runner)
    : client_(nullptr), weak_factory_(this) {
  // By default layout tests run without threaded compositing. See
  // https://crbug.com/770028 For these situations we run on the Main thread.
  mutator_queue_ =
      main_thread_task_runner || !Platform::Current()->CompositorThread()
          ? Platform::Current()->MainThread()->GetTaskRunner()
          : Platform::Current()->CompositorThread()->GetTaskRunner();
}

WorkletMutatorImpl::~WorkletMutatorImpl() {}

// static
template <typename ClientType>
std::unique_ptr<ClientType> WorkletMutatorImpl::CreateClient(
    base::WeakPtr<WorkletMutatorImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue,
    bool main_thread_client) {
  DCHECK(IsMainThread());
  auto mutator = std::make_unique<WorkletMutatorImpl>(main_thread_client);
  // This is allowed since we own the class for the duration of creation.
  *weak_interface = mutator->weak_factory_.GetWeakPtr();
  *queue = mutator->GetTaskRunner();

  return std::make_unique<ClientType>(std::move(mutator));
}

// static
std::unique_ptr<CompositorMutatorClient>
WorkletMutatorImpl::CreateCompositorThreadClient(
    base::WeakPtr<WorkletMutatorImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue) {
  return CreateClient<CompositorMutatorClient>(weak_interface, queue, false);
}

// static
std::unique_ptr<MainThreadMutatorClient>
WorkletMutatorImpl::CreateMainThreadClient(
    base::WeakPtr<WorkletMutatorImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue) {
  return CreateClient<MainThreadMutatorClient>(weak_interface, queue, true);
}

void WorkletMutatorImpl::Mutate(
    std::unique_ptr<CompositorMutatorInputState> mutator_input) {
  TRACE_EVENT0("cc", "WorkletMutatorImpl::mutate");
  if (animator_map_.IsEmpty())
    return;
  DCHECK(client_);

  Vector<std::unique_ptr<CompositorMutatorOutputState>> outputs(
      animator_map_.size());
  Vector<WaitableEvent> done_events(animator_map_.size());

  int index = 0;
  for (auto& pair : animator_map_) {
    CompositorAnimator* animator = pair.key;
    scoped_refptr<base::SingleThreadTaskRunner> animator_queue = pair.value;

    std::unique_ptr<AnimationWorkletInput> animator_input =
        mutator_input->TakeWorkletState(animator->GetScopeId());

    DCHECK(!animator_queue->BelongsToCurrentThread());
    std::unique_ptr<AutoSignal> done =
        std::make_unique<AutoSignal>(&done_events[index]);
    std::unique_ptr<CompositorMutatorOutputState>& output = outputs[index];

    if (animator_input) {
      PostCrossThreadTask(
          *animator_queue, FROM_HERE,
          CrossThreadBind(
              [](CompositorAnimator* animator,
                 std::unique_ptr<AnimationWorkletInput> input,
                 std::unique_ptr<AutoSignal> completion,
                 std::unique_ptr<CompositorMutatorOutputState>* output) {
                *output = animator->Mutate(std::move(input));
              },
              WrapCrossThreadWeakPersistent(animator),
              WTF::Passed(std::move(animator_input)),
              WTF::Passed(std::move(done)), CrossThreadUnretained(&output)));
    }
    index++;
  }

  for (WaitableEvent& event : done_events) {
    event.Wait();
  }

  for (auto& output : outputs) {
    // Animator that has no input does not produce any output.
    if (!output)
      continue;
    client_->SetMutationUpdate(std::move(output));
  }
}

void WorkletMutatorImpl::RegisterCompositorAnimator(
    CrossThreadPersistent<CompositorAnimator> animator,
    scoped_refptr<base::SingleThreadTaskRunner> animator_runner) {
  TRACE_EVENT0("cc", "WorkletMutatorImpl::RegisterCompositorAnimator");

  DCHECK(animator);
  DCHECK(mutator_queue_->BelongsToCurrentThread());

  animator_map_.insert(animator, animator_runner);
}

void WorkletMutatorImpl::UnregisterCompositorAnimator(
    CrossThreadPersistent<CompositorAnimator> animator) {
  TRACE_EVENT0("cc", "WorkletMutatorImpl::UnregisterCompositorAnimator");
  DCHECK(animator);
  DCHECK(mutator_queue_->BelongsToCurrentThread());

  animator_map_.erase(animator);
}

bool WorkletMutatorImpl::HasAnimators() {
  return !animator_map_.IsEmpty();
}

WorkletMutatorImpl::AutoSignal::AutoSignal(WaitableEvent* event)
    : event_(event) {
  DCHECK(event);
}

WorkletMutatorImpl::AutoSignal::~AutoSignal() {
  event_->Signal();
}

}  // namespace blink
