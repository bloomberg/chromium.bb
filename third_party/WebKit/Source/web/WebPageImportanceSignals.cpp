// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebPageImportanceSignals.h"

#include "public/platform/Platform.h"
#include "public/web/WebViewClient.h"

namespace blink {

void WebPageImportanceSignals::reset()
{
    m_hadFormInteraction = false;
    m_issuedNonGetFetchFromScript = false;
    if (m_observer)
        m_observer->pageImportanceSignalsChanged();
}

void WebPageImportanceSignals::setHadFormInteraction()
{
    m_hadFormInteraction = true;
    if (m_observer)
        m_observer->pageImportanceSignalsChanged();
}

void WebPageImportanceSignals::setIssuedNonGetFetchFromScript()
{
    m_issuedNonGetFetchFromScript = true;
    if (m_observer)
        m_observer->pageImportanceSignalsChanged();
}

void WebPageImportanceSignals::onCommitLoad()
{
    Platform::current()->histogramEnumeration("PageImportanceSignals.HadFormInteraction.OnCommitLoad", m_hadFormInteraction, 2);
    Platform::current()->histogramEnumeration("PageImportanceSignals.IssuedNonGetFetchFromScript.OnCommitLoad", m_issuedNonGetFetchFromScript, 2);

    reset();
}

} // namespace blink
