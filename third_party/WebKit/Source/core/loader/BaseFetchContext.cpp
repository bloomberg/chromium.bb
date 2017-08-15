// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/BaseFetchContext.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/Settings.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/SubresourceFilter.h"
#include "core/loader/private/FrameClientHintsPreferencesContext.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

void BaseFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                   FetchResourceType type) {
  bool is_main_resource = type == kFetchMainResource;
  if (!is_main_resource) {
    if (!request.DidSetHTTPReferrer()) {
      request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
          GetReferrerPolicy(), request.Url(), GetOutgoingReferrer()));
      request.AddHTTPOriginIfNeeded(GetSecurityOrigin());
    } else {
      DCHECK_EQ(SecurityPolicy::GenerateReferrer(request.GetReferrerPolicy(),
                                                 request.Url(),
                                                 request.HttpReferrer())
                    .referrer,
                request.HttpReferrer());
      request.AddHTTPOriginIfNeeded(request.HttpReferrer());
    }
  }

  auto address_space = GetAddressSpace();
  if (address_space)
    request.SetExternalRequestStateFromRequestorAddressSpace(*address_space);
}

ResourceRequestBlockedReason BaseFetchContext::CanRequest(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    FetchParameters::OriginRestriction origin_restriction,
    ResourceRequest::RedirectStatus redirect_status) const {
  ResourceRequestBlockedReason blocked_reason =
      CanRequestInternal(type, resource_request, url, options, reporting_policy,
                         origin_restriction, redirect_status);
  if (blocked_reason != ResourceRequestBlockedReason::kNone &&
      reporting_policy == SecurityViolationReportingPolicy::kReport) {
    DispatchDidBlockRequest(resource_request, options.initiator_info,
                            blocked_reason);
  }
  return blocked_reason;
}

void BaseFetchContext::AddWarningConsoleMessage(const String& message,
                                                LogSource source) const {
  // When LogSource is extended, this DCHECK should be replaced with a logic to
  // convert LogSource to blink::MessageSource.
  DCHECK_EQ(source, kJSSource);
  AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel, message));
}

void BaseFetchContext::AddErrorConsoleMessage(const String& message,
                                              LogSource source) const {
  // When LogSource is extended, this DCHECK should be replaced with a logic to
  // convert LogSource to blink::MessageSource.
  DCHECK_EQ(source, kJSSource);
  AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
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

void BaseFetchContext::AddCSPHeaderIfNecessary(Resource::Type type,
                                               ResourceRequest& request) {
  const ContentSecurityPolicy* csp = GetContentSecurityPolicy();
  if (!csp)
    return;
  if (csp->ShouldSendCSPHeader(type))
    request.AddHTTPHeaderField("CSP", "active");
}

ResourceRequestBlockedReason BaseFetchContext::CheckCSPForRequest(
    WebURLRequest::RequestContext request_context,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  return CheckCSPForRequestInternal(
      request_context, url, options, reporting_policy, redirect_status,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);
}

ResourceRequestBlockedReason BaseFetchContext::CheckCSPForRequestInternal(
    WebURLRequest::RequestContext request_context,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status,
    ContentSecurityPolicy::CheckHeaderType check_header_type) const {
  if (ShouldBypassMainWorldCSP() || options.content_security_policy_option ==
                                        kDoNotCheckContentSecurityPolicy) {
    return ResourceRequestBlockedReason::kNone;
  }

  const ContentSecurityPolicy* csp = GetContentSecurityPolicy();
  if (csp && !csp->AllowRequest(
                 request_context, url, options.content_security_policy_nonce,
                 options.integrity_metadata, options.parser_disposition,
                 redirect_status, reporting_policy, check_header_type)) {
    return ResourceRequestBlockedReason::kCSP;
  }
  return ResourceRequestBlockedReason::kNone;
}

ResourceRequestBlockedReason BaseFetchContext::CanRequestInternal(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    FetchParameters::OriginRestriction origin_restriction,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (IsDetached()) {
    if (!resource_request.GetKeepalive() ||
        redirect_status == ResourceRequest::RedirectStatus::kNoRedirect) {
      return ResourceRequestBlockedReason::kOther;
    }
  }

  if (ShouldBlockRequestByInspector(resource_request.Url()))
    return ResourceRequestBlockedReason::kInspector;

  SecurityOrigin* security_origin = options.security_origin.Get();
  if (!security_origin)
    security_origin = GetSecurityOrigin();

  if (origin_restriction != FetchParameters::kNoOriginRestriction &&
      security_origin && !security_origin->CanDisplay(url)) {
    if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
      AddErrorConsoleMessage(
          "Not allowed to load local resource: " + url.GetString(), kJSSource);
    }
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::requestResource URL was not "
                                 "allowed by SecurityOrigin::CanDisplay";
    return ResourceRequestBlockedReason::kOther;
  }

  // Some types of resources can be loaded only from the same origin. Other
  // types of resources, like Images, Scripts, and CSS, can be loaded from
  // any URL.
  switch (type) {
    case Resource::kMainResource:
    case Resource::kImage:
    case Resource::kCSSStyleSheet:
    case Resource::kScript:
    case Resource::kFont:
    case Resource::kRaw:
    case Resource::kLinkPrefetch:
    case Resource::kTextTrack:
    case Resource::kImportResource:
    case Resource::kMedia:
    case Resource::kManifest:
    case Resource::kMock:
      // By default these types of resources can be loaded from any origin.
      // FIXME: Are we sure about Resource::kFont?
      if (origin_restriction == FetchParameters::kRestrictToSameOrigin &&
          !security_origin->CanRequest(url)) {
        PrintAccessDeniedMessage(url);
        return ResourceRequestBlockedReason::kOrigin;
      }
      break;
    case Resource::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    case Resource::kSVGDocument:
      if (!security_origin->CanRequest(url)) {
        PrintAccessDeniedMessage(url);
        return ResourceRequestBlockedReason::kOrigin;
      }
      break;
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

  WebURLRequest::FrameType frame_type = resource_request.GetFrameType();

  // Measure the number of legacy URL schemes ('ftp://') and the number of
  // embedded-credential ('http://user:password@...') resources embedded as
  // subresources.
  if (frame_type != WebURLRequest::kFrameTypeTopLevel) {
    bool is_subresource = frame_type == WebURLRequest::kFrameTypeNone;
    const SecurityOrigin* embedding_origin =
        is_subresource ? GetSecurityOrigin() : GetParentSecurityOrigin();
    DCHECK(embedding_origin);
    if (SchemeRegistry::ShouldTreatURLSchemeAsLegacy(url.Protocol()) &&
        !SchemeRegistry::ShouldTreatURLSchemeAsLegacy(
            embedding_origin->Protocol())) {
      CountDeprecation(WebFeature::kLegacyProtocolEmbeddedAsSubresource);

      // TODO(mkwst): Enabled by default in M59. Drop the runtime-enabled check
      // in M60: https://www.chromestatus.com/feature/5709390967472128
      if (RuntimeEnabledFeatures::BlockLegacySubresourcesEnabled())
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
    if (RuntimeEnabledFeatures::RestrictCanRequestURLCharacterSetEnabled())
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

  return ResourceRequestBlockedReason::kNone;
}

DEFINE_TRACE(BaseFetchContext) {
  FetchContext::Trace(visitor);
}

}  // namespace blink
