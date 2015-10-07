// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DocumentTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/TraceEvent.h"
#include "wtf/RawPtr.h"

namespace blink {

DocumentTiming::DocumentTiming(Document& document)
    : m_domLoading(0.0)
    , m_domInteractive(0.0)
    , m_domContentLoadedEventStart(0.0)
    , m_domContentLoadedEventEnd(0.0)
    , m_domComplete(0.0)
    , m_firstLayout(0.0)
    , m_firstTextPaint(0.0)
    , m_document(document)
{
}

DEFINE_TRACE(DocumentTiming)
{
    visitor->trace(m_document);
}

void DocumentTiming::notifyDocumentTimingChanged()
{
    if (m_document && m_document->loader())
        m_document->loader()->didChangePerformanceTiming();
}

void DocumentTiming::markDomLoading()
{
    m_domLoading = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domLoading", m_domLoading);
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomInteractive()
{
    m_domInteractive = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domInteractive", m_domInteractive);
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomContentLoadedEventStart()
{
    m_domContentLoadedEventStart = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domContentLoadedEventStart", m_domContentLoadedEventStart);
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomContentLoadedEventEnd()
{
    m_domContentLoadedEventEnd = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domContentLoadedEventEnd", m_domContentLoadedEventEnd);
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomComplete()
{
    m_domComplete = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domComplete", m_domComplete);
    notifyDocumentTimingChanged();
}

void DocumentTiming::markFirstLayout()
{
    m_firstLayout = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "firstLayout", m_firstLayout);
    notifyDocumentTimingChanged();
}

void DocumentTiming::markFirstTextPaint()
{
    m_firstTextPaint = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "firstTextPaint", m_firstTextPaint);
    notifyDocumentTimingChanged();
}

} // namespace blink
