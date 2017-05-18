/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#ifndef FetchContext_h
#define FetchContext_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebURLLoader.h"

namespace blink {

class ClientHintsPreferences;
class KURL;
class MHTMLArchive;
class PlatformProbeSink;
class ResourceError;
class ResourceResponse;
class ResourceTimingInfo;
class WebTaskRunner;
enum class WebCachePolicy;

enum FetchResourceType { kFetchMainResource, kFetchSubresource };

// The FetchContext is an interface for performing context specific processing
// in response to events in the ResourceFetcher. The ResourceFetcher or its job
// class, ResourceLoader, may call the methods on a FetchContext.
//
// Any processing that depends on components outside platform/loader/fetch/
// should be implemented on a subclass of this interface, and then exposed to
// the ResourceFetcher via this interface.
class PLATFORM_EXPORT FetchContext
    : public GarbageCollectedFinalized<FetchContext> {
  WTF_MAKE_NONCOPYABLE(FetchContext);

 public:
  enum LogMessageType { kLogErrorMessage, kLogWarningMessage };

  static FetchContext& NullInstance();

  virtual ~FetchContext() {}

  DECLARE_VIRTUAL_TRACE();

  virtual bool IsFrameFetchContext() { return false; }

  virtual void AddAdditionalRequestHeaders(ResourceRequest&, FetchResourceType);

  // Returns the cache policy for the resource. ResourceRequest is not passed as
  // a const reference as a header needs to be added for doc.write blocking
  // intervention.
  virtual WebCachePolicy ResourceRequestCachePolicy(
      const ResourceRequest&,
      Resource::Type,
      FetchParameters::DeferOption) const;

  virtual void DispatchDidChangeResourcePriority(unsigned long identifier,
                                                 ResourceLoadPriority,
                                                 int intra_priority_value);

  // This internally dispatches WebFrameClient::willSendRequest and hooks
  // request interceptors like ServiceWorker and ApplicationCache.
  // This may modify the request.
  enum class RedirectType { kForRedirect, kNotForRedirect };
  virtual void PrepareRequest(ResourceRequest&, RedirectType);

  // The last callback before a request is actually sent to the browser process.
  // TODO(https://crbug.com/632580): make this take const ResourceRequest&.
  virtual void DispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirect_response,
      const FetchInitiatorInfo& = FetchInitiatorInfo());
  virtual void DispatchDidLoadResourceFromMemoryCache(unsigned long identifier,
                                                      const ResourceRequest&,
                                                      const ResourceResponse&);
  enum class ResourceResponseType { kNotFromMemoryCache, kFromMemoryCache };
  virtual void DispatchDidReceiveResponse(unsigned long identifier,
                                          const ResourceResponse&,
                                          WebURLRequest::FrameType,
                                          WebURLRequest::RequestContext,
                                          Resource*,
                                          ResourceResponseType);
  virtual void DispatchDidReceiveData(unsigned long identifier,
                                      const char* data,
                                      int data_length);
  virtual void DispatchDidReceiveEncodedData(unsigned long identifier,
                                             int encoded_data_length);
  virtual void DispatchDidDownloadData(unsigned long identifier,
                                       int data_length,
                                       int encoded_data_length);
  virtual void DispatchDidFinishLoading(unsigned long identifier,
                                        double finish_time,
                                        int64_t encoded_data_length,
                                        int64_t decoded_body_length);
  virtual void DispatchDidFail(unsigned long identifier,
                               const ResourceError&,
                               int64_t encoded_data_length,
                               bool is_internal_request);

  virtual bool ShouldLoadNewResource(Resource::Type) const { return false; }

  // Called when a resource load is first requested, which may not be when the
  // load actually begins.
  virtual void RecordLoadingActivity(unsigned long identifier,
                                     const ResourceRequest&,
                                     Resource::Type,
                                     const AtomicString& fetch_initiator_name);

  virtual void DidLoadResource(Resource*);

  virtual void AddResourceTiming(const ResourceTimingInfo&);
  virtual bool AllowImage(bool, const KURL&) const { return false; }
  virtual ResourceRequestBlockedReason CanRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchParameters::OriginRestriction) const {
    return ResourceRequestBlockedReason::kOther;
  }
  virtual ResourceRequestBlockedReason CanFollowRedirect(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchParameters::OriginRestriction) const {
    return ResourceRequestBlockedReason::kOther;
  }
  virtual ResourceRequestBlockedReason AllowResponse(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&) const {
    return ResourceRequestBlockedReason::kOther;
  }

  virtual bool IsControlledByServiceWorker() const { return false; }
  virtual int64_t ServiceWorkerID() const { return -1; }

  virtual bool IsMainFrame() const { return true; }
  virtual bool DefersLoading() const { return false; }
  virtual bool IsLoadComplete() const { return false; }
  virtual bool PageDismissalEventBeingDispatched() const { return false; }
  virtual bool UpdateTimingInfoForIFrameNavigation(ResourceTimingInfo*) {
    return false;
  }
  virtual void SendImagePing(const KURL&);
  virtual void AddConsoleMessage(const String&,
                                 LogMessageType = kLogErrorMessage) const;
  virtual SecurityOrigin* GetSecurityOrigin() const { return nullptr; }

  // Populates the ResourceRequest using the given values and information
  // stored in the FetchContext implementation. Used by ResourceFetcher to
  // prepare a ResourceRequest instance at the start of resource loading.
  virtual void PopulateResourceRequest(const KURL&,
                                       Resource::Type,
                                       const ClientHintsPreferences&,
                                       const FetchParameters::ResourceWidth&,
                                       const ResourceLoaderOptions&,
                                       SecurityViolationReportingPolicy,
                                       ResourceRequest&);
  // Sets the first party for cookies and requestor origin using information
  // stored in the FetchContext implementation.
  virtual void SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&);

  virtual MHTMLArchive* Archive() const { return nullptr; }

  virtual RefPtr<WebTaskRunner> LoadingTaskRunner() const { return nullptr; }

  PlatformProbeSink* GetPlatformProbeSink() const {
    return platform_probe_sink_;
  }

  virtual std::unique_ptr<WebURLLoader> CreateURLLoader() {
    NOTREACHED();
    return nullptr;
  }

 protected:
  FetchContext();

 private:
  Member<PlatformProbeSink> platform_probe_sink_;
};

}  // namespace blink

#endif
