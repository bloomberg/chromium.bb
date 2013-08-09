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

#include "core/html/HTMLImport.h"
#include "core/loader/cache/RawResource.h"
#include "core/loader/cache/ResourcePtr.h"
#include "weborigin/KURL.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class DocumentWriter;
class HTMLImportLoaderClient;

class HTMLImportLoader : public RefCounted<HTMLImportLoader>, public HTMLImport, public RawResourceClient {
public:
    enum State {
        StateLoading,
        StateWritten,
        StateError,
        StateReady
    };

    HTMLImportLoader(HTMLImport*, const KURL&, const ResourcePtr<RawResource>&);
    virtual ~HTMLImportLoader();

    Document* importedDocument() const;
    const KURL& url() const { return m_url; }

    void addClient(HTMLImportLoaderClient*);
    void removeClient(HTMLImportLoaderClient*);
    void importDestroyed();
    bool isDone() const { return m_state == StateReady || m_state == StateError; }
    bool isLoaded() const { return m_state == StateReady; }

    // HTMLImport
    virtual HTMLImportRoot* root() OVERRIDE;
    virtual HTMLImport* parent() const OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual void wasDetachedFromDocument() OVERRIDE;
    virtual void didFinishParsing() OVERRIDE;
    virtual bool isProcessing() const OVERRIDE;

private:

    // RawResourceClient
    virtual void responseReceived(Resource*, const ResourceResponse&) OVERRIDE;
    virtual void dataReceived(Resource*, const char* data, int length) OVERRIDE;
    virtual void notifyFinished(Resource*) OVERRIDE;

    State startWritingAndParsing(const ResourceResponse&);
    State finishWriting();
    State finishParsing();

    void setState(State);
    void didFinish();

    HTMLImport* m_parent;
    Vector<HTMLImportLoaderClient*> m_clients;
    State m_state;
    KURL m_url;
    ResourcePtr<RawResource> m_resource;
    RefPtr<Document> m_importedDocument;
    RefPtr<DocumentWriter> m_writer;
};

} // namespace WebCore

#endif // HTMLImportLoader_h
