// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintTiming_h
#define PaintTiming_h

#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;

class PaintTiming final : public NoBaseWillBeGarbageCollectedFinalized<PaintTiming>, public WillBeHeapSupplement<Document> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PaintTiming);
public:
    virtual ~PaintTiming() { }

    static PaintTiming& from(Document&);

    void markFirstPaint();
    void markFirstTextPaint();
    void markFirstImagePaint();

    // These return monotonically-increasing seconds.
    double firstPaint() const { return m_firstPaint; }
    double firstTextPaint() const { return m_firstTextPaint; }
    double firstImagePaint() const { return m_firstImagePaint; }

    DECLARE_VIRTUAL_TRACE();

private:
    explicit PaintTiming(Document&);
    LocalFrame* frame() const;
    void notifyPaintTimingChanged();

    double m_firstPaint = 0.0;
    double m_firstTextPaint = 0.0;
    double m_firstImagePaint = 0.0;

    RawPtrWillBeMember<Document> m_document;
};

}

#endif
