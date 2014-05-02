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
#include "core/dom/custom/CustomElementMicrotaskDispatcher.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/html/imports/HTMLImportChildClient.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"

namespace WebCore {

HTMLImportChild::HTMLImportChild(Document& master, const KURL& url, SyncMode sync)
    : HTMLImport(sync)
#if ENABLE(OILPAN)
    , m_master(&master)
#else
    , m_master(master)
#endif
    , m_url(url)
    , m_weakFactory(this)
    , m_loader(0)
    , m_client(0)
{
#if !ENABLE(OILPAN)
    m_master.guardRef();
#endif
}

HTMLImportChild::~HTMLImportChild()
{
    // importDestroyed() should be called before the destruction.
    ASSERT(!m_loader);

    if (m_client)
        m_client->importChildWasDestroyed(this);

#if !ENABLE(OILPAN)
    m_master.guardDeref();
#endif
}

void HTMLImportChild::wasAlreadyLoaded()
{
    ASSERT(!m_loader);
    ASSERT(m_client);
    ensureLoader();
    stateWillChange();
}

void HTMLImportChild::startLoading(const ResourcePtr<RawResource>& resource)
{
    ASSERT(!this->resource());
    ASSERT(!m_loader);

    setResource(resource);

    ensureLoader();
}

void HTMLImportChild::didFinish()
{
    if (m_client)
        m_client->didFinish();
}

static bool hasAsyncImportAncestor(HTMLImport* import)
{
    for (HTMLImport* i = import; i; i = i->parent()) {
        if (!i->isSync())
            return true;
    }

    return false;
}

void HTMLImportChild::didFinishLoading()
{
    clearResource();
    stateWillChange();
    if (m_customElementMicrotaskStep)
        CustomElementMicrotaskDispatcher::instance().importDidFinish(m_customElementMicrotaskStep.get());
    // FIXME(crbug.com/365956):
    // This is needed because async import currently never consumes the queue and
    // it prevents the children of such an async import from finishing.
    // Their steps are never processed.
    // Although this is clearly not the right fix,
    // https://codereview.chromium.org/249563003/ should get rid of this part.
    if (hasAsyncImportAncestor(this))
        didFinishUpgradingCustomElements();
}

void HTMLImportChild::didFinishUpgradingCustomElements()
{
    stateWillChange();
    m_customElementMicrotaskStep.clear();
}

bool HTMLImportChild::isLoaded() const
{
    return m_loader && m_loader->isDone();
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

Document* HTMLImportChild::document() const
{
    return m_loader ? m_loader->document() : 0;
}

void HTMLImportChild::stateWillChange()
{
    toHTMLImportsController(root())->scheduleRecalcState();
}

void HTMLImportChild::stateDidChange()
{
    HTMLImport::stateDidChange();

    ensureLoader();
    if (state().isReady())
        didFinish();
}

void HTMLImportChild::ensureLoader()
{
    if (m_loader)
        return;

    if (HTMLImportChild* found = toHTMLImportsController(root())->findLinkFor(m_url, this))
        shareLoader(found);
    else
        createLoader();

    if (isSync() && !isDone()) {
        ASSERT(!m_customElementMicrotaskStep);
        m_customElementMicrotaskStep = CustomElement::didCreateImport(this)->weakPtr();
    }
}

void HTMLImportChild::createLoader()
{
    ASSERT(!m_loader);
    m_loader = toHTMLImportsController(root())->createLoader();
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
    return m_loader && m_loader->isDone() && !m_customElementMicrotaskStep;
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
    fprintf(stderr, " loader=%p step=%p sync=%s url=%s",
        m_loader,
        m_customElementMicrotaskStep.get(),
        isSync() ? "Y" : "N",
        url().string().utf8().data());
}
#endif

} // namespace WebCore
