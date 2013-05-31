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
#include "core/html/HTMLImportsController.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Range.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLLinkElement.h"
#include "core/loader/cache/CachedResourceLoader.h"
#include "core/loader/cache/CachedScript.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

PassRefPtr<LinkImport> LinkImport::create(HTMLLinkElement* owner)
{
    return adoptRef(new LinkImport(owner));
}

LinkImport::LinkImport(HTMLLinkElement* owner)
    : LinkResource(owner)
    , m_controller(0)
    , m_ofSameLocation(0)
    , m_state(StatePreparing)
{
}

LinkImport::~LinkImport()
{
    if (m_resource)
        m_resource->removeClient(this);
}

LinkImport::State LinkImport::finish()
{
    if (!m_controller)
        return StateError;

    if (m_resource->loadFailedOrCanceled())
        return StateError;

    String error;
    if (!m_controller->securityOrigin()->canRequest(m_resource->response().url())
        && !m_resource->passesAccessControlCheck(m_controller->securityOrigin(), error)) {
        m_controller->showSecurityErrorMessage("Import from origin '" + SecurityOrigin::create(m_resource->response().url())->toString() + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + error);
        return StateError;
    }

    // FIXME(morrita): This should be done in incremental way.
    RefPtr<Document> parsingPlaceholder = HTMLDocument::create(0, KURL());
    parsingPlaceholder->setContent(m_resource->script());

    // Doctypes cannot be moved between documents. So we remove it before the migration.
    if (RefPtr<Node> doctype = parsingPlaceholder->doctype())
        parsingPlaceholder->removeChild(doctype.get());
    m_importedFragment->takeAllChildrenFrom(parsingPlaceholder.get());

    return StateReady;
}

void LinkImport::notifyFinished(CachedResource*)
{
    setState(finish());
}

void LinkImport::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;

    if ((m_state == StateReady  || m_state == StateError)
        && m_controller)
        m_controller->didLoad();
}

LinkImport::State LinkImport::startRequest()
{
    ASSERT(m_owner);
    ASSERT(m_state == StatePreparing);

    // FIXME(morrita): Should take care of sub-imports whose document doesn't have frame.
    if (!m_owner->document()->frame())
        return StateError;

    LinkRequestBuilder builder(m_owner);
    if (!builder.isValid())
        return StateError;

    m_controller = m_owner->document()->ensureImports();
    if (RefPtr<LinkImport> found = m_controller->findLinkFor(builder.url())) {
        m_ofSameLocation = found.get();
        return StateReady;
    }

    CachedResourceRequest request = builder.build(true);
    m_resource = m_owner->document()->cachedResourceLoader()->requestScript(request);
    if (!m_resource)
        return StateError;

    m_resource->addClient(this);
    m_url = builder.url();
    m_controller->addImport(this);
    m_importedFragment = m_controller->createDocumentFragment();

    return StateStarted;
}

DocumentFragment* LinkImport::importedFragment() const
{
    if (!m_owner)
        return 0;
    if (m_state != StateReady)
        return 0;

    if (m_ofSameLocation) {
        ASSERT(!m_importedFragment);
        return m_ofSameLocation->importedFragment();
    }

    return m_importedFragment.get();
}

void LinkImport::process()
{
    if (StatePreparing != m_state)
        return;
    setState(startRequest());
}

void LinkImport::ownerRemoved()
{
    m_owner = 0;
}

void LinkImport::importDestroyed()
{
    m_controller = 0;
    m_importedFragment.clear();
}

PassOwnPtr<HTMLImportsController> HTMLImportsController::create(Document* master)
{
    return adoptPtr(new HTMLImportsController(master));
}

HTMLImportsController::HTMLImportsController(Document* master)
    : m_master(master)
    , m_importedFragmentOwner(HTMLDocument::create(0, KURL()))
{
}

HTMLImportsController::~HTMLImportsController()
{
    for (size_t i = 0; i < m_imports.size(); ++i)
        m_imports[i]->importDestroyed();
}

void HTMLImportsController::addImport(PassRefPtr<LinkImport> link)
{
    ASSERT(!link->url().isEmpty() && link->url().isValid());
    m_imports.append(link);
}

void HTMLImportsController::showSecurityErrorMessage(const String& message)
{
    m_master->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message);
}

void HTMLImportsController::didLoad()
{
    if (haveLoaded())
        m_master->didLoadAllImports();
}

PassRefPtr<DocumentFragment> HTMLImportsController::createDocumentFragment() const
{
    return m_importedFragmentOwner->createDocumentFragment();
}

PassRefPtr<LinkImport> HTMLImportsController::findLinkFor(const KURL& url) const
{
    for (size_t i = 0; i < m_imports.size(); ++i) {
        if (m_imports[i]->url() == url)
            return m_imports[i];
    }

    return 0;
}

SecurityOrigin* HTMLImportsController::securityOrigin() const
{
    return m_master->securityOrigin();
}

bool HTMLImportsController::haveLoaded() const
{
    for (size_t i = 0; i < m_imports.size(); ++i) {
        if (!m_imports[i]->isDone())
            return false;
    }

    return true;
}

} // namespace WebCore
