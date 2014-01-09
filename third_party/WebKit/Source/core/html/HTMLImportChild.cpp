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
#include "core/html/HTMLImportChild.h"

#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementPendingImport.h"
#include "core/dom/custom/CustomElementRegistrationContext.h"
#include "core/html/HTMLImportChildClient.h"
#include "core/html/HTMLImportLoader.h"

namespace WebCore {

HTMLImportChild::HTMLImportChild(const KURL& url, bool createdByParser)
    : HTMLImport(WaitingLoaderOrChildren, createdByParser)
    , m_url(url)
    , m_traversingClients(false)
{
}

HTMLImportChild::~HTMLImportChild()
{
    // importDestroyed() should be called before the destruction.
    ASSERT(!m_loader);
    for (size_t i = 0; i < m_clients.size(); ++i) {
        TemporaryChange<bool> traversing(m_traversingClients, true);
        m_clients[i]->importWillBeDestroyed();
    }
}

void HTMLImportChild::wasAlreadyLoaded()
{
    ASSERT(!m_loader);
    ASSERT(m_clients.size());

    loaderWasResolved();
    ensureLoader();
}

void HTMLImportChild::startLoading(const ResourcePtr<RawResource>& resource)
{
    ASSERT(!hasResource());
    ASSERT(!m_loader);

    if (isCreatedByParser()) {
        m_pendingImport = CustomElementPendingImport::create(this);
        master()->registrationContext()->didStartLoadingImport(m_pendingImport.get());
    }

    HTMLImportResourceOwner::setResource(resource);

    // If the node is "document blocked", it cannot create HTMLImportLoader
    // even if there is no sharable one found, as there is possibility that
    // preceding imports load the sharable imports.
    // In that case preceding one should win because it comes first in the tree order.
    // See also didUnblockFromCreatingDocument().
    if (isBlockedFromCreatingDocument())
        return;

    ensureLoader();
}

void HTMLImportChild::didFinish()
{
    for (size_t i = 0; i < m_clients.size(); ++i) {
        TemporaryChange<bool> traversing(m_traversingClients, true);
        m_clients[i]->didFinish();
    }

    if (m_pendingImport)
        master()->registrationContext()->didFinishLoadingImport(m_pendingImport.release());
}

void HTMLImportChild::didFinishLoading()
{
    clearResource();
    loaderDidFinish();
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
        m_loader->removeClient(this);
        m_loader.clear();
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

// Once all preceding imports are loaded and "document blocking" ends,
// HTMLImportChild can decide whether it should load the import by itself
// or it can share existing one.
void HTMLImportChild::didUnblockFromCreatingDocument()
{
    HTMLImport::didUnblockFromCreatingDocument();
    ASSERT(!m_loader || !m_loader->isOwnedBy(this));
    ensureLoader();
}

void HTMLImportChild::didBecomeReady()
{
    HTMLImport::didBecomeReady();
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
    ASSERT(!isBlockedFromCreatingDocument());
    ASSERT(!m_loader);
    m_loader = HTMLImportLoader::create(this, parent()->document()->fetcher());
    m_loader->addClient(this);
    m_loader->startLoading(resource());
}

void HTMLImportChild::shareLoader(HTMLImportChild* loader)
{
    ASSERT(!m_loader);
    m_loader = loader->m_loader;
    m_loader->addClient(this);
    root()->blockerGone();
}

bool HTMLImportChild::isProcessing() const
{
    return m_loader && m_loader->isOwnedBy(this) && m_loader->isProcessing();
}

bool HTMLImportChild::isDone() const
{
    return m_loader && m_loader->isDone();
}

bool HTMLImportChild::isLoaded() const
{
    return m_loader->isLoaded();
}

void HTMLImportChild::addClient(HTMLImportChildClient* client)
{
    ASSERT(!m_traversingClients);
    ASSERT(!m_clients.contains(client));
    m_clients.append(client);
}

void HTMLImportChild::removeClient(HTMLImportChildClient* client)
{
    ASSERT(!m_traversingClients);
    size_t i = m_clients.find(client);
    ASSERT(i != kNotFound);
    m_clients.remove(i);
}

} // namespace WebCore
