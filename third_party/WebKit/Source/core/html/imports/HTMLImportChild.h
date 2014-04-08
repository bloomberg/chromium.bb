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

#ifndef HTMLImportChild_h
#define HTMLImportChild_h

#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceOwner.h"
#include "core/html/imports/HTMLImport.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementMicrotaskImportStep;
class HTMLImportLoader;
class HTMLImportChildClient;
class HTMLLinkElement;

//
// An import tree node subclas to encapsulate imported document
// lifecycle. This class is owned by LinkStyle. The actual loading
// is done by HTMLImportLoader, which can be shared among multiple
// HTMLImportChild of same link URL.
//
// HTMLImportChild implements ResourceClient through ResourceOwner
// so that it can speculatively request linked resources while it is unblocked.
//
class HTMLImportChild FINAL : public HTMLImport, public ResourceOwner<RawResource> {
public:
    HTMLImportChild(Document&, const KURL&, SyncMode);
    virtual ~HTMLImportChild();

    HTMLLinkElement* link() const;
    Document* importedDocument() const;
    const KURL& url() const { return m_url; }

    void wasAlreadyLoaded();
    void startLoading(const ResourcePtr<RawResource>&);
    void importDestroyed();

    // HTMLImport
    virtual bool isChild() const OVERRIDE { return true; }
    virtual HTMLImportRoot* root() OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual void wasDetachedFromDocument() OVERRIDE;
    virtual void didFinishParsing() OVERRIDE;
    virtual void didRemoveAllPendingStylesheet() OVERRIDE;
    virtual bool isDone() const OVERRIDE;
    virtual bool hasLoader() const OVERRIDE;
    virtual bool ownsLoader() const OVERRIDE;
    virtual CustomElementMicrotaskImportStep* customElementMicrotaskStep() const OVERRIDE FINAL { return m_customElementMicrotaskStep; }
    virtual void stateDidChange() OVERRIDE;

#if !defined(NDEBUG)
    virtual void showThis() OVERRIDE;
#endif

    void setClient(HTMLImportChildClient*);
    void clearClient();
    bool loaderHasError() const;

    void didFinishLoading();

private:
    // RawResourceOwner doing nothing.
    // HTMLImportChild owns the resource so that the contents of prefetched Resource doesn't go away.
    virtual void responseReceived(Resource*, const ResourceResponse&) OVERRIDE { }
    virtual void dataReceived(Resource*, const char*, int) OVERRIDE { }
    virtual void notifyFinished(Resource*) OVERRIDE { }

    void didFinish();
    void createLoader();
    void shareLoader(HTMLImportChild*);
    void ensureLoader();

    Document& m_master;
    KURL m_url;
    CustomElementMicrotaskImportStep* m_customElementMicrotaskStep;
    HTMLImportLoader* m_loader;
    HTMLImportChildClient* m_client;
};

inline HTMLImportChild* toHTMLImportChild(HTMLImport* import)
{
    ASSERT(!import || import->isChild());
    return static_cast<HTMLImportChild*>(import);
}

} // namespace WebCore

#endif // HTMLImportChild_h
