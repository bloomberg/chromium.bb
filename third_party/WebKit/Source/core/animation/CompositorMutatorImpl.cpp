// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CompositorMutatorImpl.h"

#include "core/animation/CompositorAnimator.h"
#include "core/animation/CustomCompositorAnimationManager.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/CompositorMutationsTarget.h"
#include "platform/graphics/CompositorMutatorClient.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

void CreateCompositorMutatorClient(
    std::unique_ptr<CompositorMutatorClient>* ptr,
    WaitableEvent* done_event) {
  CompositorMutatorImpl* mutator = CompositorMutatorImpl::Create();
  ptr->reset(new CompositorMutatorClient(mutator, mutator->AnimationManager()));
  mutator->SetClient(ptr->get());
  done_event->Signal();
}

}  // namespace

CompositorMutatorImpl::CompositorMutatorImpl()
    : animation_manager_(WTF::WrapUnique(new CustomCompositorAnimationManager)),
      client_(nullptr) {}

std::unique_ptr<CompositorMutatorClient> CompositorMutatorImpl::CreateClient() {
  std::unique_ptr<CompositorMutatorClient> mutator_client;
  WaitableEvent done_event;
  if (WebThread* compositor_thread = Platform::Current()->CompositorThread()) {
    compositor_thread->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&CreateCompositorMutatorClient,
                                         CrossThreadUnretained(&mutator_client),
                                         CrossThreadUnretained(&done_event)));
  } else {
    CreateCompositorMutatorClient(&mutator_client, &done_event);
  }
  // TODO(flackr): Instead of waiting for this event, we may be able to just set
  // the mutator on the CompositorWorkerProxyClient directly from the compositor
  // thread before it gets used there. We still need to make sure we only
  // create one mutator though.
  done_event.Wait();
  return mutator_client;
}

CompositorMutatorImpl* CompositorMutatorImpl::Create() {
  return new CompositorMutatorImpl();
}

bool CompositorMutatorImpl::Mutate(double monotonic_time_now) {
  TRACE_EVENT0("cc", "CompositorMutatorImpl::mutate");
  bool need_to_reinvoke = false;
  for (CompositorAnimator* animator : animators_) {
    if (animator->Mutate(monotonic_time_now))
      need_to_reinvoke = true;
  }

  return need_to_reinvoke;
}

void CompositorMutatorImpl::RegisterCompositorAnimator(
    CompositorAnimator* animator) {
  DCHECK(!IsMainThread());
  TRACE_EVENT0("cc", "CompositorMutatorImpl::registerCompositorAnimator");
  DCHECK(!animators_.Contains(animator));
  animators_.insert(animator);
  SetNeedsMutate();
}

void CompositorMutatorImpl::UnregisterCompositorAnimator(
    CompositorAnimator* animator) {
  DCHECK(animators_.Contains(animator));
  animators_.erase(animator);
}

void CompositorMutatorImpl::SetNeedsMutate() {
  DCHECK(!IsMainThread());
  TRACE_EVENT0("cc", "CompositorMutatorImpl::setNeedsMutate");
  client_->SetNeedsMutate();
}

}  // namespace blink
