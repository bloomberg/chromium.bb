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
#include "platform/Timer.h"
#include "platform/loader/LinkHeader.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "public/platform/WebPrerender.h"

namespace blink {

static unsigned PrerenderRelTypesFromRelAttribute(
    const LinkRelAttribute& rel_attribute,
    Document& document) {
  unsigned result = 0;
  if (rel_attribute.IsLinkPrerender()) {
    result |= kPrerenderRelTypePrerender;
    UseCounter::Count(document, UseCounter::kLinkRelPrerender);
  }
  if (rel_attribute.IsLinkNext()) {
    result |= kPrerenderRelTypeNext;
    UseCounter::Count(document, UseCounter::kLinkRelNext);
  }

  return result;
}

LinkLoader::LinkLoader(LinkLoaderClient* client,
                       RefPtr<WebTaskRunner> task_runner)
    : client_(client),
      link_load_timer_(task_runner, this, &LinkLoader::LinkLoadTimerFired),
      link_loading_error_timer_(task_runner,
                                this,
                                &LinkLoader::LinkLoadingErrorTimerFired) {
  DCHECK(client_);
}

LinkLoader::~LinkLoader() {}

void LinkLoader::LinkLoadTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &link_load_timer_);
  client_->LinkLoaded();
}

void LinkLoader::LinkLoadingErrorTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &link_loading_error_timer_);
  client_->LinkLoadingErrored();
}

void LinkLoader::TriggerEvents(const Resource* resource) {
  if (resource->ErrorOccurred())
    link_loading_error_timer_.StartOneShot(0, BLINK_FROM_HERE);
  else
    link_load_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void LinkLoader::NotifyFinished(Resource* resource) {
  DCHECK_EQ(this->GetResource(), resource);

  TriggerEvents(resource);
  ClearResource();
}

void LinkLoader::DidStartPrerender() {
  client_->DidStartLinkPrerender();
}

void LinkLoader::DidStopPrerender() {
  client_->DidStopLinkPrerender();
}

void LinkLoader::DidSendLoadForPrerender() {
  client_->DidSendLoadForLinkPrerender();
}

void LinkLoader::DidSendDOMContentLoadedForPrerender() {
  client_->DidSendDOMContentLoadedForLinkPrerender();
}

enum LinkCaller {
  kLinkCalledFromHeader,
  kLinkCalledFromMarkup,
};

static void DnsPrefetchIfNeeded(
    const LinkRelAttribute& rel_attribute,
    const KURL& href,
    Document& document,
    const NetworkHintsInterface& network_hints_interface,
    LinkCaller caller) {
  if (rel_attribute.IsDNSPrefetch()) {
    UseCounter::Count(document, UseCounter::kLinkRelDnsPrefetch);
    if (caller == kLinkCalledFromHeader)
      UseCounter::Count(document, UseCounter::kLinkHeaderDnsPrefetch);
    Settings* settings = document.GetSettings();
    // FIXME: The href attribute of the link element can be in "//hostname"
    // form, and we shouldn't attempt to complete that as URL
    // <https://bugs.webkit.org/show_bug.cgi?id=48857>.
    if (settings && settings->GetDNSPrefetchingEnabled() && href.IsValid() &&
        !href.IsEmpty()) {
      if (settings->GetLogDnsPrefetchAndPreconnect()) {
        document.AddConsoleMessage(ConsoleMessage::Create(
            kOtherMessageSource, kVerboseMessageLevel,
            String("DNS prefetch triggered for " + href.Host())));
      }
      network_hints_interface.DnsPrefetchHost(href.Host());
    }
  }
}

static void PreconnectIfNeeded(
    const LinkRelAttribute& rel_attribute,
    const KURL& href,
    Document& document,
    const CrossOriginAttributeValue cross_origin,
    const NetworkHintsInterface& network_hints_interface,
    LinkCaller caller) {
  if (rel_attribute.IsPreconnect() && href.IsValid() &&
      href.ProtocolIsInHTTPFamily()) {
    UseCounter::Count(document, UseCounter::kLinkRelPreconnect);
    if (caller == kLinkCalledFromHeader)
      UseCounter::Count(document, UseCounter::kLinkHeaderPreconnect);
    Settings* settings = document.GetSettings();
    if (settings && settings->GetLogDnsPrefetchAndPreconnect()) {
      document.AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kVerboseMessageLevel,
          String("Preconnect triggered for ") + href.GetString()));
      if (cross_origin != kCrossOriginAttributeNotSet) {
        document.AddConsoleMessage(ConsoleMessage::Create(
            kOtherMessageSource, kVerboseMessageLevel,
            String("Preconnect CORS setting is ") +
                String((cross_origin == kCrossOriginAttributeAnonymous)
                           ? "anonymous"
                           : "use-credentials")));
      }
    }
    network_hints_interface.PreconnectHost(href, cross_origin);
  }
}

WTF::Optional<Resource::Type> LinkLoader::GetResourceTypeFromAsAttribute(
    const String& as) {
  DCHECK_EQ(as.DeprecatedLower(), as);
  if (as == "image") {
    return Resource::kImage;
  } else if (as == "script") {
    return Resource::kScript;
  } else if (as == "style") {
    return Resource::kCSSStyleSheet;
  } else if (as == "video") {
    return Resource::kMedia;
  } else if (as == "audio") {
    return Resource::kMedia;
  } else if (as == "track") {
    return Resource::kTextTrack;
  } else if (as == "font") {
    return Resource::kFont;
  } else if (as.IsEmpty()) {
    return Resource::kRaw;
  }
  return WTF::kNullopt;
}

Resource* LinkLoader::LinkPreloadedResourceForTesting() {
  return link_preload_resource_client_
             ? link_preload_resource_client_->GetResource()
             : nullptr;
}

void LinkLoader::CreateLinkPreloadResourceClient(Resource* resource) {
  if (!resource)
    return;
  switch (resource->GetType()) {
    case Resource::kImage:
      link_preload_resource_client_ = LinkPreloadImageResourceClient::Create(
          this, ToImageResource(resource));
      break;
    case Resource::kScript:
      link_preload_resource_client_ = LinkPreloadScriptResourceClient::Create(
          this, ToScriptResource(resource));
      break;
    case Resource::kCSSStyleSheet:
      link_preload_resource_client_ = LinkPreloadStyleResourceClient::Create(
          this, ToCSSStyleSheetResource(resource));
      break;
    case Resource::kFont:
      link_preload_resource_client_ =
          LinkPreloadFontResourceClient::Create(this, ToFontResource(resource));
      break;
    case Resource::kMedia:
    case Resource::kTextTrack:
    case Resource::kRaw:
      link_preload_resource_client_ =
          LinkPreloadRawResourceClient::Create(this, ToRawResource(resource));
      break;
    default:
      NOTREACHED();
  }
}

static bool IsSupportedType(Resource::Type resource_type,
                            const String& mime_type) {
  if (mime_type.IsEmpty())
    return true;
  switch (resource_type) {
    case Resource::kImage:
      return MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(mime_type);
    case Resource::kScript:
      return MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type);
    case Resource::kCSSStyleSheet:
      return MIMETypeRegistry::IsSupportedStyleSheetMIMEType(mime_type);
    case Resource::kFont:
      return MIMETypeRegistry::IsSupportedFontMIMEType(mime_type);
    case Resource::kMedia:
      return MIMETypeRegistry::IsSupportedMediaMIMEType(mime_type, String());
    case Resource::kTextTrack:
      return MIMETypeRegistry::IsSupportedTextTrackMIMEType(mime_type);
    case Resource::kRaw:
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

static Resource* PreloadIfNeeded(const LinkRelAttribute& rel_attribute,
                                 const KURL& href,
                                 Document& document,
                                 const String& as,
                                 const String& mime_type,
                                 const String& media,
                                 CrossOriginAttributeValue cross_origin,
                                 LinkCaller caller,
                                 bool& error_occurred,
                                 ViewportDescription* viewport_description,
                                 ReferrerPolicy referrer_policy) {
  if (!document.Loader() || !rel_attribute.IsLinkPreload())
    return nullptr;

  UseCounter::Count(document, UseCounter::kLinkRelPreload);
  if (!href.IsValid() || href.IsEmpty()) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=preload> has an invalid `href` value")));
    return nullptr;
  }

  if (!media.IsEmpty()) {
    MediaValues* media_values =
        MediaValues::CreateDynamicIfFrameExists(document.GetFrame());
    if (viewport_description) {
      media_values->OverrideViewportDimensions(
          viewport_description->max_width.GetFloatValue(),
          viewport_description->max_height.GetFloatValue());
    }

    // Preload only if media matches
    MediaQuerySet* media_queries = MediaQuerySet::Create(media);
    MediaQueryEvaluator evaluator(*media_values);
    if (!evaluator.Eval(media_queries))
      return nullptr;
  }
  if (caller == kLinkCalledFromHeader)
    UseCounter::Count(document, UseCounter::kLinkHeaderPreload);
  Optional<Resource::Type> resource_type =
      LinkLoader::GetResourceTypeFromAsAttribute(as);
  if (resource_type == WTF::kNullopt) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=preload> must have a valid `as` value")));
    error_occurred = true;
    return nullptr;
  }

  if (!IsSupportedType(resource_type.value(), mime_type)) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        String("<link rel=preload> has an unsupported `type` value")));
    return nullptr;
  }
  ResourceRequest resource_request(document.CompleteURL(href));
  resource_request.SetRequestContext(
      ResourceFetcher::DetermineRequestContext(resource_type.value(), false));

  if (referrer_policy != kReferrerPolicyDefault) {
    resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        referrer_policy, href, document.OutgoingReferrer()));
  }

  FetchParameters link_fetch_params(
      resource_request, FetchInitiatorTypeNames::link, document.EncodingName());

  if (cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                                  cross_origin);
  }
  Settings* settings = document.GetSettings();
  if (settings && settings->GetLogPreload()) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kVerboseMessageLevel,
        String("Preload triggered for " + href.Host() + href.GetPath())));
  }
  link_fetch_params.SetLinkPreload(true);
  return document.Loader()->StartPreload(resource_type.value(),
                                         link_fetch_params);
}

static Resource* PrefetchIfNeeded(Document& document,
                                  const KURL& href,
                                  const LinkRelAttribute& rel_attribute,
                                  CrossOriginAttributeValue cross_origin,
                                  ReferrerPolicy referrer_policy) {
  if (rel_attribute.IsLinkPrefetch() && href.IsValid() && document.GetFrame()) {
    UseCounter::Count(document, UseCounter::kLinkRelPrefetch);

    ResourceRequest resource_request(document.CompleteURL(href));
    if (referrer_policy != kReferrerPolicyDefault) {
      resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
          referrer_policy, href, document.OutgoingReferrer()));
    }

    FetchParameters link_fetch_params(resource_request,
                                      FetchInitiatorTypeNames::link);
    if (cross_origin != kCrossOriginAttributeNotSet) {
      link_fetch_params.SetCrossOriginAccessControl(
          document.GetSecurityOrigin(), cross_origin);
    }
    return LinkFetchResource::Fetch(Resource::kLinkPrefetch, link_fetch_params,
                                    document.Fetcher());
  }
  return nullptr;
}

void LinkLoader::LoadLinksFromHeader(
    const String& header_value,
    const KURL& base_url,
    Document* document,
    const NetworkHintsInterface& network_hints_interface,
    CanLoadResources can_load_resources,
    MediaPreloadPolicy media_policy,
    ViewportDescriptionWrapper* viewport_description_wrapper) {
  if (!document || header_value.IsEmpty())
    return;
  LinkHeaderSet header_set(header_value);
  for (auto& header : header_set) {
    if (!header.Valid() || header.Url().IsEmpty() || header.Rel().IsEmpty())
      continue;

    if (media_policy == kOnlyLoadMedia && header.Media().IsEmpty())
      continue;
    if (media_policy == kOnlyLoadNonMedia && !header.Media().IsEmpty())
      continue;

    LinkRelAttribute rel_attribute(header.Rel());
    KURL url(base_url, header.Url());
    // Sanity check to avoid re-entrancy here.
    if (url == base_url)
      continue;
    if (can_load_resources != kOnlyLoadResources) {
      DnsPrefetchIfNeeded(rel_attribute, url, *document,
                          network_hints_interface, kLinkCalledFromHeader);

      PreconnectIfNeeded(rel_attribute, url, *document,
                         GetCrossOriginAttributeValue(header.CrossOrigin()),
                         network_hints_interface, kLinkCalledFromHeader);
    }
    if (can_load_resources != kDoNotLoadResources) {
      bool error_occurred = false;
      ViewportDescription* viewport_description =
          (viewport_description_wrapper && viewport_description_wrapper->set)
              ? &(viewport_description_wrapper->description)
              : nullptr;

      CrossOriginAttributeValue cross_origin =
          GetCrossOriginAttributeValue(header.CrossOrigin());
      PreloadIfNeeded(rel_attribute, url, *document, header.As(),
                      header.MimeType(), header.Media(), cross_origin,
                      kLinkCalledFromHeader, error_occurred,
                      viewport_description, kReferrerPolicyDefault);
      PrefetchIfNeeded(*document, url, rel_attribute, cross_origin,
                       kReferrerPolicyDefault);
    }
    if (rel_attribute.IsServiceWorker()) {
      UseCounter::Count(*document, UseCounter::kLinkHeaderServiceWorker);
    }
    // TODO(yoav): Add more supported headers as needed.
  }
}

bool LinkLoader::LoadLink(
    const LinkRelAttribute& rel_attribute,
    CrossOriginAttributeValue cross_origin,
    const String& type,
    const String& as,
    const String& media,
    ReferrerPolicy referrer_policy,
    const KURL& href,
    Document& document,
    const NetworkHintsInterface& network_hints_interface) {
  if (!client_->ShouldLoadLink())
    return false;

  DnsPrefetchIfNeeded(rel_attribute, href, document, network_hints_interface,
                      kLinkCalledFromMarkup);

  PreconnectIfNeeded(rel_attribute, href, document, cross_origin,
                     network_hints_interface, kLinkCalledFromMarkup);

  bool error_occurred = false;
  CreateLinkPreloadResourceClient(PreloadIfNeeded(
      rel_attribute, href, document, as, type, media, cross_origin,
      kLinkCalledFromMarkup, error_occurred, nullptr, referrer_policy));
  if (error_occurred)
    link_loading_error_timer_.StartOneShot(0, BLINK_FROM_HERE);

  if (href.IsEmpty() || !href.IsValid())
    Released();

  Resource* resource = PrefetchIfNeeded(document, href, rel_attribute,
                                        cross_origin, referrer_policy);
  if (resource)
    SetResource(resource);

  if (const unsigned prerender_rel_types =
          PrerenderRelTypesFromRelAttribute(rel_attribute, document)) {
    if (!prerender_) {
      prerender_ =
          PrerenderHandle::Create(document, this, href, prerender_rel_types);
    } else if (prerender_->Url() != href) {
      prerender_->Cancel();
      prerender_ =
          PrerenderHandle::Create(document, this, href, prerender_rel_types);
    }
    // TODO(gavinp): Handle changes to rel types of existing prerenders.
  } else if (prerender_) {
    prerender_->Cancel();
    prerender_.Clear();
  }
  return true;
}

void LinkLoader::Released() {
  // Only prerenders need treatment here; other links either use the Resource
  // interface, or are notionally atomic (dns prefetch).
  if (prerender_) {
    prerender_->Cancel();
    prerender_.Clear();
  }
  if (link_preload_resource_client_)
    link_preload_resource_client_->Clear();
}

DEFINE_TRACE(LinkLoader) {
  visitor->Trace(client_);
  visitor->Trace(prerender_);
  visitor->Trace(link_preload_resource_client_);
  ResourceOwner<Resource, ResourceClient>::Trace(visitor);
  PrerenderClient::Trace(visitor);
}

}  // namespace blink
