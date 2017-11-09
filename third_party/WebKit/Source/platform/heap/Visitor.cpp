// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/Visitor.h"

#include <memory>
#include "platform/heap/BlinkGC.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/VisitorImpl.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<Visitor> Visitor::Create(ThreadState* state, MarkingMode mode) {
  return std::make_unique<Visitor>(state, mode);
}

Visitor::Visitor(ThreadState* state, MarkingMode marking_mode)
    : state_(state), marking_mode_(marking_mode) {
  // See ThreadState::runScheduledGC() why we need to already be in a
  // GCForbiddenScope before any safe point is entered.
  DCHECK(state->IsGCForbidden());
#if DCHECK_IS_ON()
  DCHECK(state->CheckThread());
#endif
}

Visitor::~Visitor() {}

void Visitor::MarkNoTracingCallback(Visitor* visitor, void* object) {
  visitor->MarkNoTracing(object);
}

}  // namespace blink
