// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/Visitor.h"

#include "platform/heap/BlinkGC.h"
#include "platform/heap/MarkingVisitor.h"
#include "platform/heap/ThreadState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<Visitor> Visitor::create(ThreadState* state, MarkingMode mode) {
  return WTF::makeUnique<MarkingVisitor>(state, mode);
}

Visitor::Visitor(ThreadState* state, MarkingMode markingMode)
    : VisitorHelper(state), m_markingMode(markingMode) {
  // See ThreadState::runScheduledGC() why we need to already be in a
  // GCForbiddenScope before any safe point is entered.
  DCHECK(state->isGCForbidden());
#if ENABLE(ASSERT)
  DCHECK(state->checkThread());
#endif
}

Visitor::~Visitor() {}

}  // namespace blink
