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
#include "platform/loader/fetch/CachePolicy.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ClientHintsPreferences;
class KURL;
class MHTMLArchive;
class ResourceError;
class ResourceResponse;
class ResourceTimingInfo;
class WebTaskRunner;
enum class WebCachePolicy;

enum FetchResourceType { FetchMainResource, FetchSubresource };

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
  enum LogMessageType { LogErrorMessage, LogWarningMessage };

  static FetchContext& nullInstance();

  virtual ~FetchContext() {}
  DEFINE_INLINE_VIRTUAL_TRACE() {}

  virtual bool isLiveContext() { return false; }

  virtual void addAdditionalRequestHeaders(ResourceRequest&, FetchResourceType);
  virtual CachePolicy getCachePolicy() const;
  // Returns the cache policy for the resource. ResourceRequest is not passed as
  // a const reference as a header needs to be added for doc.write blocking
  // intervention.
  virtual WebCachePolicy resourceRequestCachePolicy(
      ResourceRequest&,
      Resource::Type,
      FetchRequest::DeferOption) const;

  virtual void dispatchDidChangeResourcePriority(unsigned long identifier,
                                                 ResourceLoadPriority,
                                                 int intraPriorityValue);
  // The last callback before a request is actually sent to the browser process.
  virtual void dispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirectResponse,
      const FetchInitiatorInfo& = FetchInitiatorInfo());
  virtual void dispatchDidLoadResourceFromMemoryCache(
      unsigned long identifier,
      Resource*,
      WebURLRequest::FrameType,
      WebURLRequest::RequestContext);
  virtual void dispatchDidReceiveResponse(unsigned long identifier,
                                          const ResourceResponse&,
                                          WebURLRequest::FrameType,
                                          WebURLRequest::RequestContext,
                                          Resource*);
  virtual void dispatchDidReceiveData(unsigned long identifier,
                                      const char* data,
                                      int dataLength);
  virtual void dispatchDidReceiveEncodedData(unsigned long identifier,
                                             int encodedDataLength);
  virtual void dispatchDidDownloadData(unsigned long identifier,
                                       int dataLength,
                                       int encodedDataLength);
  virtual void dispatchDidFinishLoading(unsigned long identifier,
                                        double finishTime,
                                        int64_t encodedDataLength,
                                        int64_t decodedBodyLength);
  virtual void dispatchDidFail(unsigned long identifier,
                               const ResourceError&,
                               int64_t encodedDataLength,
                               bool isInternalRequest);

  virtual bool shouldLoadNewResource(Resource::Type) const { return false; }
  // Called when a resource load is first requested, which may not be when the
  // load actually begins.
  enum class V8ActivityLoggingPolicy { SuppressLogging, Log };
  virtual void willStartLoadingResource(unsigned long identifier,
                                        ResourceRequest&,
                                        Resource::Type,
                                        const AtomicString& fetchInitiatorName,
                                        V8ActivityLoggingPolicy);
  virtual void didLoadResource(Resource*);

  virtual void addResourceTiming(const ResourceTimingInfo&);
  virtual bool allowImage(bool, const KURL&) const { return false; }
  virtual ResourceRequestBlockedReason canRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchRequest::OriginRestriction) const {
    return ResourceRequestBlockedReason::Other;
  }
  virtual ResourceRequestBlockedReason allowResponse(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&) const {
    return ResourceRequestBlockedReason::Other;
  }

  virtual bool isControlledByServiceWorker() const { return false; }
  virtual int64_t serviceWorkerID() const { return -1; }

  virtual bool isMainFrame() const { return true; }
  virtual bool defersLoading() const { return false; }
  virtual bool isLoadComplete() const { return false; }
  virtual bool pageDismissalEventBeingDispatched() const { return false; }
  virtual bool updateTimingInfoForIFrameNavigation(ResourceTimingInfo*) {
    return false;
  }
  virtual void sendImagePing(const KURL&);
  virtual void addConsoleMessage(const String&,
                                 LogMessageType = LogErrorMessage) const;
  virtual SecurityOrigin* getSecurityOrigin() const { return nullptr; }

  // Populates the ResourceRequest using the given values and information
  // stored in the FetchContext implementation. Used by ResourceFetcher to
  // prepare a ResourceRequest instance at the start of resource loading.
  virtual void populateResourceRequest(Resource::Type,
                                       const ClientHintsPreferences&,
                                       const FetchRequest::ResourceWidth&,
                                       ResourceRequest&);
  // Sets the first party for cookies and requestor origin using information
  // stored in the FetchContext implementation.
  virtual void setFirstPartyCookieAndRequestorOrigin(ResourceRequest&);

  virtual MHTMLArchive* archive() const { return nullptr; }

  virtual ResourceLoadPriority modifyPriorityForExperiments(
      ResourceLoadPriority priority) {
    return priority;
  }

  virtual RefPtr<WebTaskRunner> loadingTaskRunner() const { return nullptr; }

 protected:
  FetchContext() {}
};

}  // namespace blink

#endif
