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
#include "core/page/UseCounter.h"

#include "core/dom/Document.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/page/DOMWindow.h"
#include "core/page/Page.h"
#include "core/page/PageConsole.h"
#include "core/platform/HistogramSupport.h"
#include "wtf/text/WTFString.h"

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

void UseCounter::count(Document* document, Feature feature)
{
    if (!document)
        return;

    Page* page = document->page();
    if (!page)
        return;

    ASSERT(page->useCounter()->deprecationMessage(feature).isEmpty());
    page->useCounter()->recordMeasurement(feature);
}

void UseCounter::count(DOMWindow* domWindow, Feature feature)
{
    ASSERT(domWindow);
    count(domWindow->document(), feature);
}

void UseCounter::countDeprecation(ScriptExecutionContext* context, Feature feature)
{
    if (!context || !context->isDocument())
        return;
    UseCounter::countDeprecation(toDocument(context), feature);
}

void UseCounter::countDeprecation(DOMWindow* window, Feature feature)
{
    if (!window)
        return;
    UseCounter::countDeprecation(window->document(), feature);
}

void UseCounter::countDeprecation(Document* document, Feature feature)
{
    if (!document)
        return;

    Page* page = document->page();
    if (!page)
        return;

    if (page->useCounter()->recordMeasurement(feature)) {
        ASSERT(!page->useCounter()->deprecationMessage(feature).isEmpty());
        page->console()->addMessage(DeprecationMessageSource, WarningMessageLevel, page->useCounter()->deprecationMessage(feature));
    }
}

String UseCounter::deprecationMessage(Feature feature)
{
    switch (feature) {
    // Content Security Policy
    case PrefixedContentSecurityPolicy:
    case PrefixedContentSecurityPolicyReportOnly:
        return "The 'X-WebKit-CSP' headers are deprecated; please consider using the canonical 'Content-Security-Policy' header instead.";

    // HTMLMediaElement
    case PrefixedMediaAddKey:
        return "'HTMLMediaElement.webkitAddKey()' is deprecated. Please use 'MediaKeySession.update()' instead.";
    case PrefixedMediaGenerateKeyRequest:
        return "'HTMLMediaElement.webkitGenerateKeyRequest()' is deprecated. Please use 'MediaKeys.createSession()' instead.";

    // Quota
    case StorageInfo:
        return "'window.webkitStorageInfo' is deprecated. Please use 'navigator.webkitTemporaryStorage' or 'navigator.webkitPersistentStorage' instead.";

    // Performance
    case PrefixedPerformanceTimeline:
        return "'window.performance.webkitGet*' methods have been deprecated. Please use the unprefixed 'performance.get*' methods instead.";
    case PrefixedUserTiming:
        return "'window.performance.webkit*' methods have been deprecated. Please use the unprefixed 'window.performance.*' methods instead.";

    // Web Audio
    case WebAudioLooping:
        return "AudioBufferSourceNode 'looping' attribute is deprecated.  Use 'loop' instead.";

    // Features that aren't deprecated don't have a deprecation message.
    default:
        return String();
    }
}

} // namespace WebCore
