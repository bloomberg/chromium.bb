// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/Visitor.h"

#include "platform/heap/BlinkGC.h"
#include "platform/heap/MarkingVisitor.h"
#include "platform/heap/ThreadState.h"

namespace blink {

VisitorScope::VisitorScope(ThreadState* state, BlinkGC::GCType gcType)
    : m_state(state)
{
    // See ThreadState::runScheduledGC() why we need to already be in a
    // GCForbiddenScope before any safe point is entered.
    m_state->enterGCForbiddenScope();

    ASSERT(m_state->checkThread());

    switch (gcType) {
    case BlinkGC::GCWithSweep:
    case BlinkGC::GCWithoutSweep:
        m_visitor = adoptPtr(new MarkingVisitor<Visitor::GlobalMarking>());
        break;
    case BlinkGC::TakeSnapshot:
        m_visitor = adoptPtr(new MarkingVisitor<Visitor::SnapshotMarking>());
        break;
    case BlinkGC::ThreadTerminationGC:
        m_visitor = adoptPtr(new MarkingVisitor<Visitor::ThreadLocalMarking>());
        break;
    case BlinkGC::ThreadLocalWeakProcessing:
        m_visitor = adoptPtr(new MarkingVisitor<Visitor::WeakProcessing>());
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

VisitorScope::~VisitorScope()
{
    m_state->leaveGCForbiddenScope();
}

} // namespace blink
