// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/DeprecatedScheduleStyleRecalcDuringCompositingUpdate.h"

#include "wtf/Assertions.h"

namespace WebCore {

DeprecatedScheduleStyleRecalcDuringCompositingUpdate::DeprecatedScheduleStyleRecalcDuringCompositingUpdate(DocumentLifecycle& lifecycle)
    : m_lifecycle(lifecycle)
    , m_deprecatedTransition(lifecycle.state(), DocumentLifecycle::VisualUpdatePending)
    , m_originalState(lifecycle.state())
{
}

DeprecatedScheduleStyleRecalcDuringCompositingUpdate::~DeprecatedScheduleStyleRecalcDuringCompositingUpdate()
{
    if (m_originalState != DocumentLifecycle::InStyleRecalc
        && m_originalState != DocumentLifecycle::AfterPerformLayout
        && m_originalState != DocumentLifecycle::InCompositingUpdate)
        return;
    if (m_lifecycle.state() != m_originalState) {
        ASSERT(m_lifecycle.state() == DocumentLifecycle::VisualUpdatePending);
        m_lifecycle.advanceTo(m_originalState);
    }
}

}
