// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutatorClient.h"

#include <memory>
#include "base/bind.h"
#include "base/callback.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_impl.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

CompositorMutatorClient::CompositorMutatorClient(CompositorMutator* mutator)
    : mutator_(mutator) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CompositorMutatorClient::CompositorMutatorClient");
}

CompositorMutatorClient::~CompositorMutatorClient() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CompositorMutatorClient::~CompositorMutatorClient");
}

void CompositorMutatorClient::Mutate(
    base::TimeTicks monotonic_time,
    std::unique_ptr<cc::MutatorInputState> state) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::Mutate");
  double monotonic_time_now = (monotonic_time - base::TimeTicks()).InSecondsF();
  mutator_->Mutate(monotonic_time_now, std::move(state));
}

void CompositorMutatorClient::SetMutationUpdate(
    std::unique_ptr<cc::MutatorOutputState> output_state) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::SetMutationUpdate");
  client_->SetMutationUpdate(std::move(output_state));
}

void CompositorMutatorClient::SetClient(cc::LayerTreeMutatorClient* client) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::SetClient");
  client_ = client;
}

bool CompositorMutatorClient::HasAnimators() {
  return mutator_->HasAnimators();
}

}  // namespace blink
