// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkPreloadResourceClients_h
#define LinkPreloadResourceClients_h

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/RawResource.h"
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

class LinkPreloadImageResourceClient: public LinkPreloadResourceClient, public ResourceOwner<ImageResource, ImageResourceClient> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadImageResourceClient);
    USING_FAST_MALLOC_WILL_BE_REMOVED(LinkPreloadImageResourceClient);
public:
    static PassOwnPtrWillBeRawPtr<LinkPreloadImageResourceClient> create(LinkLoader* loader, ImageResource* resource)
    {
        return adoptPtrWillBeNoop(new LinkPreloadImageResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadImage"; }
    virtual ~LinkPreloadImageResourceClient() { }

    void clear() override { clearResource(); }

    void notifyFinished(Resource* resource) override
    {
        ASSERT(this->resource() == toImageResource(resource));
        triggerEvents(resource);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        LinkPreloadResourceClient::trace(visitor);
        ResourceOwner<ImageResource, ImageResourceClient>::trace(visitor);
    }

private:
    LinkPreloadImageResourceClient(LinkLoader* loader, ImageResource* resource)
        : LinkPreloadResourceClient(loader)
    {
        setResource(resource);
    }
};

class LinkPreloadFontResourceClient: public LinkPreloadResourceClient, public ResourceOwner<FontResource, FontResourceClient> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadFontResourceClient);
    USING_FAST_MALLOC_WILL_BE_REMOVED(LinkPreloadFontResourceClient);
public:
    static PassOwnPtrWillBeRawPtr<LinkPreloadFontResourceClient> create(LinkLoader* loader, FontResource* resource)
    {
        return adoptPtrWillBeNoop(new LinkPreloadFontResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadFont"; }
    virtual ~LinkPreloadFontResourceClient() { }

    void clear() override { clearResource(); }

    void fontLoaded(FontResource* resource) override
    {
        ASSERT(this->resource() == resource);
        triggerEvents(resource);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        LinkPreloadResourceClient::trace(visitor);
        ResourceOwner<FontResource, FontResourceClient>::trace(visitor);
    }

private:
    LinkPreloadFontResourceClient(LinkLoader* loader, FontResource* resource)
        : LinkPreloadResourceClient(loader)
    {
        setResource(resource);
    }
};

class LinkPreloadRawResourceClient: public LinkPreloadResourceClient, public ResourceOwner<RawResource, RawResourceClient> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadRawResourceClient);
    USING_FAST_MALLOC_WILL_BE_REMOVED(LinkPreloadRawResourceClient);
public:
    static PassOwnPtrWillBeRawPtr<LinkPreloadRawResourceClient> create(LinkLoader* loader, RawResource* resource)
    {
        return adoptPtrWillBeNoop(new LinkPreloadRawResourceClient(loader, resource));
    }

    virtual String debugName() const { return "LinkPreloadRaw"; }
    virtual ~LinkPreloadRawResourceClient() { }

    void clear() override { clearResource(); }

    void notifyFinished(Resource* resource) override
    {
        ASSERT(this->resource() == toRawResource(resource));
        triggerEvents(resource);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        LinkPreloadResourceClient::trace(visitor);
        ResourceOwner<RawResource, RawResourceClient>::trace(visitor);
    }

private:
    LinkPreloadRawResourceClient(LinkLoader* loader, RawResource* resource)
        : LinkPreloadResourceClient(loader)
    {
        setResource(resource);
    }
};

} // namespace blink

#endif // LinkPreloadResourceClients_h
