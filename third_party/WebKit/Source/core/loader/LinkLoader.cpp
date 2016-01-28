/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#include "core/loader/LinkLoader.h"

#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/LinkFetchResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/LinkRelAttribute.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/LinkHeader.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/PrerenderHandle.h"
#include "platform/Prerender.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/NetworkHints.h"
#include "public/platform/WebPrerender.h"

namespace blink {

static unsigned prerenderRelTypesFromRelAttribute(const LinkRelAttribute& relAttribute, Document& document)
{
    unsigned result = 0;
    if (relAttribute.isLinkPrerender()) {
        result |= PrerenderRelTypePrerender;
        UseCounter::count(document, UseCounter::LinkRelPrerender);
    }
    if (relAttribute.isLinkNext()) {
        result |= PrerenderRelTypeNext;
        UseCounter::count(document, UseCounter::LinkRelNext);
    }

    return result;
}

LinkLoader::LinkLoader(LinkLoaderClient* client)
    : m_client(client)
    , m_linkLoadTimer(this, &LinkLoader::linkLoadTimerFired)
    , m_linkLoadingErrorTimer(this, &LinkLoader::linkLoadingErrorTimerFired)
{
}

LinkLoader::~LinkLoader()
{
}

void LinkLoader::linkLoadTimerFired(Timer<LinkLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_linkLoadTimer);
    m_client->linkLoaded();
}

void LinkLoader::linkLoadingErrorTimerFired(Timer<LinkLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_linkLoadingErrorTimer);
    m_client->linkLoadingErrored();
}

void LinkLoader::triggerEvents(const Resource* resource)
{
    if (resource->errorOccurred())
        m_linkLoadingErrorTimer.startOneShot(0, BLINK_FROM_HERE);
    else
        m_linkLoadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void LinkLoader::notifyFinished(Resource* resource)
{
    ASSERT(this->resource() == resource);

    triggerEvents(resource);
    clearResource();
}

void LinkLoader::didStartPrerender()
{
    m_client->didStartLinkPrerender();
}

void LinkLoader::didStopPrerender()
{
    m_client->didStopLinkPrerender();
}

void LinkLoader::didSendLoadForPrerender()
{
    m_client->didSendLoadForLinkPrerender();
}

void LinkLoader::didSendDOMContentLoadedForPrerender()
{
    m_client->didSendDOMContentLoadedForLinkPrerender();
}

enum LinkCaller {
    LinkCalledFromHeader,
    LinkCalledFromMarkup,
};


static void dnsPrefetchIfNeeded(const LinkRelAttribute& relAttribute, const KURL& href, Document& document, const NetworkHintsInterface& networkHintsInterface, LinkCaller caller)
{
    if (relAttribute.isDNSPrefetch()) {
        UseCounter::count(document, UseCounter::LinkRelDnsPrefetch);
        if (caller == LinkCalledFromHeader)
            UseCounter::count(document, UseCounter::LinkHeaderDnsPrefetch);
        Settings* settings = document.settings();
        // FIXME: The href attribute of the link element can be in "//hostname" form, and we shouldn't attempt
        // to complete that as URL <https://bugs.webkit.org/show_bug.cgi?id=48857>.
        if (settings && settings->dnsPrefetchingEnabled() && href.isValid() && !href.isEmpty()) {
            if (settings->logDnsPrefetchAndPreconnect())
                document.addConsoleMessage(ConsoleMessage::create(OtherMessageSource, DebugMessageLevel, String("DNS prefetch triggered for " + href.host())));
            networkHintsInterface.dnsPrefetchHost(href.host());
        }
    }
}

static void preconnectIfNeeded(const LinkRelAttribute& relAttribute, const KURL& href, Document& document, const CrossOriginAttributeValue crossOrigin, const NetworkHintsInterface& networkHintsInterface, LinkCaller caller)
{
    if (relAttribute.isPreconnect() && href.isValid() && href.protocolIsInHTTPFamily()) {
        UseCounter::count(document, UseCounter::LinkRelPreconnect);
        if (caller == LinkCalledFromHeader)
            UseCounter::count(document, UseCounter::LinkHeaderPreconnect);
        ASSERT(RuntimeEnabledFeatures::linkPreconnectEnabled());
        Settings* settings = document.settings();
        if (settings && settings->logDnsPrefetchAndPreconnect()) {
            document.addConsoleMessage(ConsoleMessage::create(OtherMessageSource, DebugMessageLevel, String("Preconnect triggered for ") + href.string()));
            if (crossOrigin != CrossOriginAttributeNotSet) {
                document.addConsoleMessage(ConsoleMessage::create(OtherMessageSource, DebugMessageLevel,
                    String("Preconnect CORS setting is ") + String((crossOrigin == CrossOriginAttributeAnonymous) ? "anonymous" : "use-credentials")));
            }
        }
        networkHintsInterface.preconnectHost(href, crossOrigin);
    }
}

Resource::Type LinkLoader::getTypeFromAsAttribute(const String& as, Document* document)
{
    if (equalIgnoringCase(as, "image"))
        return Resource::Image;
    if (equalIgnoringCase(as, "script"))
        return Resource::Script;
    if (equalIgnoringCase(as, "style"))
        return Resource::CSSStyleSheet;
    if (equalIgnoringCase(as, "audio") || equalIgnoringCase(as, "video"))
        return Resource::Media;
    if (equalIgnoringCase(as, "font"))
        return Resource::Font;
    if (equalIgnoringCase(as, "track"))
        return Resource::TextTrack;
    if (document && !as.isEmpty())
        document->addConsoleMessage(ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, String("<link rel=preload> must have a valid `as` value")));
    return Resource::LinkPreload;
}

void LinkLoader::createLinkPreloadResourceClient(ResourcePtr<Resource> resource)
{
    if (!resource)
        return;
    switch (resource->type()) {
    case Resource::Image:
        m_linkPreloadResourceClient = LinkPreloadImageResourceClient::create(this, toImageResource(resource.get()));
        break;
    case Resource::Script:
        m_linkPreloadResourceClient = LinkPreloadScriptResourceClient::create(this, toScriptResource(resource.get()));
        break;
    case Resource::CSSStyleSheet:
        m_linkPreloadResourceClient = LinkPreloadStyleResourceClient::create(this, toCSSStyleSheetResource(resource.get()));
        break;
    case Resource::Font:
        m_linkPreloadResourceClient = LinkPreloadFontResourceClient::create(this, toFontResource(resource.get()));
        break;
    case Resource::Media:
    case Resource::TextTrack:
    case Resource::Raw:
    case Resource::LinkPreload:
        m_linkPreloadResourceClient = LinkPreloadRawResourceClient::create(this, toRawResource(resource.get()));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

static ResourcePtr<Resource> preloadIfNeeded(const LinkRelAttribute& relAttribute, const KURL& href, Document& document, const String& as, LinkCaller caller)
{
    if (!document.loader() || !relAttribute.isLinkPreload())
        return nullptr;

    UseCounter::count(document, UseCounter::LinkRelPreload);
    ASSERT(RuntimeEnabledFeatures::linkPreloadEnabled());
    if (!href.isValid() || href.isEmpty()) {
        document.addConsoleMessage(ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, String("<link rel=preload> has an invalid `href` value")));
        return nullptr;
    }
    if (caller == LinkCalledFromHeader)
        UseCounter::count(document, UseCounter::LinkHeaderPreload);
    Resource::Type type = LinkLoader::getTypeFromAsAttribute(as, &document);
    ResourceRequest resourceRequest(document.completeURL(href));
    ResourceFetcher::determineRequestContext(resourceRequest, type, false);
    FetchRequest linkRequest(resourceRequest, FetchInitiatorTypeNames::link);

    linkRequest.setPriority(document.fetcher()->loadPriority(type, linkRequest));
    Settings* settings = document.settings();
    if (settings && settings->logPreload())
        document.addConsoleMessage(ConsoleMessage::create(OtherMessageSource, DebugMessageLevel, String("Preload triggered for " + href.host() + href.path())));
    linkRequest.setForPreload(true);
    linkRequest.setAvoidBlockingOnLoad(true);
    return document.loader()->startPreload(type, linkRequest);
}

bool LinkLoader::loadLinkFromHeader(const String& headerValue, Document* document, const NetworkHintsInterface& networkHintsInterface, CanLoadResources canLoadResources)
{
    if (!document)
        return false;
    LinkHeaderSet headerSet(headerValue);
    for (auto& header : headerSet) {
        if (!header.valid() || header.url().isEmpty() || header.rel().isEmpty())
            return false;

        LinkRelAttribute relAttribute(header.rel());
        KURL url = document->completeURL(header.url());
        if (canLoadResources != OnlyLoadResources) {
            if (RuntimeEnabledFeatures::linkHeaderEnabled())
                dnsPrefetchIfNeeded(relAttribute, url, *document, networkHintsInterface, LinkCalledFromHeader);

            if (RuntimeEnabledFeatures::linkPreconnectEnabled())
                preconnectIfNeeded(relAttribute, url, *document, header.crossOrigin(), networkHintsInterface, LinkCalledFromHeader);
        }
        if (canLoadResources != DoNotLoadResources) {
            if (RuntimeEnabledFeatures::linkPreloadEnabled())
                preloadIfNeeded(relAttribute, url, *document, header.as(), LinkCalledFromHeader);
        }
        // TODO(yoav): Add more supported headers as needed.
    }
    return true;
}

bool LinkLoader::loadLink(const LinkRelAttribute& relAttribute, CrossOriginAttributeValue crossOrigin, const String& type, const String& as, const KURL& href, Document& document, const NetworkHintsInterface& networkHintsInterface)
{
    // TODO(yoav): Do all links need to load only after they're in document???

    // TODO(yoav): Convert all uses of the CrossOriginAttribute to CrossOriginAttributeValue. crbug.com/486689
    // FIXME(crbug.com/463266): We're ignoring type here. Maybe we shouldn't.
    dnsPrefetchIfNeeded(relAttribute, href, document, networkHintsInterface, LinkCalledFromMarkup);

    preconnectIfNeeded(relAttribute, href, document, crossOrigin, networkHintsInterface, LinkCalledFromMarkup);

    if (m_client->shouldLoadLink())
        createLinkPreloadResourceClient(preloadIfNeeded(relAttribute, href, document, as, LinkCalledFromMarkup));

    if (href.isEmpty() || !href.isValid())
        released();

    // FIXME(crbug.com/323096): Should take care of import.
    if (relAttribute.isLinkPrefetch() && href.isValid() && document.frame()) {
        if (!m_client->shouldLoadLink())
            return false;
        UseCounter::count(document, UseCounter::LinkRelPrefetch);

        FetchRequest linkRequest(ResourceRequest(document.completeURL(href)), FetchInitiatorTypeNames::link);
        if (crossOrigin != CrossOriginAttributeNotSet)
            linkRequest.setCrossOriginAccessControl(document.securityOrigin(), crossOrigin);
        setResource(LinkFetchResource::fetch(Resource::LinkPrefetch, linkRequest, document.fetcher()));
    }

    if (const unsigned prerenderRelTypes = prerenderRelTypesFromRelAttribute(relAttribute, document)) {
        if (!m_prerender) {
            m_prerender = PrerenderHandle::create(document, this, href, prerenderRelTypes);
        } else if (m_prerender->url() != href) {
            m_prerender->cancel();
            m_prerender = PrerenderHandle::create(document, this, href, prerenderRelTypes);
        }
        // TODO(gavinp): Handle changes to rel types of existing prerenders.
    } else if (m_prerender) {
        m_prerender->cancel();
        m_prerender.clear();
    }
    return true;
}

void LinkLoader::released()
{
    // Only prerenders need treatment here; other links either use the Resource interface, or are notionally
    // atomic (dns prefetch).
    if (m_prerender) {
        m_prerender->cancel();
        m_prerender.clear();
    }
    if (m_linkPreloadResourceClient)
        m_linkPreloadResourceClient->clear();
}

DEFINE_TRACE(LinkLoader)
{
    visitor->trace(m_client);
    visitor->trace(m_prerender);
    visitor->trace(m_linkPreloadResourceClient);
    ResourceOwner<Resource, ResourceClient>::trace(visitor);
}

} // namespace blink
