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
#include "core/html/HTMLImportLoader.h"

#include "core/dom/Document.h"
#include "core/html/HTMLImportData.h"
#include "core/html/HTMLImportLoaderClient.h"

namespace WebCore {

HTMLImportLoader::HTMLImportLoader(const KURL& url, HTMLImportLoaderClient* client)
    : m_url(url)
    , m_client(client)
{
}

HTMLImportLoader::~HTMLImportLoader()
{
    // importDestroyed() should be called before the destruction.
    ASSERT(!m_data);
}

void HTMLImportLoader::wasAlreadyLoadedAs(HTMLImportLoader* found)
{
    ASSERT(!m_data);
    ASSERT(m_client);
    shareData(found);
}

void HTMLImportLoader::startLoading(const ResourcePtr<RawResource>& resource)
{
    ASSERT(!hasResource());
    ASSERT(!m_data);

    HTMLImportResourceOwner::setResource(resource);

    // If the node is "document blocked", it cannot create HTMLImportData
    // even if there is no sharable one found, as there is possibility that
    // preceding imports load the sharable imports.
    // In that case preceding one should win because it comes first in the tree order.
    // See also didUnblockDocument().
    if (isDocumentBlocked())
        return;

    createData();
}

void HTMLImportLoader::didFinish()
{
    if (m_client)
        m_client->didFinish();
    clearResource();
    root()->blockerGone();
}

Document* HTMLImportLoader::importedDocument() const
{
    if (!m_data)
        return 0;
    return m_data->importedDocument();
}

void HTMLImportLoader::importDestroyed()
{
    if (parent())
        parent()->removeChild(this);
    if (m_data) {
        m_data->removeClient(this);
        m_data.clear();
    }
}

HTMLImportRoot* HTMLImportLoader::root()
{
    return parent() ? parent()->root() : 0;
}

Document* HTMLImportLoader::document() const
{
    return (m_data && m_data->isOwnedBy(this)) ? m_data->document() : 0;
}

void HTMLImportLoader::wasDetachedFromDocument()
{
    // For imported documens this shouldn't be called because Document::m_import is
    // cleared before Document is destroyed by HTMLImportLoader::importDestroyed().
    ASSERT_NOT_REACHED();
}

void HTMLImportLoader::didFinishParsing()
{
    ASSERT(m_data->isOwnedBy(this));
    m_data->didFinishParsing();
}

// Once all preceding imports are loaded and "document blocking" ends,
// HTMLImportLoader can decide whether it should load the import by itself
// or it can share existing one.
void HTMLImportLoader::didUnblockDocument()
{
    HTMLImport::didUnblockDocument();
    ASSERT(!isDocumentBlocked());
    ASSERT(!m_data || !m_data->isOwnedBy(this));

    if (m_data)
        return;
    if (HTMLImportLoader* found = root()->findLinkFor(m_url, this))
        shareData(found);
    else
        createData();
}

void HTMLImportLoader::createData()
{
    ASSERT(!isDocumentBlocked());
    ASSERT(!m_data);
    m_data = HTMLImportData::create(this, parent()->document()->fetcher());
    m_data->addClient(this);
    m_data->startLoading(resource());
}

void HTMLImportLoader::shareData(HTMLImportLoader* loader)
{
    ASSERT(!m_data);
    m_data = loader->m_data;
    m_data->addClient(this);
    root()->blockerGone();
}

bool HTMLImportLoader::isProcessing() const
{
    return m_data && m_data->isOwnedBy(this) && m_data->isProcessing();
}

bool HTMLImportLoader::isDone() const
{
    return m_data && m_data->isDone();
}

bool HTMLImportLoader::isLoaded() const
{
    return m_data->isLoaded();
}

} // namespace WebCore
