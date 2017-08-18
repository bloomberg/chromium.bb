/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
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

#ifndef DocumentThreadableLoader_h
#define DocumentThreadableLoader_h

#include <memory>
#include "core/CoreExport.h"
#include "core/loader/ThreadableLoader.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class KURL;
class ResourceRequest;
class SecurityOrigin;
class ThreadableLoaderClient;
class ThreadableLoadingContext;

// TODO(horo): We are using this class not only in documents, but also in
// workers. We should change the name to ThreadableLoaderImpl.
class CORE_EXPORT DocumentThreadableLoader final : public ThreadableLoader,
                                                   private RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentThreadableLoader);

 public:
  static void LoadResourceSynchronously(Document&,
                                        const ResourceRequest&,
                                        ThreadableLoaderClient&,
                                        const ThreadableLoaderOptions&,
                                        const ResourceLoaderOptions&);
  static DocumentThreadableLoader* Create(ThreadableLoadingContext&,
                                          ThreadableLoaderClient*,
                                          const ThreadableLoaderOptions&,
                                          const ResourceLoaderOptions&);
  ~DocumentThreadableLoader() override;

  void Start(const ResourceRequest&) override;

  void OverrideTimeout(unsigned long timeout) override;

  void Cancel() override;
  void SetDefersLoading(bool);

  DECLARE_TRACE();

 private:
  enum BlockingBehavior { kLoadSynchronously, kLoadAsynchronously };

  DocumentThreadableLoader(ThreadableLoadingContext&,
                           ThreadableLoaderClient*,
                           BlockingBehavior,
                           const ThreadableLoaderOptions&,
                           const ResourceLoaderOptions&);

  void Clear();

  // ResourceClient
  void NotifyFinished(Resource*) override;

  String DebugName() const override { return "DocumentThreadableLoader"; }

  // RawResourceClient
  void DataSent(Resource*,
                unsigned long long bytes_sent,
                unsigned long long total_bytes_to_be_sent) override;
  void ResponseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override;
  void SetSerializedCachedMetadata(Resource*, const char*, size_t) override;
  void DataReceived(Resource*, const char* data, size_t data_length) override;
  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override;
  void RedirectBlocked() override;
  void DataDownloaded(Resource*, int) override;
  void DidReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override;

  // Notify Inspector and log to console about resource response. Use this
  // method if response is not going to be finished normally.
  void ReportResponseReceived(unsigned long identifier,
                              const ResourceResponse&);

  // Methods containing code to handle resource fetch results which are common
  // to both sync and async mode.
  //
  // The FetchCredentialsMode argument must be the request's credentials mode.
  // It's used for CORS check.
  void HandleResponse(unsigned long identifier,
                      WebURLRequest::FetchRequestMode,
                      WebURLRequest::FetchCredentialsMode,
                      const ResourceResponse&,
                      std::unique_ptr<WebDataConsumerHandle>);
  void HandleReceivedData(const char* data, size_t data_length);
  void HandleSuccessfulFinish(unsigned long identifier, double finish_time);

  void DidTimeout(TimerBase*);
  // Calls the appropriate loading method according to policy and data about
  // origin. Only for handling the initial load (including fallback after
  // consulting ServiceWorker).
  void DispatchInitialRequest(ResourceRequest&);
  void MakeCrossOriginAccessRequest(const ResourceRequest&);

  // TODO(hintzed): CORS handled out of Blink. Code in methods below named
  // *OutOfBlinkCORS is to be moved back into the corresponding methods
  // (e.g. those inherited from RawResourceClient) after
  // https://crbug.com/736308 is fixed (i.e. when CORS is generally handled out
  // of Blink).
  void DispatchInitialRequestOutOfBlinkCORS(ResourceRequest&);
  void MakeCrossOriginAccessRequestOutOfBlinkCORS(const ResourceRequest&);
  void StartOutOfBlinkCORS(const ResourceRequest&);
  bool RedirectReceivedOutOfBlinkCORS(Resource*,
                                      const ResourceRequest&,
                                      const ResourceResponse&);
  void HandleResponseOutOfBlinkCORS(unsigned long identifier,
                                    WebURLRequest::FetchRequestMode,
                                    WebURLRequest::FetchCredentialsMode,
                                    const ResourceResponse&,
                                    std::unique_ptr<WebDataConsumerHandle>);
  // TODO(hintzed): CORS handled in Blink. Methods below named *BlinkCORS are to
  // be removed after https://crbug.com/736308 is fixed (i.e. when CORS is
  // handled out of Blink).
  void DispatchInitialRequestBlinkCORS(ResourceRequest&);
  void MakeCrossOriginAccessRequestBlinkCORS(const ResourceRequest&);
  void StartBlinkCORS(const ResourceRequest&);
  bool RedirectReceivedBlinkCORS(Resource*,
                                 const ResourceRequest&,
                                 const ResourceResponse&);
  void HandleResponseBlinkCORS(unsigned long identifier,
                               WebURLRequest::FetchRequestMode,
                               WebURLRequest::FetchCredentialsMode,
                               const ResourceResponse&,
                               std::unique_ptr<WebDataConsumerHandle>);

  // Loads m_fallbackRequestForServiceWorker.
  void LoadFallbackRequestForServiceWorker();
  // Issues a CORS preflight.
  void LoadPreflightRequest(const ResourceRequest&,
                            const ResourceLoaderOptions&);
  // Loads actual_request_.
  void LoadActualRequest();
  // Clears actual_request_ and reports access control check failure to
  // m_client.
  void HandlePreflightFailure(const KURL&, const String& error_description);
  // Investigates the response for the preflight request. If successful,
  // the actual request will be made later in handleSuccessfulFinish().
  void HandlePreflightResponse(const ResourceResponse&);

  void DispatchDidFailAccessControlCheck(const ResourceError&);
  void DispatchDidFail(const ResourceError&);

  void LoadRequestAsync(const ResourceRequest&, ResourceLoaderOptions);
  void LoadRequestSync(const ResourceRequest&, ResourceLoaderOptions);

  void PrepareCrossOriginRequest(ResourceRequest&) const;

  // This method modifies the ResourceRequest by calling
  // SetAllowStoredCredentials() on it based on same-origin-ness and the
  // credentials mode.
  //
  // This method configures the ResourceLoaderOptions so that the underlying
  // ResourceFetcher doesn't perform some part of the CORS logic since this
  // class performs it by itself.
  void LoadRequest(ResourceRequest&, ResourceLoaderOptions);
  bool IsAllowedRedirect(WebURLRequest::FetchRequestMode, const KURL&) const;

  // TODO(hiroshige): After crbug.com/633696 is fixed,
  // - Remove RawResourceClientStateChecker logic,
  // - Make DocumentThreadableLoader to be a ResourceOwner and remove this
  //   re-implementation of ResourceOwner, and
  // - Consider re-applying RawResourceClientStateChecker in a more
  //   general fashion (crbug.com/640291).
  RawResource* GetResource() const { return resource_.Get(); }
  void ClearResource() { SetResource(nullptr); }
  void SetResource(RawResource* new_resource) {
    if (new_resource == resource_)
      return;

    if (RawResource* old_resource = resource_.Release()) {
      checker_.WillRemoveClient();
      old_resource->RemoveClient(this);
    }

    if (new_resource) {
      resource_ = new_resource;
      checker_.WillAddClient();
      resource_->AddClient(this);
    }
  }
  Member<RawResource> resource_;
  // End of ResourceOwner re-implementation, see above.

  SecurityOrigin* GetSecurityOrigin() const;

  // Returns null if the loader is not associated with Document.
  // TODO(kinuko): Remove dependency to document.
  Document* GetDocument() const;

  ExecutionContext* GetExecutionContext() const;

  ThreadableLoaderClient* client_;
  Member<ThreadableLoadingContext> loading_context_;

  const ThreadableLoaderOptions options_;
  // Some items may be overridden by m_forceDoNotAllowStoredCredentials and
  // m_securityOrigin. In such a case, build a ResourceLoaderOptions with
  // up-to-date values from them and this variable, and use it.
  const ResourceLoaderOptions resource_loader_options_;

  // True when feature OutOfBlinkCORS is enabled (https://crbug.com/736308).
  bool out_of_blink_cors_;

  // Corresponds to the CORS flag in the Fetch spec.
  bool cors_flag_;
  bool suborigin_force_credentials_;
  RefPtr<SecurityOrigin> security_origin_;

  // Set to true when the response data is given to a data consumer handle.
  bool is_using_data_consumer_handle_;

  const bool async_;

  // Holds the original request context (used for sanity checks).
  WebURLRequest::RequestContext request_context_;

  // Saved so that we can use the original value for the modes in
  // ResponseReceived() where |resource| might be a reused one (e.g. preloaded
  // resource) which can have different modes.
  WebURLRequest::FetchRequestMode fetch_request_mode_;
  WebURLRequest::FetchCredentialsMode fetch_credentials_mode_;

  // Holds the original request for fallback in case the Service Worker
  // does not respond.
  ResourceRequest fallback_request_for_service_worker_;

  // Holds the original request and options for it during preflight request
  // handling phase.
  ResourceRequest actual_request_;
  ResourceLoaderOptions actual_options_;

  // stores request headers in case of a cross-origin redirect.
  HTTPHeaderMap request_headers_;

  TaskRunnerTimer<DocumentThreadableLoader> timeout_timer_;
  double request_started_seconds_;  // Time an asynchronous fetch request is
                                    // started

  // Max number of times that this DocumentThreadableLoader can follow
  // cross-origin redirects. This is used to limit the number of redirects. But
  // this value is not the max number of total redirects allowed, because
  // same-origin redirects are not counted here.
  int cors_redirect_limit_;

  WebURLRequest::FetchRedirectMode redirect_mode_;

  // Holds the referrer after a redirect response was received. This referrer is
  // used to populate the HTTP Referer header when following the redirect.
  bool override_referrer_;
  Referrer referrer_after_redirect_;

  RawResourceClientStateChecker checker_;
};

}  // namespace blink

#endif  // DocumentThreadableLoader_h
