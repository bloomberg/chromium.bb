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

#ifndef HTMLImportsController_h
#define HTMLImportsController_h

#include "core/html/LinkResource.h"
#include "core/loader/cache/CachedResourceClient.h"
#include "core/loader/cache/CachedResourceHandle.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CachedResourceLoader;
class HTMLImportLoader;
class HTMLImportsController;

//
// A LinkResource subclasss used for @rel=import.
//
class LinkImport : public LinkResource {
    WTF_MAKE_FAST_ALLOCATED;
public:

    static PassRefPtr<LinkImport> create(HTMLLinkElement* owner);

    explicit LinkImport(HTMLLinkElement* owner);
    virtual ~LinkImport();

    // LinkResource
    virtual void process() OVERRIDE;
    virtual Type type() const OVERRIDE { return Import; }
    virtual void ownerRemoved() OVERRIDE;

    Document* importedDocument() const;

private:
    RefPtr<HTMLImportLoader> m_loader;
};


class HTMLImportLoader : public RefCounted<HTMLImportLoader>, public CachedResourceClient {
public:
    enum State {
        StateLoading,
        StateError,
        StateReady
    };

    static PassRefPtr<HTMLImportLoader> create(HTMLImportsController*, const KURL&, const CachedResourceHandle<CachedScript>&);
    virtual ~HTMLImportLoader();

    Document* importedDocument() const;
    const KURL& url() const { return m_url; }

    void importDestroyed();
    bool isDone() const { return m_state == StateReady || m_state == StateError; }

private:
    HTMLImportLoader(HTMLImportsController*, const KURL&, const CachedResourceHandle<CachedScript>&);

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*) OVERRIDE;

    State finish();
    void setState(State);

    HTMLImportsController* m_controller;
    State m_state;
    KURL m_url;
    CachedResourceHandle<CachedScript> m_resource;
    RefPtr<Document> m_importedDocument;
};


class HTMLImportsController : public RefCounted<HTMLImportsController> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<HTMLImportsController> create(Document*);

    explicit HTMLImportsController(Document*);
    virtual ~HTMLImportsController();

    void addImport(PassRefPtr<HTMLImportLoader>);
    void showSecurityErrorMessage(const String&);
    PassRefPtr<HTMLImportLoader> findLinkFor(const KURL&) const;
    SecurityOrigin* securityOrigin() const;
    CachedResourceLoader* cachedResourceLoader() const;
    bool haveLoaded() const;
    void didLoad();

private:
    Document* m_master;

    // List of import which has been loaded or being loaded.
    typedef Vector<RefPtr<HTMLImportLoader> > ImportList;
    ImportList m_imports;
};

} // namespace WebCore

#endif // HTMLImportsController_h
