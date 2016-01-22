// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkPreloadResourceClients_h
#define LinkPreloadResourceClients_h

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/ScriptResource.h"
#include "core/fetch/StyleSheetResourceClient.h"

namespace blink {

class LinkLoader;

class LinkPreloadResourceClient : public NoBaseWillBeGarbageCollectedFinalized<LinkPreloadResourceClient> {
public:
    virtual ~LinkPreloadResourceClient() { }

    void triggerEvents(const Resource*);
    virtual void clear() = 0;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_loader);
    }

protected:
    LinkPreloadResourceClient(LinkLoader* loader)
        : m_loader(loader)
    {
        ASSERT(loader);
    }

private:
    RawPtrWillBeMember<LinkLoader> m_loader;
};

class LinkPreloadScriptResourceClient: public LinkPreloadResourceClient, public ResourceOwner<ScriptResource, ScriptResourceClient> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadScriptResourceClient);
    USING_FAST_MALLOC_WILL_BE_REMOVED(LinkPreloadScriptResourceClient);
public:
    static PassOwnPtrWillBeRawPtr<LinkPreloadScriptResourceClient> create(LinkLoader* loader, ScriptResource* resource)
    {
        return adoptPtrWillBeNoop(new LinkPreloadScriptResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadScript"; }
    virtual ~LinkPreloadScriptResourceClient() { }

    void clear() override { clearResource(); }

    void notifyFinished(Resource* resource) override
    {
        ASSERT(this->resource() == resource);
        triggerEvents(resource);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        LinkPreloadResourceClient::trace(visitor);
        ResourceOwner<ScriptResource, ScriptResourceClient>::trace(visitor);
    }

private:
    LinkPreloadScriptResourceClient(LinkLoader* loader, ScriptResource* resource)
        : LinkPreloadResourceClient(loader)
    {
        setResource(resource);
    }
};

class LinkPreloadStyleResourceClient: public LinkPreloadResourceClient, public ResourceOwner<CSSStyleSheetResource, StyleSheetResourceClient> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadStyleResourceClient);
    USING_FAST_MALLOC_WILL_BE_REMOVED(LinkPreloadStyleResourceClient);
public:
    static PassOwnPtrWillBeRawPtr<LinkPreloadStyleResourceClient> create(LinkLoader* loader, CSSStyleSheetResource* resource)
    {
        return adoptPtrWillBeNoop(new LinkPreloadStyleResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadStyle"; }
    virtual ~LinkPreloadStyleResourceClient() { }

    void clear() override { clearResource(); }

    void setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource* resource) override
    {
        ASSERT(this->resource() == resource);
        triggerEvents(static_cast<const Resource*>(resource));
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        LinkPreloadResourceClient::trace(visitor);
        ResourceOwner<CSSStyleSheetResource, StyleSheetResourceClient>::trace(visitor);
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
