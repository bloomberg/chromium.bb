// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/TraceEvent.h"
#include "wtf/RawPtr.h"

namespace blink {

static const char kSupplementName[] = "PaintTiming";

PaintTiming& PaintTiming::from(Document& document)
{
    PaintTiming* timing = static_cast<PaintTiming*>(WillBeHeapSupplement<Document>::from(document, kSupplementName));
    if (!timing) {
        timing = new PaintTiming(document);
        WillBeHeapSupplement<Document>::provideTo(document, kSupplementName, adoptPtrWillBeNoop(timing));
    }
    return *timing;
}

PaintTiming::PaintTiming(Document& document)
    : m_document(document)
{
}

DEFINE_TRACE(PaintTiming)
{
    visitor->trace(m_document);
}

LocalFrame* PaintTiming::frame() const
{
    return m_document ? m_document->frame() : nullptr;
}

void PaintTiming::notifyPaintTimingChanged()
{
    if (m_document && m_document->loader())
        m_document->loader()->didChangePerformanceTiming();
}

void PaintTiming::markFirstPaint()
{
    if (m_firstPaint != 0.0)
        return;
    m_firstPaint = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstPaint", m_firstPaint, "frame", frame());
    notifyPaintTimingChanged();
}

void PaintTiming::markFirstTextPaint()
{
    if (m_firstTextPaint != 0.0)
        return;
    m_firstTextPaint = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstTextPaint", m_firstTextPaint, "frame", frame());

    if (firstContentfulPaint() == 0.0)
        markFirstContentfulPaint();
    else
        notifyPaintTimingChanged();
}

void PaintTiming::markFirstImagePaint()
{
    if (m_firstImagePaint != 0.0)
        return;
    m_firstImagePaint = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstImagePaint", m_firstImagePaint, "frame", frame());

    if (firstContentfulPaint() == 0.0)
        markFirstContentfulPaint();
    else
        notifyPaintTimingChanged();
}

void PaintTiming::markFirstContentfulPaint()
{
    if (m_firstContentfulPaint != 0.0)
        return;
    m_firstContentfulPaint = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstContentfulPaint", m_firstContentfulPaint, "frame", frame());
    notifyPaintTimingChanged();
}

} // namespace blink
