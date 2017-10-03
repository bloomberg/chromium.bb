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
    : client_(nullptr), mutator_(mutator) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CompositorMutatorClient::CompositorMutatorClient");
}

CompositorMutatorClient::~CompositorMutatorClient() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CompositorMutatorClient::~CompositorMutatorClient");
}

bool CompositorMutatorClient::Mutate(base::TimeTicks monotonic_time) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::Mutate");
  double monotonic_time_now = (monotonic_time - base::TimeTicks()).InSecondsF();
  bool should_reinvoke = mutator_->Mutate(monotonic_time_now);
  return should_reinvoke;
}

void CompositorMutatorClient::SetClient(cc::LayerTreeMutatorClient* client) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::SetClient");
  client_ = client;
  SetNeedsMutate();
}

void CompositorMutatorClient::SetNeedsMutate() {
  TRACE_EVENT0("cc", "CompositorMutatorClient::setNeedsMutate");
  client_->SetNeedsMutate();
}

}  // namespace blink
