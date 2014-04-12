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
#include "core/html/imports/HTMLImportLoader.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/html/HTMLDocument.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/loader/DocumentWriter.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"


namespace WebCore {

HTMLImportLoader::HTMLImportLoader(HTMLImportsController* controller)
    : m_controller(controller)
    , m_state(StateLoading)
{
}

HTMLImportLoader::~HTMLImportLoader()
{
    clear();
}

void HTMLImportLoader::importDestroyed()
{
    clear();
}

void HTMLImportLoader::clear()
{
    m_controller = 0;
    if (m_importedDocument) {
        m_importedDocument->setImportsController(0);
        m_importedDocument->cancelParsing();
        m_importedDocument.clear();
    }
}

void HTMLImportLoader::startLoading(const ResourcePtr<RawResource>& resource)
{
    setResource(resource);
}

void HTMLImportLoader::responseReceived(Resource* resource, const ResourceResponse& response)
{
    // Resource may already have been loaded with the import loader
    // being added as a client later & now being notified. Fail early.
    if (resource->loadFailedOrCanceled() || response.httpStatusCode() >= 400) {
        setState(StateError);
        return;
    }
    setState(startWritingAndParsing(response));
}

void HTMLImportLoader::dataReceived(Resource*, const char* data, int length)
{
    RefPtr<DocumentWriter> protectingWriter(m_writer);
    m_writer->addData(data, length);
}

void HTMLImportLoader::notifyFinished(Resource* resource)
{
    // The writer instance indicates that a part of the document can be already loaded.
    // We don't take such a case as an error because the partially-loaded document has been visible from script at this point.
    if (resource->loadFailedOrCanceled() && !m_writer) {
        setState(StateError);
        return;
    }

    setState(finishWriting());
}

HTMLImportLoader::State HTMLImportLoader::startWritingAndParsing(const ResourceResponse& response)
{
    ASSERT(!m_imports.isEmpty());
    DocumentInit init = DocumentInit(response.url(), 0, m_controller->master()->contextDocument(), m_controller)
        .withRegistrationContext(m_controller->master()->registrationContext());
    m_importedDocument = HTMLDocument::create(init);
    m_writer = DocumentWriter::create(m_importedDocument.get(), response.mimeType(), "UTF-8");

    return StateLoading;
}

HTMLImportLoader::State HTMLImportLoader::finishWriting()
{
    return StateWritten;
}

HTMLImportLoader::State HTMLImportLoader::finishParsing()
{
    return StateParsed;
}

HTMLImportLoader::State HTMLImportLoader::finishLoading()
{
    return StateLoaded;
}

void HTMLImportLoader::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;

    if (m_state == StateParsed || m_state == StateError || m_state == StateWritten) {
        if (RefPtr<DocumentWriter> writer = m_writer.release())
            writer->end();
    }

    // Since DocumentWriter::end() can let setState() reenter, we shouldn't refer to m_state here.
    if (state == StateLoaded || state == StateError)
        didFinishLoading();
}

void HTMLImportLoader::didFinishParsing()
{
    setState(finishParsing());
    if (!hasPendingResources())
        setState(finishLoading());
}

void HTMLImportLoader::didRemoveAllPendingStylesheet()
{
    if (m_state == StateParsed)
        setState(finishLoading());
}

bool HTMLImportLoader::hasPendingResources() const
{
    return m_importedDocument && m_importedDocument->styleEngine()->hasPendingSheets();
}

Document* HTMLImportLoader::importedDocument() const
{
    if (m_state == StateError)
        return 0;
    return m_importedDocument.get();
}

void HTMLImportLoader::didFinishLoading()
{
    for (size_t i = 0; i < m_imports.size(); ++i)
        m_imports[i]->didFinishLoading();

    clearResource();

    ASSERT(!m_importedDocument || !m_importedDocument->parsing());
}

void HTMLImportLoader::addImport(HTMLImportChild* client)
{
    ASSERT(kNotFound == m_imports.find(client));
    m_imports.append(client);
    if (isDone())
        client->didFinishLoading();
}

void HTMLImportLoader::removeImport(HTMLImportChild* client)
{
    ASSERT(kNotFound != m_imports.find(client));
    m_imports.remove(m_imports.find(client));
}

} // namespace WebCore
