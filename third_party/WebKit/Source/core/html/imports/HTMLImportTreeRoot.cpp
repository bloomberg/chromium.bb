// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/imports/HTMLImportTreeRoot.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/html/imports/HTMLImportChild.h"

namespace WebCore {

PassOwnPtr<HTMLImportTreeRoot> HTMLImportTreeRoot::create(Document* document)
{
    return adoptPtr(new HTMLImportTreeRoot(document));
}

HTMLImportTreeRoot::HTMLImportTreeRoot(Document* document)
    : HTMLImport(HTMLImport::Sync)
    , m_document(document)
    , m_recalcTimer(this, &HTMLImportTreeRoot::recalcTimerFired)
{
    recalcTreeState(this); // This recomputes initial state.
}

HTMLImportTreeRoot::~HTMLImportTreeRoot()
{
    for (size_t i = 0; i < m_imports.size(); ++i)
        m_imports[i]->importDestroyed();
    m_imports.clear();
}

Document* HTMLImportTreeRoot::document() const
{
    return m_document;
}

bool HTMLImportTreeRoot::isDone() const
{
    return !m_document->parsing() && m_document->styleEngine()->haveStylesheetsLoaded();
}

void HTMLImportTreeRoot::stateWillChange()
{
    scheduleRecalcState();
}

void HTMLImportTreeRoot::stateDidChange()
{
    HTMLImport::stateDidChange();

    if (!state().isReady())
        return;
    if (LocalFrame* frame = m_document->frame())
        frame->loader().checkCompleted();
}

void HTMLImportTreeRoot::scheduleRecalcState()
{
    if (m_recalcTimer.isActive() || !m_document)
        return;
    m_recalcTimer.startOneShot(0, FROM_HERE);
}

HTMLImportChild* HTMLImportTreeRoot::add(PassOwnPtr<HTMLImportChild> child)
{
    m_imports.append(child);
    return m_imports.last().get();
}

HTMLImportChild* HTMLImportTreeRoot::find(const KURL& url) const
{
    for (size_t i = 0; i < m_imports.size(); ++i) {
        HTMLImportChild* candidate = m_imports[i].get();
        if (equalIgnoringFragmentIdentifier(candidate->url(), url))
            return candidate;
    }

    return 0;
}

void HTMLImportTreeRoot::recalcTimerFired(Timer<HTMLImportTreeRoot>*)
{
    ASSERT(m_document);

    do {
        m_recalcTimer.stop();
        HTMLImport::recalcTreeState(this);
    } while (m_recalcTimer.isActive());
}

}
