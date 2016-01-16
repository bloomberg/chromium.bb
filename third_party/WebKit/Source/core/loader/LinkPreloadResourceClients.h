// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkPreloadResourceClients_h
#define LinkPreloadResourceClients_h

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/ScriptResource.h"
#include "core/fetch/StyleSheetResourceClient.h"

namespace blink {

class LinkLoader;

class LinkPreloadResourceClient : public NoBaseWillBeGarbageCollectedFinalized<LinkPreloadResourceClient> {
public:
    virtual ~LinkPreloadResourceClient() { }

    void triggerEvents(const Resource*) const;

protected:
    LinkPreloadResourceClient(LinkLoader* loader)
        : m_loader(loader)
    {
        ASSERT(loader);
    }

private:
    LinkLoader* m_loader;
};

class LinkPreloadScriptResourceClient: public LinkPreloadResourceClient, public ResourceOwner<ScriptResource, ScriptResourceClient> {
public:
    static PassOwnPtr<LinkPreloadScriptResourceClient> create(LinkLoader* loader, ScriptResource* resource)
    {
        return adoptPtr(new LinkPreloadScriptResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadScript"; }

    void notifyFinished(Resource* resource) override
    {
        ASSERT(this->resource() == resource);
        triggerEvents(resource);
    }

private:
    LinkPreloadScriptResourceClient(LinkLoader* loader, ScriptResource* resource)
        : LinkPreloadResourceClient(loader)
    {
        setResource(resource);
    }
};

class LinkPreloadStyleResourceClient: public LinkPreloadResourceClient, public ResourceOwner<CSSStyleSheetResource, StyleSheetResourceClient> {
public:
    static PassOwnPtr<LinkPreloadStyleResourceClient> create(LinkLoader* loader, CSSStyleSheetResource* resource)
    {
        return adoptPtr(new LinkPreloadStyleResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadStyle"; }

    void setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource* resource) override
    {
        ASSERT(this->resource() == resource);
        triggerEvents(static_cast<const Resource*>(resource));
    }

private:
    LinkPreloadStyleResourceClient(LinkLoader* loader, CSSStyleSheetResource* resource)
        : LinkPreloadResourceClient(loader)
    {
        setResource(resource);
    }
};

}

#endif // LinkPreloadResourceClients_h
