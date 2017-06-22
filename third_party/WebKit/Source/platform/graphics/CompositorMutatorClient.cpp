// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutatorClient.h"

#include <memory>
#include "base/bind.h"
#include "base/callback.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_impl.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/CompositorMutationsTarget.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

CompositorMutatorClient::CompositorMutatorClient(
    CompositorMutator* mutator,
    CompositorMutationsTarget* mutations_target)
    : client_(nullptr),
      mutations_target_(mutations_target),
      mutator_(mutator),
      mutations_(nullptr) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorMutatorClient::CompositorMutatorClient");
}

CompositorMutatorClient::~CompositorMutatorClient() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorMutatorClient::~CompositorMutatorClient");
}

bool CompositorMutatorClient::Mutate(base::TimeTicks monotonic_time,
                                     cc::LayerTreeImpl* tree_impl) {
  // TODO(majidvp): At the moment we do not ever use the |tree_impl| or produce
  // any mutations. However we expect to use the tree and produce mutations for
  // AnimationWorklets. Remove these if that plan changes.
  TRACE_EVENT0("compositor-worker", "CompositorMutatorClient::Mutate");
  double monotonic_time_now = (monotonic_time - base::TimeTicks()).InSecondsF();
  bool should_reinvoke = mutator_->Mutate(monotonic_time_now);
  return should_reinvoke;
}

void CompositorMutatorClient::SetClient(cc::LayerTreeMutatorClient* client) {
  TRACE_EVENT0("compositor-worker", "CompositorMutatorClient::SetClient");
  client_ = client;
  SetNeedsMutate();
}

base::Closure CompositorMutatorClient::TakeMutations() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorMutatorClient::TakeMutations");
  if (!mutations_)
    return base::Closure();

  return base::Bind(&CompositorMutationsTarget::ApplyMutations,
                    base::Unretained(mutations_target_),
                    base::Owned(mutations_.release()));
}

void CompositorMutatorClient::SetNeedsMutate() {
  TRACE_EVENT0("compositor-worker", "CompositorMutatorClient::setNeedsMutate");
  client_->SetNeedsMutate();
}

void CompositorMutatorClient::SetMutationsForTesting(
    std::unique_ptr<CompositorMutations> mutations) {
  mutations_ = std::move(mutations);
}

}  // namespace blink
