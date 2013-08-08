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
#include "core/html/LinkImport.h"

#include "core/dom/Document.h"
#include "core/html/HTMLImportLoader.h"
#include "core/html/HTMLImportsController.h"
#include "core/html/HTMLLinkElement.h"
#include "core/loader/CrossOriginAccessControl.h"
#include "core/loader/cache/ResourceFetcher.h"

namespace WebCore {

PassRefPtr<LinkImport> LinkImport::create(HTMLLinkElement* owner)
{
    return adoptRef(new LinkImport(owner));
}

LinkImport::LinkImport(HTMLLinkElement* owner)
    : LinkResource(owner)
{
}

LinkImport::~LinkImport()
{
}

Document* LinkImport::importedDocument() const
{
    if (!m_loader)
        return 0;
    return m_loader->importedDocument();
}

void LinkImport::process()
{
    if (m_loader)
        return;
    if (!m_owner)
        return;

    if (!m_owner->document()->frame() && !m_owner->document()->import())
        return;

    LinkRequestBuilder builder(m_owner);
    if (!builder.isValid())
        return;

    if (!m_owner->document()->import()) {
        ASSERT(m_owner->document()->frame()); // The document should be the master.
        HTMLImportsController::provideTo(m_owner->document());
    }

    HTMLImport* parent = m_owner->document()->import();
    HTMLImportsController* controller = parent->controller();
    if (RefPtr<HTMLImportLoader> found = controller->findLinkFor(builder.url())) {
        m_loader = found;
        return;
    }

    FetchRequest request = builder.build(true);
    request.setPotentiallyCrossOriginEnabled(controller->securityOrigin(), DoNotAllowStoredCredentials);
    ResourcePtr<CachedRawResource> resource = m_owner->document()->fetcher()->requestImport(request);
    if (!resource)
        return;

    m_loader = controller->createLoader(parent, builder.url(), resource);
}

void LinkImport::ownerRemoved()
{
    m_owner = 0;
    m_loader.clear();
}

} // namespace WebCore
