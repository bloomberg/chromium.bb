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

Visitor::Visitor(ThreadState* state) : state_(state) {}

Visitor::~Visitor() = default;

std::unique_ptr<MarkingVisitor> MarkingVisitor::Create(ThreadState* state,
                                                       MarkingMode mode) {
  return std::make_unique<MarkingVisitor>(state, mode);
}

MarkingVisitor::MarkingVisitor(ThreadState* state, MarkingMode marking_mode)
    : Visitor(state), marking_mode_(marking_mode) {
  // See ThreadState::runScheduledGC() why we need to already be in a
  // GCForbiddenScope before any safe point is entered.
  DCHECK(state->IsGCForbidden());
#if DCHECK_IS_ON()
  DCHECK(state->CheckThread());
#endif
}

MarkingVisitor::~MarkingVisitor() = default;

void MarkingVisitor::MarkNoTracingCallback(Visitor* visitor, void* object) {
  // TODO(mlippautz): Remove cast;
  reinterpret_cast<MarkingVisitor*>(visitor)->MarkNoTracing(object);
}

}  // namespace blink
