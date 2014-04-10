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

#ifndef HTMLImportLoader_h
#define HTMLImportLoader_h

#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceOwner.h"
#include "wtf/Vector.h"

namespace WebCore {

class Document;
class DocumentWriter;
class HTMLImportChild;
class HTMLImportsController;

//
// Owning imported Document lifetime. It also implements ResourceClient through ResourceOwner
// to feed fetched bytes to the DocumentWriter of the imported document.
// HTMLImportLoader is owned by and shared between HTMLImportChild.
//
//
class HTMLImportLoader FINAL : public RefCounted<HTMLImportLoader>, public ResourceOwner<RawResource> {
public:
    enum State {
        StateLoading,
        StateWritten,
        StateParsed,
        StateLoaded,
        StateError
    };

    static PassRefPtr<HTMLImportLoader> create(HTMLImportsController* controller)
    {
        return adoptRef(new HTMLImportLoader(controller));
    }

    virtual ~HTMLImportLoader();

    Document* document() const { return m_importedDocument.get(); }
    Document* importedDocument() const;
    void addImport(HTMLImportChild*);
    void removeImport(HTMLImportChild*);
    HTMLImportChild* firstImport() const { return m_imports[0]; }

    bool isDone() const { return m_state == StateLoaded || m_state == StateError; }
    bool hasError() const { return m_state == StateError; }

    void importDestroyed();
    void startLoading(const ResourcePtr<RawResource>&);
    void didFinishParsing();
    void didRemoveAllPendingStylesheet();
    bool isOwnedBy(const HTMLImportChild* import) const { return m_imports[0] == import; }

private:
    HTMLImportLoader(HTMLImportsController*);

    // RawResourceClient
    virtual void responseReceived(Resource*, const ResourceResponse&) OVERRIDE;
    virtual void dataReceived(Resource*, const char* data, int length) OVERRIDE;
    virtual void notifyFinished(Resource*) OVERRIDE;

    State startWritingAndParsing(const ResourceResponse&);
    State finishWriting();
    State finishParsing();
    State finishLoading();

    void setState(State);
    void didFinishLoading();
    bool hasPendingResources() const;
    void clear();

    HTMLImportsController* m_controller;
    Vector<HTMLImportChild*> m_imports;
    State m_state;
    RefPtr<Document> m_importedDocument;
    RefPtr<DocumentWriter> m_writer;
};

} // namespace WebCore

#endif // HTMLImportLoader_h
