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

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/dom/Document.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/parser/HTMLPreloadScanner.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/private/PrerenderHandle.h"
#include "core/loader/resource/LinkFetchResource.h"
#include "platform/Prerender.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/LinkHeader.h"
#include "platform/network/NetworkHints.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "public/platform/WebPrerender.h"

namespace blink {

static unsigned prerenderRelTypesFromRelAttribute(
    const LinkRelAttribute& relAttribute,
    Document& document) {
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

LinkLoader::LinkLoader(LinkLoaderClient* client,
                       RefPtr<WebTaskRunner> taskRunner)
    : m_client(client),
      m_linkLoadTimer(taskRunner, this, &LinkLoader::linkLoadTimerFired),
      m_linkLoadingErrorTimer(taskRunner,
                              this,
                              &LinkLoader::linkLoadingErrorTimerFired) {
  DCHECK(m_client);
}

LinkLoader::~LinkLoader() {}

void LinkLoader::linkLoadTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &m_linkLoadTimer);
  m_client->linkLoaded();
}

void LinkLoader::linkLoadingErrorTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &m_linkLoadingErrorTimer);
  m_client->linkLoadingErrored();
}

void LinkLoader::triggerEvents(const Resource* resource) {
  if (resource->errorOccurred())
    m_linkLoadingErrorTimer.startOneShot(0, BLINK_FROM_HERE);
  else
    m_linkLoadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void LinkLoader::notifyFinished(Resource* resource) {
  DCHECK_EQ(this->resource(), resource);

  triggerEvents(resource);
  clearResource();
}

void LinkLoader::didStartPrerender() {
  m_client->didStartLinkPrerender();
}

void LinkLoader::didStopPrerender() {
  m_client->didStopLinkPrerender();
}

void LinkLoader::didSendLoadForPrerender() {
  m_client->didSendLoadForLinkPrerender();
}

void LinkLoader::didSendDOMContentLoadedForPrerender() {
  m_client->didSendDOMContentLoadedForLinkPrerender();
}

enum LinkCaller {
  LinkCalledFromHeader,
  LinkCalledFromMarkup,
};

static void dnsPrefetchIfNeeded(
    const LinkRelAttribute& relAttribute,
    const KURL& href,
    Document& document,
    const NetworkHintsInterface& networkHintsInterface,
    LinkCaller caller) {
  if (relAttribute.isDNSPrefetch()) {
    UseCounter::count(document, UseCounter::LinkRelDnsPrefetch);
    if (caller == LinkCalledFromHeader)
      UseCounter::count(document, UseCounter::LinkHeaderDnsPrefetch);
    Settings* settings = document.settings();
    // FIXME: The href attribute of the link element can be in "//hostname"
    // form, and we shouldn't attempt to complete that as URL
    // <https://bugs.webkit.org/show_bug.cgi?id=48857>.
    if (settings && settings->getDNSPrefetchingEnabled() && href.isValid() &&
        !href.isEmpty()) {
      if (settings->getLogDnsPrefetchAndPreconnect()) {
        document.addConsoleMessage(ConsoleMessage::create(
            OtherMessageSource, VerboseMessageLevel,
            String("DNS prefetch triggered for " + href.host())));
      }
      networkHintsInterface.dnsPrefetchHost(href.host());
    }
  }
}

static void preconnectIfNeeded(
    const LinkRelAttribute& relAttribute,
    const KURL& href,
    Document& document,
    const CrossOriginAttributeValue crossOrigin,
    const NetworkHintsInterface& networkHintsInterface,
    LinkCaller caller) {
  if (relAttribute.isPreconnect() && href.isValid() &&
      href.protocolIsInHTTPFamily()) {
    UseCounter::count(document, UseCounter::LinkRelPreconnect);
    if (caller == LinkCalledFromHeader)
      UseCounter::count(document, UseCounter::LinkHeaderPreconnect);
    Settings* settings = document.settings();
    if (settings && settings->getLogDnsPrefetchAndPreconnect()) {
      document.addConsoleMessage(ConsoleMessage::create(
          OtherMessageSource, VerboseMessageLevel,
          String("Preconnect triggered for ") + href.getString()));
      if (crossOrigin != CrossOriginAttributeNotSet) {
        document.addConsoleMessage(ConsoleMessage::create(
            OtherMessageSource, VerboseMessageLevel,
            String("Preconnect CORS setting is ") +
                String((crossOrigin == CrossOriginAttributeAnonymous)
                           ? "anonymous"
                           : "use-credentials")));
      }
    }
    networkHintsInterface.preconnectHost(href, crossOrigin);
  }
}

WTF::Optional<Resource::Type> LinkLoader::getResourceTypeFromAsAttribute(
    const String& as) {
  DCHECK_EQ(as.lower(), as);
  if (as == "image") {
    return Resource::Image;
  } else if (as == "script") {
    return Resource::Script;
  } else if (as == "style") {
    return Resource::CSSStyleSheet;
  } else if (as == "media") {
    return Resource::Media;
  } else if (as == "font") {
    return Resource::Font;
  } else if (as == "track") {
    return Resource::TextTrack;
  } else if (as.isEmpty()) {
    return Resource::Raw;
  }
  return WTF::nullopt;
}

void LinkLoader::createLinkPreloadResourceClient(Resource* resource) {
  if (!resource)
    return;
  switch (resource->getType()) {
    case Resource::Image:
      m_linkPreloadResourceClient = LinkPreloadImageResourceClient::create(
          this, toImageResource(resource));
      break;
    case Resource::Script:
      m_linkPreloadResourceClient = LinkPreloadScriptResourceClient::create(
          this, toScriptResource(resource));
      break;
    case Resource::CSSStyleSheet:
      m_linkPreloadResourceClient = LinkPreloadStyleResourceClient::create(
          this, toCSSStyleSheetResource(resource));
      break;
    case Resource::Font:
      m_linkPreloadResourceClient =
          LinkPreloadFontResourceClient::create(this, toFontResource(resource));
      break;
    case Resource::Media:
    case Resource::TextTrack:
    case Resource::Raw:
      m_linkPreloadResourceClient =
          LinkPreloadRawResourceClient::create(this, toRawResource(resource));
      break;
    default:
      NOTREACHED();
  }
}

static bool isSupportedType(Resource::Type resourceType,
                            const String& mimeType) {
  if (mimeType.isEmpty())
    return true;
  switch (resourceType) {
    case Resource::Image:
      return MIMETypeRegistry::isSupportedImagePrefixedMIMEType(mimeType);
    case Resource::Script:
      return MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType);
    case Resource::CSSStyleSheet:
      return MIMETypeRegistry::isSupportedStyleSheetMIMEType(mimeType);
    case Resource::Font:
      return MIMETypeRegistry::isSupportedFontMIMEType(mimeType);
    case Resource::Media:
      return MIMETypeRegistry::isSupportedMediaMIMEType(mimeType, String());
    case Resource::TextTrack:
      return MIMETypeRegistry::isSupportedTextTrackMIMEType(mimeType);
    case Resource::Raw:
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

static Resource* preloadIfNeeded(const LinkRelAttribute& relAttribute,
                                 const KURL& href,
                                 Document& document,
                                 const String& as,
                                 const String& mimeType,
                                 const String& media,
                                 CrossOriginAttributeValue crossOrigin,
                                 LinkCaller caller,
                                 bool& errorOccurred,
                                 ViewportDescription* viewportDescription,
                                 ReferrerPolicy referrerPolicy) {
  if (!document.loader() || !relAttribute.isLinkPreload())
    return nullptr;

  UseCounter::count(document, UseCounter::LinkRelPreload);
  if (!href.isValid() || href.isEmpty()) {
    document.addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, WarningMessageLevel,
        String("<link rel=preload> has an invalid `href` value")));
    return nullptr;
  }

  if (!media.isEmpty()) {
    MediaValues* mediaValues =
        MediaValues::createDynamicIfFrameExists(document.frame());
    if (viewportDescription) {
      mediaValues->overrideViewportDimensions(
          viewportDescription->maxWidth.getFloatValue(),
          viewportDescription->maxHeight.getFloatValue());
    }

    // Preload only if media matches
    MediaQuerySet* mediaQueries = MediaQuerySet::create(media);
    MediaQueryEvaluator evaluator(*mediaValues);
    if (!evaluator.eval(mediaQueries))
      return nullptr;
  }
  if (caller == LinkCalledFromHeader)
    UseCounter::count(document, UseCounter::LinkHeaderPreload);
  Optional<Resource::Type> resourceType =
      LinkLoader::getResourceTypeFromAsAttribute(as);
  if (resourceType == WTF::nullopt) {
    document.addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, WarningMessageLevel,
        String("<link rel=preload> must have a valid `as` value")));
    errorOccurred = true;
    return nullptr;
  }

  if (!isSupportedType(resourceType.value(), mimeType)) {
    document.addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, WarningMessageLevel,
        String("<link rel=preload> has an unsupported `type` value")));
    return nullptr;
  }
  ResourceRequest resourceRequest(document.completeURL(href));
  ResourceFetcher::determineRequestContext(resourceRequest,
                                           resourceType.value(), false);

  if (referrerPolicy != ReferrerPolicyDefault) {
    resourceRequest.setHTTPReferrer(SecurityPolicy::generateReferrer(
        referrerPolicy, href, document.outgoingReferrer()));
  }

  FetchRequest linkRequest(resourceRequest, FetchInitiatorTypeNames::link,
                           document.encodingName());

  if (crossOrigin != CrossOriginAttributeNotSet) {
    linkRequest.setCrossOriginAccessControl(document.getSecurityOrigin(),
                                            crossOrigin);
  }
  Settings* settings = document.settings();
  if (settings && settings->getLogPreload()) {
    document.addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, VerboseMessageLevel,
        String("Preload triggered for " + href.host() + href.path())));
  }
  linkRequest.setLinkPreload(true);
  return document.loader()->startPreload(resourceType.value(), linkRequest);
}

static Resource* prefetchIfNeeded(Document& document,
                                  const KURL& href,
                                  const LinkRelAttribute& relAttribute,
                                  CrossOriginAttributeValue crossOrigin,
                                  ReferrerPolicy referrerPolicy) {
  if (relAttribute.isLinkPrefetch() && href.isValid() && document.frame()) {
    UseCounter::count(document, UseCounter::LinkRelPrefetch);

    FetchRequest linkRequest(ResourceRequest(document.completeURL(href)),
                             FetchInitiatorTypeNames::link);
    if (referrerPolicy != ReferrerPolicyDefault) {
      linkRequest.mutableResourceRequest().setHTTPReferrer(
          SecurityPolicy::generateReferrer(referrerPolicy, href,
                                           document.outgoingReferrer()));
    }
    if (crossOrigin != CrossOriginAttributeNotSet) {
      linkRequest.setCrossOriginAccessControl(document.getSecurityOrigin(),
                                              crossOrigin);
    }
    return LinkFetchResource::fetch(Resource::LinkPrefetch, linkRequest,
                                    document.fetcher());
  }
  return nullptr;
}

void LinkLoader::loadLinksFromHeader(
    const String& headerValue,
    const KURL& baseURL,
    Document* document,
    const NetworkHintsInterface& networkHintsInterface,
    CanLoadResources canLoadResources,
    MediaPreloadPolicy mediaPolicy,
    ViewportDescriptionWrapper* viewportDescriptionWrapper) {
  if (!document || headerValue.isEmpty())
    return;
  LinkHeaderSet headerSet(headerValue);
  for (auto& header : headerSet) {
    if (!header.valid() || header.url().isEmpty() || header.rel().isEmpty())
      continue;

    if (mediaPolicy == OnlyLoadMedia && header.media().isEmpty())
      continue;
    if (mediaPolicy == OnlyLoadNonMedia && !header.media().isEmpty())
      continue;

    LinkRelAttribute relAttribute(header.rel());
    KURL url(baseURL, header.url());
    // Sanity check to avoid re-entrancy here.
    if (url == baseURL)
      continue;
    if (canLoadResources != OnlyLoadResources) {
      dnsPrefetchIfNeeded(relAttribute, url, *document, networkHintsInterface,
                          LinkCalledFromHeader);

      preconnectIfNeeded(relAttribute, url, *document,
                         crossOriginAttributeValue(header.crossOrigin()),
                         networkHintsInterface, LinkCalledFromHeader);
    }
    if (canLoadResources != DoNotLoadResources) {
      bool errorOccurred = false;
      ViewportDescription* viewportDescription =
          (viewportDescriptionWrapper && viewportDescriptionWrapper->set)
              ? &(viewportDescriptionWrapper->description)
              : nullptr;

      CrossOriginAttributeValue crossOrigin =
          crossOriginAttributeValue(header.crossOrigin());
      preloadIfNeeded(relAttribute, url, *document, header.as(),
                      header.mimeType(), header.media(), crossOrigin,
                      LinkCalledFromHeader, errorOccurred, viewportDescription,
                      ReferrerPolicyDefault);
      prefetchIfNeeded(*document, url, relAttribute, crossOrigin,
                       ReferrerPolicyDefault);
    }
    if (relAttribute.isServiceWorker()) {
      UseCounter::count(*document, UseCounter::LinkHeaderServiceWorker);
    }
    // TODO(yoav): Add more supported headers as needed.
  }
}

bool LinkLoader::loadLink(const LinkRelAttribute& relAttribute,
                          CrossOriginAttributeValue crossOrigin,
                          const String& type,
                          const String& as,
                          const String& media,
                          ReferrerPolicy referrerPolicy,
                          const KURL& href,
                          Document& document,
                          const NetworkHintsInterface& networkHintsInterface) {
  if (!m_client->shouldLoadLink())
    return false;

  dnsPrefetchIfNeeded(relAttribute, href, document, networkHintsInterface,
                      LinkCalledFromMarkup);

  preconnectIfNeeded(relAttribute, href, document, crossOrigin,
                     networkHintsInterface, LinkCalledFromMarkup);

  bool errorOccurred = false;
  createLinkPreloadResourceClient(preloadIfNeeded(
      relAttribute, href, document, as, type, media, crossOrigin,
      LinkCalledFromMarkup, errorOccurred, nullptr, referrerPolicy));
  if (errorOccurred)
    m_linkLoadingErrorTimer.startOneShot(0, BLINK_FROM_HERE);

  if (href.isEmpty() || !href.isValid())
    released();

  Resource* resource = prefetchIfNeeded(document, href, relAttribute,
                                        crossOrigin, referrerPolicy);
  if (resource)
    setResource(resource);

  if (const unsigned prerenderRelTypes =
          prerenderRelTypesFromRelAttribute(relAttribute, document)) {
    if (!m_prerender) {
      m_prerender =
          PrerenderHandle::create(document, this, href, prerenderRelTypes);
    } else if (m_prerender->url() != href) {
      m_prerender->cancel();
      m_prerender =
          PrerenderHandle::create(document, this, href, prerenderRelTypes);
    }
    // TODO(gavinp): Handle changes to rel types of existing prerenders.
  } else if (m_prerender) {
    m_prerender->cancel();
    m_prerender.clear();
  }
  return true;
}

void LinkLoader::released() {
  // Only prerenders need treatment here; other links either use the Resource
  // interface, or are notionally atomic (dns prefetch).
  if (m_prerender) {
    m_prerender->cancel();
    m_prerender.clear();
  }
  if (m_linkPreloadResourceClient)
    m_linkPreloadResourceClient->clear();
}

DEFINE_TRACE(LinkLoader) {
  visitor->trace(m_client);
  visitor->trace(m_prerender);
  visitor->trace(m_linkPreloadResourceClient);
  ResourceOwner<Resource, ResourceClient>::trace(visitor);
  PrerenderClient::trace(visitor);
}

}  // namespace blink
