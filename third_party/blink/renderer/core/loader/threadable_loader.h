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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADABLE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADABLE_LOADER_H_

#include <memory>

#include "base/macros.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class ExecutionContext;
class Document;
class KURL;
class ResourceRequest;
class SecurityOrigin;
class ThreadableLoadingContext;
class ThreadableLoaderClient;

// TODO(japhet): This appears unnecessary. Just take a timout_milliseconds
// optional param in constructor?
struct ThreadableLoaderOptions {
  DISALLOW_NEW();
  ThreadableLoaderOptions() : timeout_milliseconds(0) {}
  // When adding members, CrossThreadThreadableLoaderOptionsData should
  // be updated.
  unsigned long timeout_milliseconds;
};

// Useful for doing loader operations from any thread (not threadsafe, just able
// to run on threads other than the main thread).
//
// Arguments common to both loadResourceSynchronously() and create():
//
// - ThreadableLoaderOptions argument configures this ThreadableLoader's
//   behavior.
//
// - ResourceLoaderOptions argument will be passed to the FetchParameters
//   that this ThreadableLoader creates. It can be altered e.g. when
//   redirect happens.
class CORE_EXPORT ThreadableLoader
    : public GarbageCollectedFinalized<ThreadableLoader>,
      private RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadableLoader);

 public:
  static void LoadResourceSynchronously(ExecutionContext&,
                                        const ResourceRequest&,
                                        ThreadableLoaderClient&,
                                        const ThreadableLoaderOptions&,
                                        const ResourceLoaderOptions&);

  // Exposed for testing. Code outside this class should not call this function.
  static std::unique_ptr<ResourceRequest>
  CreateAccessControlPreflightRequestForTesting(const ResourceRequest&);

  // This method never returns nullptr.
  //
  // This method must always be followed by start() call.
  // ThreadableLoaderClient methods are never called before start() call.
  //
  // The async loading feature is separated into the create() method and
  // and the start() method in order to:
  // - reduce work done in a constructor
  // - not to ask the users to handle failures in the constructor and other
  //   async failures separately
  //
  // Loading completes when one of the following methods are called:
  // - didFinishLoading()
  // - didFail()
  // - didFailAccessControlCheck()
  // - didFailRedirectCheck()
  // After any of these methods is called, the loader won't call any of the
  // ThreadableLoaderClient methods.
  //
  // A user must guarantee that the loading completes before the attached
  // client gets invalid. Also, a user must guarantee that the loading
  // completes before the ThreadableLoader is destructed.
  //
  // When ThreadableLoader::cancel() is called,
  // ThreadableLoaderClient::didFail() is called with a ResourceError
  // with isCancellation() returning true, if any of didFinishLoading()
  // or didFail.*() methods have not been called yet. (didFail() may be
  // called with a ResourceError with isCancellation() returning true
  // also for cancellation happened inside the loader.)
  //
  // ThreadableLoaderClient methods may call cancel().
  static ThreadableLoader* Create(ExecutionContext&,
                                  ThreadableLoaderClient*,
                                  const ThreadableLoaderOptions&,
                                  const ResourceLoaderOptions&);

  ~ThreadableLoader() override;

  void Start(const ResourceRequest&);

  // A ThreadableLoader may have a timeout specified. It is possible, in some
  // cases, for the timeout to be overridden after the request is sent (for
  // example, XMLHttpRequests may override their timeout setting after sending).
  //
  // Set a new timeout relative to the time the request started, in
  // milliseconds.
  void OverrideTimeout(unsigned long timeout_milliseconds);

  void Cancel();

  // Detach the loader from the request. This ffunction is for "keepalive"
  // requests. No notification will be sent to the client, but the request
  // will be processed.
  void Detach();

  void SetDefersLoading(bool);

  void Trace(blink::Visitor* visitor) override;

 private:
  class DetachedClient;
  enum BlockingBehavior { kLoadSynchronously, kLoadAsynchronously };

  ThreadableLoader(ExecutionContext&,
                   ThreadableLoaderClient*,
                   BlockingBehavior,
                   const ThreadableLoaderOptions&,
                   const ResourceLoaderOptions&);

  static std::unique_ptr<ResourceRequest> CreateAccessControlPreflightRequest(
      const ResourceRequest&,
      const SecurityOrigin*);

  void Clear();

  // ResourceClient
  void NotifyFinished(Resource*) override;

  String DebugName() const override { return "ThreadableLoader"; }

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
  void DidDownloadToBlob(Resource*, scoped_refptr<BlobDataHandle>) override;

  // Notify Inspector and log to console about resource response. Use this
  // method if response is not going to be finished normally.
  void ReportResponseReceived(unsigned long identifier,
                              const ResourceResponse&);

  void DidTimeout(TimerBase*);
  // Calls the appropriate loading method according to policy and data about
  // origin. Only for handling the initial load (including fallback after
  // consulting ServiceWorker).
  void DispatchInitialRequest(ResourceRequest&);
  void MakeCrossOriginAccessRequest(const ResourceRequest&);

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
  // the actual request will be made later in NotifyFinished().
  void HandlePreflightResponse(const ResourceResponse&);

  void DispatchDidFailAccessControlCheck(const ResourceError&);
  void DispatchDidFail(const ResourceError&);

  void PrepareCrossOriginRequest(ResourceRequest&) const;

  // This method modifies the ResourceRequest by calling
  // SetAllowStoredCredentials() on it based on same-origin-ness and the
  // credentials mode.
  //
  // This method configures the ResourceLoaderOptions so that the underlying
  // ResourceFetcher doesn't perform some part of the CORS logic since this
  // class performs it by itself.
  void LoadRequest(ResourceRequest&, ResourceLoaderOptions);
  bool IsAllowedRedirect(network::mojom::FetchRequestMode, const KURL&) const;

  const SecurityOrigin* GetSecurityOrigin() const;

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
  scoped_refptr<const SecurityOrigin> security_origin_;

  // Set to true when the response data is given to a data consumer handle.
  bool is_using_data_consumer_handle_;

  const bool async_;

  // Holds the original request context (used for sanity checks).
  WebURLRequest::RequestContext request_context_;

  // Saved so that we can use the original value for the modes in
  // ResponseReceived() where |resource| might be a reused one (e.g. preloaded
  // resource) which can have different modes.
  network::mojom::FetchRequestMode fetch_request_mode_;
  network::mojom::FetchCredentialsMode fetch_credentials_mode_;

  // Holds the original request for fallback in case the Service Worker
  // does not respond.
  ResourceRequest fallback_request_for_service_worker_;

  // Holds the original request and options for it during preflight request
  // handling phase.
  ResourceRequest actual_request_;
  ResourceLoaderOptions actual_options_;

  // stores request headers in case of a cross-origin redirect.
  HTTPHeaderMap request_headers_;

  TaskRunnerTimer<ThreadableLoader> timeout_timer_;
  TimeTicks request_started_;  // Time an asynchronous fetch request is started

  // Max number of times that this ThreadableLoader can follow
  // cross-origin redirects. This is used to limit the number of redirects. But
  // this value is not the max number of total redirects allowed, because
  // same-origin redirects are not counted here.
  int cors_redirect_limit_;

  network::mojom::FetchRedirectMode redirect_mode_;

  // Holds the referrer after a redirect response was received. This referrer is
  // used to populate the HTTP Referer header when following the redirect.
  bool override_referrer_;
  Referrer referrer_after_redirect_;

  bool detached_ = false;

  RawResourceClientStateChecker checker_;

  DISALLOW_COPY_AND_ASSIGN(ThreadableLoader);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADABLE_LOADER_H_
