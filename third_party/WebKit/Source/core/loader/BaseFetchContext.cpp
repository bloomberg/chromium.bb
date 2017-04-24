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

BaseFetchContext::BaseFetchContext(ExecutionContext* context)
    : execution_context_(context) {}

void BaseFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                   FetchResourceType type) {
  bool is_main_resource = type == kFetchMainResource;
  if (!is_main_resource) {
    if (!request.DidSetHTTPReferrer()) {
      DCHECK(execution_context_);
      request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
          execution_context_->GetReferrerPolicy(), request.Url(),
          execution_context_->OutgoingReferrer()));
      request.AddHTTPOriginIfNeeded(execution_context_->GetSecurityOrigin());
    } else {
      DCHECK_EQ(SecurityPolicy::GenerateReferrer(request.GetReferrerPolicy(),
                                                 request.Url(),
                                                 request.HttpReferrer())
                    .referrer,
                request.HttpReferrer());
      request.AddHTTPOriginIfNeeded(request.HttpReferrer());
    }
  }

  if (execution_context_) {
    request.SetExternalRequestStateFromRequestorAddressSpace(
        execution_context_->GetSecurityContext().AddressSpace());
  }
}

SecurityOrigin* BaseFetchContext::GetSecurityOrigin() const {
  return execution_context_ ? execution_context_->GetSecurityOrigin() : nullptr;
}

ResourceRequestBlockedReason BaseFetchContext::CanRequest(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    FetchParameters::OriginRestriction origin_restriction) const {
  ResourceRequestBlockedReason blocked_reason = CanRequestInternal(
      type, resource_request, url, options, reporting_policy,
      origin_restriction, resource_request.GetRedirectStatus());
  if (blocked_reason != ResourceRequestBlockedReason::kNone &&
      reporting_policy == SecurityViolationReportingPolicy::kReport) {
    DispatchDidBlockRequest(resource_request, options.initiator_info,
                            blocked_reason);
  }
  return blocked_reason;
}

ResourceRequestBlockedReason BaseFetchContext::CanFollowRedirect(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    FetchParameters::OriginRestriction origin_restriction) const {
  // CanRequestInternal checks enforced CSP, so check report-only here to ensure
  // that violations are sent.
  CheckCSPForRequest(resource_request, url, options, reporting_policy,
                     RedirectStatus::kFollowedRedirect,
                     ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);
  return CanRequest(type, resource_request, url, options, reporting_policy,
                    origin_restriction);
}

ResourceRequestBlockedReason BaseFetchContext::AllowResponse(
    Resource::Type type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options) const {
  // CanRequestInternal only checks enforced policies: check report-only here
  // to ensure violations are sent.
  CheckCSPForRequest(resource_request, url, options,
                     SecurityViolationReportingPolicy::kReport,
                     RedirectStatus::kFollowedRedirect,
                     ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);
  ResourceRequestBlockedReason blocked_reason =
      CanRequestInternal(type, resource_request, url, options,
                         SecurityViolationReportingPolicy::kReport,
                         FetchParameters::kUseDefaultOriginRestrictionForType,
                         RedirectStatus::kFollowedRedirect);
  if (blocked_reason != ResourceRequestBlockedReason::kNone) {
    DispatchDidBlockRequest(resource_request, options.initiator_info,
                            blocked_reason);
  }
  return blocked_reason;
}

void BaseFetchContext::PrintAccessDeniedMessage(const KURL& url) const {
  if (url.IsNull())
    return;

  DCHECK(execution_context_);
  String message;
  if (execution_context_->Url().IsNull()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() + '.';
  } else if (url.IsLocalFile() || execution_context_->Url().IsLocalFile()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " +
              execution_context_->Url().ElidedString() +
              ". 'file:' URLs are treated as unique security origins.\n";
  } else {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " +
              execution_context_->Url().ElidedString() +
              ". Domains, protocols and ports must match.\n";
  }

  execution_context_->AddConsoleMessage(ConsoleMessage::Create(
      kSecurityMessageSource, kErrorMessageLevel, message));
}

void BaseFetchContext::AddCSPHeaderIfNecessary(Resource::Type type,
                                               ResourceRequest& request) {
  if (!execution_context_)
    return;

  const ContentSecurityPolicy* csp =
      execution_context_->GetContentSecurityPolicy();
  if (csp->ShouldSendCSPHeader(type))
    request.AddHTTPHeaderField("CSP", "active");
}

ResourceRequestBlockedReason BaseFetchContext::CheckCSPForRequest(
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status,
    ContentSecurityPolicy::CheckHeaderType check_header_type) const {
  if (ShouldBypassMainWorldCSP() || options.content_security_policy_option ==
                                        kDoNotCheckContentSecurityPolicy) {
    return ResourceRequestBlockedReason::kNone;
  }

  if (execution_context_) {
    DCHECK(execution_context_->GetContentSecurityPolicy());
    if (!execution_context_->GetContentSecurityPolicy()->AllowRequest(
            resource_request.GetRequestContext(), url,
            options.content_security_policy_nonce, options.integrity_metadata,
            options.parser_disposition, redirect_status, reporting_policy,
            check_header_type))
      return ResourceRequestBlockedReason::CSP;
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
  if (ShouldBlockRequestByInspector(resource_request))
    return ResourceRequestBlockedReason::kInspector;

  SecurityOrigin* security_origin = options.security_origin.Get();
  if (!security_origin && execution_context_)
    security_origin = execution_context_->GetSecurityOrigin();

  if (origin_restriction != FetchParameters::kNoOriginRestriction &&
      security_origin && !security_origin->CanDisplay(url)) {
    if (reporting_policy == SecurityViolationReportingPolicy::kReport)
      ReportLocalLoadFailed(url);
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
      DCHECK(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::kSVGDocument:
      if (!security_origin->CanRequest(url)) {
        PrintAccessDeniedMessage(url);
        return ResourceRequestBlockedReason::kOrigin;
      }
      break;
  }

  // We check the 'report-only' headers before upgrading the request (in
  // populateResourceRequest). We check the enforced headers here to ensure we
  // block things we ought to block.
  if (CheckCSPForRequest(
          resource_request, url, options, reporting_policy, redirect_status,
          ContentSecurityPolicy::CheckHeaderType::kCheckEnforce) ==
      ResourceRequestBlockedReason::CSP) {
    return ResourceRequestBlockedReason::CSP;
  }

  if (type == Resource::kScript || type == Resource::kImportResource) {
    if (GetContentSettingsClient() &&
        !GetContentSettingsClient()->AllowScriptFromSource(
            !GetSettings() || GetSettings()->GetScriptEnabled(), url)) {
      GetContentSettingsClient()->DidNotAllowScript();
      // TODO(estark): Use a different ResourceRequestBlockedReason here, since
      // this check has nothing to do with CSP. https://crbug.com/600795
      return ResourceRequestBlockedReason::CSP;
    }
  }

  // SVG Images have unique security rules that prevent all subresource requests
  // except for data urls.
  if (type != Resource::kMainResource && IsSVGImageChromeClient() &&
      !url.ProtocolIsData())
    return ResourceRequestBlockedReason::kOrigin;

  // Measure the number of legacy URL schemes ('ftp://') and the number of
  // embedded-credential ('http://user:password@...') resources embedded as
  // subresources.
  if (resource_request.GetFrameType() != WebURLRequest::kFrameTypeTopLevel) {
    if (GetMainResourceSecurityContext() &&
        SchemeRegistry::ShouldTreatURLSchemeAsLegacy(url.Protocol()) &&
        !SchemeRegistry::ShouldTreatURLSchemeAsLegacy(
            GetMainResourceSecurityContext()
                ->GetSecurityOrigin()
                ->Protocol())) {
      CountDeprecation(UseCounter::kLegacyProtocolEmbeddedAsSubresource);

      // TODO(mkwst): Enabled by default in M59. Drop the runtime-enabled check
      // in M60: https://www.chromestatus.com/feature/5709390967472128
      if (RuntimeEnabledFeatures::blockLegacySubresourcesEnabled())
        return ResourceRequestBlockedReason::kOrigin;
    }

    if ((!url.User().IsEmpty() || !url.Pass().IsEmpty()) &&
        resource_request.GetRequestContext() !=
            WebURLRequest::kRequestContextXMLHttpRequest) {
      CountDeprecation(
          UseCounter::kRequestedSubresourceWithEmbeddedCredentials);
      // TODO(mkwst): Remove the runtime-enabled check in M59:
      // https://www.chromestatus.com/feature/5669008342777856
      if (RuntimeEnabledFeatures::blockCredentialedSubresourcesEnabled())
        return ResourceRequestBlockedReason::kOrigin;
    }
  }

  // Check for mixed content. We do this second-to-last so that when folks block
  // mixed content with a CSP policy, they don't get a warning. They'll still
  // get a warning in the console about CSP blocking the load.
  if (ShouldBlockFetchByMixedContentCheck(resource_request, url,
                                          reporting_policy))
    return ResourceRequestBlockedReason::kMixedContent;

  if (url.WhitespaceRemoved()) {
    CountDeprecation(UseCounter::kCanRequestURLHTTPContainingNewline);
    if (url.ProtocolIsInHTTPFamily()) {
      if (RuntimeEnabledFeatures::restrictCanRequestURLCharacterSetEnabled())
        return ResourceRequestBlockedReason::kOther;
    } else {
      CountUsage(UseCounter::kCanRequestURLNonHTTPContainingNewline);
    }
  }

  // Let the client have the final say into whether or not the load should
  // proceed.
  if (GetSubresourceFilter() && type != Resource::kMainResource &&
      type != Resource::kImportResource) {
    if (!GetSubresourceFilter()->AllowLoad(
            url, resource_request.GetRequestContext(), reporting_policy)) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }
  }

  return ResourceRequestBlockedReason::kNone;
}

DEFINE_TRACE(BaseFetchContext) {
  visitor->Trace(execution_context_);
  FetchContext::Trace(visitor);
}

}  // namespace blink
