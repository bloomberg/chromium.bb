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

#include "core/html/HTMLImport.h"
#include "core/html/LinkResource.h"
#include "core/loader/cache/RawResource.h"
#include "core/platform/Supplementable.h"
#include "core/platform/Timer.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class FetchRequest;
class ScriptExecutionContext;
class ResourceFetcher;
class HTMLImportLoader;
class HTMLImportLoaderClient;

class HTMLImportsController : public HTMLImportRoot, public Supplement<ScriptExecutionContext> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void provideTo(Document*);

    explicit HTMLImportsController(Document*);
    virtual ~HTMLImportsController();

    // HTMLImport
    virtual HTMLImportRoot* root() OVERRIDE;
    virtual HTMLImport* parent() const OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual void wasDetachedFromDocument() OVERRIDE;
    virtual void didFinishParsing() OVERRIDE;
    virtual bool isProcessing() const OVERRIDE;
    // HTMLImportRoot
    virtual void importWasDisposed() OVERRIDE;
    virtual HTMLImportsController* toController() { return this; }

    PassRefPtr<HTMLImportLoader> createLoader(HTMLImport* parent, FetchRequest);
    void showSecurityErrorMessage(const String&);
    PassRefPtr<HTMLImportLoader> findLinkFor(const KURL&) const;
    SecurityOrigin* securityOrigin() const;
    ResourceFetcher* fetcher() const;

    void scheduleUnblock();
    void unblockTimerFired(Timer<HTMLImportsController>*);

private:
    void clear();

    Document* m_master;
    Timer<HTMLImportsController> m_unblockTimer;

    // List of import which has been loaded or being loaded.
    typedef Vector<RefPtr<HTMLImportLoader> > ImportList;
    ImportList m_imports;
};

} // namespace WebCore

#endif // HTMLImportsController_h
