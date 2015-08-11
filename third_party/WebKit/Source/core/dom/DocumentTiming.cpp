// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DocumentTiming.h"

#include "platform/TraceEvent.h"

namespace blink {

DocumentTiming::DocumentTiming()
    : m_domLoading(0.0)
    , m_domInteractive(0.0)
    , m_domContentLoadedEventStart(0.0)
    , m_domContentLoadedEventEnd(0.0)
    , m_domComplete(0.0)
    , m_firstLayout(0.0)
{
}

void DocumentTiming::markDomLoading()
{
    m_domLoading = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domLoading", m_domLoading);
}

void DocumentTiming::markDomInteractive()
{
    m_domInteractive = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domInteractive", m_domInteractive);
}

void DocumentTiming::markDomContentLoadedEventStart()
{
    m_domContentLoadedEventStart = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domContentLoadedEventStart", m_domContentLoadedEventStart);
}

void DocumentTiming::markDomContentLoadedEventEnd()
{
    m_domContentLoadedEventEnd = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domContentLoadedEventEnd", m_domContentLoadedEventEnd);
}

void DocumentTiming::markDomComplete()
{
    m_domComplete = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domComplete", m_domComplete);
}

void DocumentTiming::markFirstLayout()
{
    m_firstLayout = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "firstLayout", m_firstLayout);
}

} // namespace blink
