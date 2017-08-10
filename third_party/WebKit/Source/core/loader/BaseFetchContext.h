// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BaseFetchContext_h
#define BaseFetchContext_h

#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/Optional.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class ConsoleMessage;
class KURL;
class SecurityOrigin;
class SubresourceFilter;

// A core-level implementaiton of FetchContext that does not depend on
// Frame. This class provides basic default implementation for some methods.
class CORE_EXPORT BaseFetchContext : public FetchContext {
 public:
  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  ResourceRequestBlockedReason CanRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchParameters::OriginRestriction,
      ResourceRequest::RedirectStatus) const override;
  ResourceRequestBlockedReason CheckCSPForRequest(
      WebURLRequest::RequestContext,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus) const override;

  DECLARE_VIRTUAL_TRACE();

  virtual KURL GetSiteForCookies() const = 0;
  virtual void CountUsage(WebFeature) const = 0;
  virtual void CountDeprecation(WebFeature) const = 0;

  void AddWarningConsoleMessage(const String&, LogSource) const override;
  void AddErrorConsoleMessage(const String&, LogSource) const override;

 protected:
  // Used for security checks.
  virtual bool AllowScriptFromSource(const KURL&) const = 0;
  virtual SubresourceFilter* GetSubresourceFilter() const = 0;

  // Note: subclasses are expected to override following methods.
  // Used in the default implementation for CanRequest, CanFollowRedirect
  // and AllowResponse.
  virtual bool ShouldBlockRequestByInspector(const KURL&) const = 0;
  virtual void DispatchDidBlockRequest(const ResourceRequest&,
                                       const FetchInitiatorInfo&,
                                       ResourceRequestBlockedReason) const = 0;
  virtual bool ShouldBypassMainWorldCSP() const = 0;
  virtual bool IsSVGImageChromeClient() const = 0;
  virtual bool ShouldBlockFetchByMixedContentCheck(
      WebURLRequest::RequestContext,
      WebURLRequest::FrameType,
      ResourceRequest::RedirectStatus,
      const KURL&,
      SecurityViolationReportingPolicy) const = 0;
  virtual bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                         const KURL&) const = 0;
  virtual ReferrerPolicy GetReferrerPolicy() const = 0;
  virtual String GetOutgoingReferrer() const = 0;
  virtual const KURL& Url() const = 0;
  virtual const SecurityOrigin* GetParentSecurityOrigin() const = 0;
  virtual Optional<WebAddressSpace> GetAddressSpace() const = 0;
  virtual const ContentSecurityPolicy* GetContentSecurityPolicy() const = 0;

  virtual void AddConsoleMessage(ConsoleMessage*) const = 0;

  // Utility method that can be used to implement other methods.
  void PrintAccessDeniedMessage(const KURL&) const;
  void AddCSPHeaderIfNecessary(Resource::Type, ResourceRequest&);

 private:
  // Utility methods that are used in default implement for CanRequest,
  // CanFollowRedirect and AllowResponse.
  ResourceRequestBlockedReason CanRequestInternal(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchParameters::OriginRestriction,
      ResourceRequest::RedirectStatus) const;

  ResourceRequestBlockedReason CheckCSPForRequestInternal(
      WebURLRequest::RequestContext,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus,
      ContentSecurityPolicy::CheckHeaderType) const;
};

}  // namespace blink

#endif  // BaseFetchContext_h
