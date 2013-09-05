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

#include "core/dom/CustomElementRegistrationContext.h"
#include "core/dom/Document.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLImportLoaderClient.h"
#include "core/loader/DocumentWriter.h"
#include "core/page/ContentSecurityPolicyResponseHeaders.h"

namespace WebCore {

HTMLImportLoader::HTMLImportLoader(HTMLImport* parent, const KURL& url)
    : m_parent(parent)
    , m_state(StateLoading)
    , m_url(url)
{
}

HTMLImportLoader::~HTMLImportLoader()
{
    // importDestroyed() should be called before the destruction.
    ASSERT(!m_parent);
    ASSERT(!m_importedDocument);
    if (m_resource)
        m_resource->removeClient(this);
}

void HTMLImportLoader::setResource(const ResourcePtr<RawResource>& resource)
{
    m_resource = resource;
    m_resource->addClient(this);
}

void HTMLImportLoader::responseReceived(Resource*, const ResourceResponse& response)
{
    setState(startWritingAndParsing(response));
}

void HTMLImportLoader::dataReceived(Resource*, const char* data, int length)
{
    RefPtr<DocumentWriter> protectingWriter(m_writer);
    m_writer->addData(data, length);
}

void HTMLImportLoader::notifyFinished(Resource*)
{
    setState(finishWriting());
}

void HTMLImportLoader::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;

    if (m_state == StateReady || m_state == StateError || m_state == StateWritten) {
        if (RefPtr<DocumentWriter> writer = m_writer.release())
            writer->end();
    }

    // Since DocumentWriter::end() let setState() reenter, we shouldn't refer to m_state here.
    if (state == StateReady || state == StateError)
        didFinish();
}

void HTMLImportLoader::didFinish()
{
    for (size_t i = 0; i < m_clients.size(); ++i)
        m_clients[i]->didFinish();

    if (m_resource) {
        m_resource->removeClient(this);
        m_resource = 0;
    }

    ASSERT(!document() || !document()->parsing());
    root()->importWasDisposed();
}

HTMLImportLoader::State HTMLImportLoader::startWritingAndParsing(const ResourceResponse& response)
{
    // Current canAccess() implementation isn't sufficient for catching cross-domain redirects: http://crbug.com/256976
    if (!m_parent->document()->fetcher()->canAccess(m_resource.get()))
        return StateError;

    DocumentInit init = DocumentInit(response.url(), 0, root()->document()->contextDocument(), this)
        .withRegistrationContext(root()->document()->registrationContext());
    m_importedDocument = HTMLDocument::create(init);
    m_importedDocument->initContentSecurityPolicy(ContentSecurityPolicyResponseHeaders(response));
    m_writer = DocumentWriter::create(m_importedDocument.get(), response.mimeType(), response.textEncodingName());

    return StateLoading;
}

HTMLImportLoader::State HTMLImportLoader::finishWriting()
{
    if (!m_parent)
        return StateError;
    // The writer instance indicates that a part of the document can be already loaded.
    // We don't take such a case as an error because the partially-loaded document has been visible from script at this point.
    if (m_resource->loadFailedOrCanceled() && !m_writer)
        return StateError;

    return StateWritten;
}

HTMLImportLoader::State HTMLImportLoader::finishParsing()
{
    if (!m_parent)
        return StateError;
    return StateReady;
}

Document* HTMLImportLoader::importedDocument() const
{
    if (m_state == StateError)
        return 0;
    return m_importedDocument.get();
}

void HTMLImportLoader::addClient(HTMLImportLoaderClient* client)
{
    ASSERT(notFound == m_clients.find(client));
    m_clients.append(client);
    if (isDone())
        client->didFinish();
}

void HTMLImportLoader::removeClient(HTMLImportLoaderClient* client)
{
    ASSERT(notFound != m_clients.find(client));
    m_clients.remove(m_clients.find(client));
}

void HTMLImportLoader::importDestroyed()
{
    m_parent = 0;
    if (RefPtr<Document> document = m_importedDocument.release())
        document->setImport(0);
}

HTMLImportRoot* HTMLImportLoader::root()
{
    return m_parent ? m_parent->root() : 0;
}

HTMLImport* HTMLImportLoader::parent() const
{
    return m_parent;
}

Document* HTMLImportLoader::document() const
{
    return m_importedDocument.get();
}

void HTMLImportLoader::wasDetachedFromDocument()
{
    // For imported documens this shouldn't be called because Document::m_import is
    // cleared before Document is destroyed by HTMLImportLoader::importDestroyed().
    ASSERT_NOT_REACHED();
}

void HTMLImportLoader::didFinishParsing()
{
    setState(finishParsing());
}

bool HTMLImportLoader::isProcessing() const
{
    if (!m_importedDocument)
        return !isDone();
    return m_importedDocument->parsing();
}

} // namespace WebCore
