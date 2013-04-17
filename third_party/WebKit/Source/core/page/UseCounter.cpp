/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UseCounter.h"

#include "DOMWindow.h"
#include "Document.h"
#include "HistogramSupport.h"
#include "Page.h"

namespace WebCore {

UseCounter::UseCounter()
{
}

UseCounter::~UseCounter()
{
    // We always log PageDestruction so that we have a scale for the rest of the features.
    HistogramSupport::histogramEnumeration("WebCore.FeatureObserver", PageDestruction, NumberOfFeatures);

    updateMeasurements();
}

void UseCounter::updateMeasurements()
{
    HistogramSupport::histogramEnumeration("WebCore.FeatureObserver", PageVisits, NumberOfFeatures);

    if (!m_countBits)
        return;

    for (unsigned i = 0; i < NumberOfFeatures; ++i) {
        if (m_countBits->quickGet(i))
            HistogramSupport::histogramEnumeration("WebCore.FeatureObserver", i, NumberOfFeatures);
    }

    // Clearing count bits is timing sensitive.
    m_countBits->clearAll();
}

void UseCounter::didCommitLoad()
{
    updateMeasurements();
}

void UseCounter::observe(Document* document, Feature feature)
{
    if (!document)
        return;

    Page* page = document->page();
    if (!page)
        return;

    page->useCounter()->didObserve(feature);
}

void UseCounter::observe(DOMWindow* domWindow, Feature feature)
{
    ASSERT(domWindow);
    observe(domWindow->document(), feature);
}

} // namespace WebCore
