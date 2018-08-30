// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/content_settings_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"
#include "third_party/blink/renderer/core/loader/private/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

// This function maps from Blink's internal "request context" concept to Fetch's
// notion of a request's "destination":
// https://fetch.spec.whatwg.org/#concept-request-destination.
const char* GetDestinationFromContext(WebURLRequest::RequestContext context) {
  switch (context) {
    case WebURLRequest::kRequestContextUnspecified:
    case WebURLRequest::kRequestContextBeacon:
    case WebURLRequest::kRequestContextDownload:
    case WebURLRequest::kRequestContextEventSource:
    case WebURLRequest::kRequestContextFetch:
    case WebURLRequest::kRequestContextPing:
    case WebURLRequest::kRequestContextXMLHttpRequest:
    case WebURLRequest::kRequestContextSubresource:
    case WebURLRequest::kRequestContextPrefetch:
      return "\"\"";
    case WebURLRequest::kRequestContextCSPReport:
      return "report";
    case WebURLRequest::kRequestContextAudio:
      return "audio";
    case WebURLRequest::kRequestContextEmbed:
      return "embed";
    case WebURLRequest::kRequestContextFont:
      return "font";
    case WebURLRequest::kRequestContextFrame:
    case WebURLRequest::kRequestContextHyperlink:
    case WebURLRequest::kRequestContextIframe:
    case WebURLRequest::kRequestContextLocation:
    case WebURLRequest::kRequestContextForm:
      return "document";
    case WebURLRequest::kRequestContextImage:
    case WebURLRequest::kRequestContextFavicon:
    case WebURLRequest::kRequestContextImageSet:
      return "image";
    case WebURLRequest::kRequestContextManifest:
      return "manifest";
    case WebURLRequest::kRequestContextObject:
      return "object";
    case WebURLRequest::kRequestContextScript:
      return "script";
    case WebURLRequest::kRequestContextServiceWorker:
      return "serviceworker";
    case WebURLRequest::kRequestContextSharedWorker:
      return "sharedworker";
    case WebURLRequest::kRequestContextStyle:
      return "style";
    case WebURLRequest::kRequestContextTrack:
      return "track";
    case WebURLRequest::kRequestContextVideo:
      return "video";
    case WebURLRequest::kRequestContextWorker:
      return "worker";
    case WebURLRequest::kRequestContextXSLT:
      return "xslt";
    case WebURLRequest::kRequestContextImport:
    case WebURLRequest::kRequestContextInternal:
    case WebURLRequest::kRequestContextPlugin:
      return "unknown";
  }
  NOTREACHED();
  return "";
}

MessageSource ConvertLogSourceToMessageSource(FetchContext::LogSource source) {
  // When LogSource is extended, this switch statement should be modified to
  // convert LogSource to blink::MessageSource.
  switch (source) {
    case FetchContext::kJSSource:
      return kJSMessageSource;
    case FetchContext::kSecuritySource:
      return kSecurityMessageSource;
    case FetchContext::kOtherSource:
      return kOtherMessageSource;
  }
  NOTREACHED();
  return kOtherMessageSource;
}

}  // namespace

void BaseFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                   FetchResourceType type) {
  bool is_main_resource = type == kFetchMainResource;
  if (!is_main_resource) {
    // TODO(domfarolino): we can probably *just set* the HTTP `Referer` here
    // no matter what now.
    if (!request.DidSetHTTPReferrer()) {
      String referrer_to_use = request.ReferrerString();
      ReferrerPolicy referrer_policy_to_use = request.GetReferrerPolicy();

      if (referrer_to_use == Referrer::ClientReferrerString())
        referrer_to_use = GetFetchClientSettingsObject()->GetOutgoingReferrer();

      if (referrer_policy_to_use == kReferrerPolicyDefault) {
        referrer_policy_to_use =
            GetFetchClientSettingsObject()->GetReferrerPolicy();
      }

      // TODO(domfarolino): Stop storing ResourceRequest's referrer as a header
      // and store it elsewhere. See https://crbug.com/850813.
      request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
          referrer_policy_to_use, request.Url(), referrer_to_use));
      request.SetHTTPOriginIfNeeded(GetSecurityOrigin());
    } else {
      DCHECK_EQ(SecurityPolicy::GenerateReferrer(request.GetReferrerPolicy(),
                                                 request.Url(),
                                                 request.HttpReferrer())
                    .referrer,
                request.HttpReferrer());
      request.SetHTTPOriginToMatchReferrerIfNeeded();
    }
  }

  auto address_space = GetAddressSpace();
  if (address_space)
    request.SetExternalRequestStateFromRequestorAddressSpace(*address_space);

  if (blink::RuntimeEnabledFeatures::SecMetadataEnabled()) {
    const char* destination_value =
        GetDestinationFromContext(request.GetRequestContext());
    // We'll handle adding the header to navigations outside of Blink.
    if (strncmp(destination_value, "document", 8) != 0 &&
        request.GetRequestContext() != WebURLRequest::kRequestContextInternal) {
      const char* site_value = "cross-site";
      if (SecurityOrigin::Create(request.Url())
              ->IsSameSchemeHostPort(GetSecurityOrigin())) {
        site_value = "same-origin";
      } else {
        OriginAccessEntry access_entry(
            request.Url().Protocol(), request.Url().Host(),
            network::cors::OriginAccessEntry::kAllowRegisterableDomains);
        if (access_entry.MatchesOrigin(*GetSecurityOrigin()) ==
            network::cors::OriginAccessEntry::kMatchesOrigin) {
          site_value = "same-site";
        }
      }

      String value = String::Format("destination=%s, site=%s",
                                    destination_value, site_value);
      request.AddHTTPHeaderField("Sec-Metadata", AtomicString(value));
    }
  }
}

base::Optional<ResourceRequestBlockedReason> BaseFetchContext::CanRequest(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      CanRequestInternal(type, resource_request, url, options, reporting_policy,
                         redirect_status);
  if (blocked_reason &&
      reporting_policy == SecurityViolationReportingPolicy::kReport) {
    DispatchDidBlockRequest(resource_request, options.initiator_info,
                            blocked_reason.value(), type);
  }
  return blocked_reason;
}

void BaseFetchContext::AddInfoConsoleMessage(const String& message,
                                             LogSource source) const {
  AddConsoleMessage(ConsoleMessage::Create(
      ConvertLogSourceToMessageSource(source), kInfoMessageLevel, message));
}

void BaseFetchContext::AddWarningConsoleMessage(const String& message,
                                                LogSource source) const {
  AddConsoleMessage(ConsoleMessage::Create(
      ConvertLogSourceToMessageSource(source), kWarningMessageLevel, message));
}

void BaseFetchContext::AddErrorConsoleMessage(const String& message,
                                              LogSource source) const {
  AddConsoleMessage(ConsoleMessage::Create(
      ConvertLogSourceToMessageSource(source), kErrorMessageLevel, message));
}

bool BaseFetchContext::IsAdResource(
    const KURL& resource_url,
    Resource::Type type,
    WebURLRequest::RequestContext request_context) const {
  SubresourceFilter* filter = GetSubresourceFilter();

  // We do not need main document tagging currently so skipping main resources.
  if (filter && type != Resource::kMainResource) {
    return filter->IsAdResource(resource_url, request_context);
  }
  return false;
}

void BaseFetchContext::PrintAccessDeniedMessage(const KURL& url) const {
  if (url.IsNull())
    return;

  String message;
  if (Url().IsNull()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() + '.';
  } else if (url.IsLocalFile() || Url().IsLocalFile()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " + Url().ElidedString() +
              ". 'file:' URLs are treated as unique security origins.\n";
  } else {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " + Url().ElidedString() +
              ". Domains, protocols and ports must match.\n";
  }

  AddConsoleMessage(ConsoleMessage::Create(kSecurityMessageSource,
                                           kErrorMessageLevel, message));
}

base::Optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckCSPForRequest(
    WebURLRequest::RequestContext request_context,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  return CheckCSPForRequestInternal(
      request_context, url, options, reporting_policy, redirect_status,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);
}

base::Optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckCSPForRequestInternal(
    WebURLRequest::RequestContext request_context,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status,
    ContentSecurityPolicy::CheckHeaderType check_header_type) const {
  if (ShouldBypassMainWorldCSP() || options.content_security_policy_option ==
                                        kDoNotCheckContentSecurityPolicy) {
    return base::nullopt;
  }

  const ContentSecurityPolicy* csp = GetContentSecurityPolicy();
  if (csp && !csp->AllowRequest(
                 request_context, url, options.content_security_policy_nonce,
                 options.integrity_metadata, options.parser_disposition,
                 redirect_status, reporting_policy, check_header_type)) {
    return ResourceRequestBlockedReason::kCSP;
  }
  return base::nullopt;
}

base::Optional<ResourceRequestBlockedReason>
BaseFetchContext::CanRequestInternal(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (IsDetached()) {
    if (!resource_request.GetKeepalive() ||
        redirect_status == ResourceRequest::RedirectStatus::kNoRedirect) {
      return ResourceRequestBlockedReason::kOther;
    }
  }

  if (ShouldBlockRequestByInspector(resource_request.Url()))
    return ResourceRequestBlockedReason::kInspector;

  const SecurityOrigin* security_origin = options.security_origin.get();
  if (!security_origin)
    security_origin = GetSecurityOrigin();

  if (security_origin && !security_origin->CanDisplay(url)) {
    if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
      AddErrorConsoleMessage(
          "Not allowed to load local resource: " + url.GetString(), kJSSource);
    }
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::requestResource URL was not "
                                 "allowed by SecurityOrigin::CanDisplay";
    return ResourceRequestBlockedReason::kOther;
  }

  if (resource_request.GetFetchRequestMode() ==
      network::mojom::FetchRequestMode::kSameOrigin) {
    DCHECK(security_origin);
    if (!security_origin->CanRequest(url)) {
      PrintAccessDeniedMessage(url);
      return ResourceRequestBlockedReason::kOrigin;
    }
  }

  // User Agent CSS stylesheets should only support loading images and should be
  // restricted to data urls.
  if (options.initiator_info.name == FetchInitiatorTypeNames::uacss) {
    if (type == Resource::kImage && url.ProtocolIsData()) {
      return base::nullopt;
    }
    return ResourceRequestBlockedReason::kOther;
  }

  WebURLRequest::RequestContext request_context =
      resource_request.GetRequestContext();

  // We check the 'report-only' headers before upgrading the request (in
  // populateResourceRequest). We check the enforced headers here to ensure we
  // block things we ought to block.
  if (CheckCSPForRequestInternal(
          request_context, url, options, reporting_policy, redirect_status,
          ContentSecurityPolicy::CheckHeaderType::kCheckEnforce) ==
      ResourceRequestBlockedReason::kCSP) {
    return ResourceRequestBlockedReason::kCSP;
  }

  if (type == Resource::kScript || type == Resource::kImportResource) {
    if (!AllowScriptFromSource(url)) {
      // TODO(estark): Use a different ResourceRequestBlockedReason here, since
      // this check has nothing to do with CSP. https://crbug.com/600795
      return ResourceRequestBlockedReason::kCSP;
    }
  }

  // SVG Images have unique security rules that prevent all subresource requests
  // except for data urls.
  if (type != Resource::kMainResource && IsSVGImageChromeClient() &&
      !url.ProtocolIsData())
    return ResourceRequestBlockedReason::kOrigin;

  network::mojom::RequestContextFrameType frame_type =
      resource_request.GetFrameType();

  // Measure the number of legacy URL schemes ('ftp://') and the number of
  // embedded-credential ('http://user:password@...') resources embedded as
  // subresources.
  if (frame_type != network::mojom::RequestContextFrameType::kTopLevel) {
    bool is_subresource =
        frame_type == network::mojom::RequestContextFrameType::kNone;
    const SecurityOrigin* embedding_origin =
        is_subresource ? GetSecurityOrigin() : GetParentSecurityOrigin();
    DCHECK(embedding_origin);
    if (SchemeRegistry::ShouldTreatURLSchemeAsLegacy(url.Protocol()) &&
        !SchemeRegistry::ShouldTreatURLSchemeAsLegacy(
            embedding_origin->Protocol())) {
      CountDeprecation(WebFeature::kLegacyProtocolEmbeddedAsSubresource);

      return ResourceRequestBlockedReason::kOrigin;
    }

    if (ShouldBlockFetchAsCredentialedSubresource(resource_request, url))
      return ResourceRequestBlockedReason::kOrigin;
  }

  // Check for mixed content. We do this second-to-last so that when folks block
  // mixed content via CSP, they don't get a mixed content warning, but a CSP
  // warning instead.
  if (ShouldBlockFetchByMixedContentCheck(request_context, frame_type,
                                          resource_request.GetRedirectStatus(),
                                          url, reporting_policy))
    return ResourceRequestBlockedReason::kMixedContent;

  if (url.PotentiallyDanglingMarkup() && url.ProtocolIsInHTTPFamily()) {
    CountDeprecation(WebFeature::kCanRequestURLHTTPContainingNewline);
    return ResourceRequestBlockedReason::kOther;
  }

  // Loading of a subresource may be blocked by previews resource loading hints.
  if (GetPreviewsResourceLoadingHints() &&
      !GetPreviewsResourceLoadingHints()->AllowLoad(
          url, resource_request.Priority())) {
    // TODO (tbansal): https://crbug.com/864253. Add a specific reason for why
    // the resource fetch was blocked.
    return ResourceRequestBlockedReason::kOther;
  }

  // Let the client have the final say into whether or not the load should
  // proceed.
  if (GetSubresourceFilter() && type != Resource::kMainResource &&
      type != Resource::kImportResource) {
    if (!GetSubresourceFilter()->AllowLoad(url, request_context,
                                           reporting_policy)) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }
  }

  return base::nullopt;
}

void BaseFetchContext::Trace(blink::Visitor* visitor) {
  FetchContext::Trace(visitor);
}

}  // namespace blink
