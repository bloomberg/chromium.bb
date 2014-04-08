/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/imports/HTMLImportChild.h"

#include "core/dom/Document.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/html/imports/HTMLImportChildClient.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"

namespace WebCore {

HTMLImportChild::HTMLImportChild(Document& master, const KURL& url, SyncMode sync)
    : HTMLImport(sync)
    , m_master(master)
    , m_url(url)
    , m_customElementMicrotaskStep(0)
    , m_loader(0)
    , m_client(0)
{
    m_master.guardRef();
}

HTMLImportChild::~HTMLImportChild()
{
    // importDestroyed() should be called before the destruction.
    ASSERT(!m_loader);

    if (m_customElementMicrotaskStep) {
        // if Custom Elements were blocked, must unblock them before death
        m_customElementMicrotaskStep->importDidFinish();
        m_customElementMicrotaskStep = 0;
    }

    if (m_client)
        m_client->importChildWasDestroyed(this);

    m_master.guardDeref();
}

void HTMLImportChild::wasAlreadyLoaded()
{
    ASSERT(!m_loader);
    ASSERT(m_client);
    stateWillChange();
}

void HTMLImportChild::startLoading(const ResourcePtr<RawResource>& resource)
{
    ASSERT(!this->resource());
    ASSERT(!m_loader);

    if (isSync()) {
        ASSERT(!m_customElementMicrotaskStep);
        m_customElementMicrotaskStep = CustomElement::didCreateImport(this);
    }

    setResource(resource);

    // If the node is "document blocked", it cannot create HTMLImportLoader
    // even if there is no sharable one found, as there is possibility that
    // preceding imports load the sharable imports.
    // In that case preceding one should win because it comes first in the tree order.
    // See also didUnblockFromCreatingDocument().
    if (state().shouldBlockDocumentCreation())
        return;

    ensureLoader();
}

void HTMLImportChild::didFinish()
{
    if (m_client)
        m_client->didFinish();

    if (m_customElementMicrotaskStep) {
        m_customElementMicrotaskStep->importDidFinish();
        m_customElementMicrotaskStep = 0;
    }
}

void HTMLImportChild::didFinishLoading()
{
    clearResource();
    stateWillChange();
}

Document* HTMLImportChild::importedDocument() const
{
    if (!m_loader)
        return 0;
    return m_loader->importedDocument();
}

void HTMLImportChild::importDestroyed()
{
    if (parent())
        parent()->removeChild(this);
    if (m_loader) {
        m_loader->removeImport(this);
        m_loader = 0;
    }
}

HTMLImportRoot* HTMLImportChild::root()
{
    return parent() ? parent()->root() : 0;
}

Document* HTMLImportChild::document() const
{
    return (m_loader && m_loader->isOwnedBy(this)) ? m_loader->document() : 0;
}

void HTMLImportChild::wasDetachedFromDocument()
{
    // For imported documens this shouldn't be called because Document::m_import is
    // cleared before Document is destroyed by HTMLImportChild::importDestroyed().
    ASSERT_NOT_REACHED();
}

void HTMLImportChild::didFinishParsing()
{
    ASSERT(m_loader->isOwnedBy(this));
    m_loader->didFinishParsing();
}

void HTMLImportChild::didRemoveAllPendingStylesheet()
{
    ASSERT(m_loader->isOwnedBy(this));
    m_loader->didRemoveAllPendingStylesheet();
}

void HTMLImportChild::stateDidChange()
{
    HTMLImport::stateDidChange();

    // Once all preceding imports are loaded,
    // HTMLImportChild can decide whether it should load the import by itself
    // or it can share existing one.
    if (!state().shouldBlockDocumentCreation())
        ensureLoader();
    if (state().isReady())
        didFinish();
}

void HTMLImportChild::ensureLoader()
{
    if (m_loader)
        return;

    if (HTMLImportChild* found = root()->findLinkFor(m_url, this))
        shareLoader(found);
    else
        createLoader();
}

void HTMLImportChild::createLoader()
{
    ASSERT(!state().shouldBlockDocumentCreation());
    ASSERT(!m_loader);
    m_loader = root()->toController()->createLoader();
    m_loader->addImport(this);
    m_loader->startLoading(resource());
}

void HTMLImportChild::shareLoader(HTMLImportChild* loader)
{
    ASSERT(!m_loader);
    m_loader = loader->m_loader;
    m_loader->addImport(this);
    stateWillChange();
}

bool HTMLImportChild::isDone() const
{
    return m_loader && m_loader->isDone();
}

bool HTMLImportChild::hasLoader() const
{
    return m_loader;
}

bool HTMLImportChild::ownsLoader() const
{
    return m_loader && m_loader->isOwnedBy(this);
}

bool HTMLImportChild::loaderHasError() const
{
    return m_loader && m_loader->hasError();
}


void HTMLImportChild::setClient(HTMLImportChildClient* client)
{
    ASSERT(client);
    ASSERT(!m_client);
    m_client = client;
}

void HTMLImportChild::clearClient()
{
    // Doesn't check m_client nullity because we allow
    // clearClient() to reenter.
    m_client = 0;
}

HTMLLinkElement* HTMLImportChild::link() const
{
    if (!m_client)
        return 0;
    return m_client->link();
}

#if !defined(NDEBUG)
void HTMLImportChild::showThis()
{
    HTMLImport::showThis();
    fprintf(stderr, " loader=%p own=%s async=%s url=%s",
        m_loader,
        hasLoader() && ownsLoader() ? "Y" : "N",
        isSync() ? "Y" : "N",
        url().string().utf8().data());
}
#endif

} // namespace WebCore
