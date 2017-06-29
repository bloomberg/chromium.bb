/*
 * Copyright (C) 2005, 2006, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include <memory>
#include "base/gtest_prod_util.h"
#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoadScheduler.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"

namespace blink {

class FetchContext;
class ResourceError;
class ResourceFetcher;

// A ResourceLoader is created for each Resource by the ResourceFetcher when it
// needs to load the specified resource. A ResourceLoader creates a
// WebURLLoader and loads the resource using it. Any per-load logic should be
// implemented in this class basically.
class PLATFORM_EXPORT ResourceLoader final
    : public GarbageCollectedFinalized<ResourceLoader>,
      public ResourceLoadSchedulerClient,
      protected WebURLLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(ResourceLoader);
  USING_PRE_FINALIZER(ResourceLoader, Dispose);

 public:
  static ResourceLoader* Create(ResourceFetcher*,
                                ResourceLoadScheduler*,
                                Resource*);
  ~ResourceLoader() override;
  DECLARE_VIRTUAL_TRACE();

  void Start();

  void Cancel();

  void SetDefersLoading(bool);

  void DidChangePriority(ResourceLoadPriority, int intra_priority_value);

  // Called before start() to activate cache-aware loading if enabled in
  // |m_resource->options()| and applicable.
  void ActivateCacheAwareLoadingIfNeeded(const ResourceRequest&);

  bool IsCacheAwareLoadingActivated() const {
    return is_cache_aware_loading_activated_;
  }

  ResourceFetcher* Fetcher() { return fetcher_; }
  bool GetKeepalive() const;

  // WebURLLoaderClient
  //
  // A succesful load will consist of:
  // 0+  willFollowRedirect()
  // 0+  didSendData()
  // 1   didReceiveResponse()
  // 0-1 didReceiveCachedMetadata()
  // 0+  didReceiveData() or didDownloadData(), but never both
  // 1   didFinishLoading()
  // A failed load is indicated by 1 didFail(), which can occur at any time
  // before didFinishLoading(), including synchronous inside one of the other
  // callbacks via ResourceLoader::cancel()
  bool WillFollowRedirect(WebURLRequest&,
                          const WebURLResponse& redirect_response) override;
  void DidSendData(unsigned long long bytes_sent,
                   unsigned long long total_bytes_to_be_sent) override;
  void DidReceiveResponse(const WebURLResponse&) override;
  void DidReceiveResponse(const WebURLResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveCachedMetadata(const char* data, int length) override;
  void DidReceiveData(const char*, int) override;
  void DidReceiveTransferSizeUpdate(int transfer_size_diff) override;
  void DidDownloadData(int, int) override;
  void DidFinishLoading(double finish_time,
                        int64_t encoded_data_length,
                        int64_t encoded_body_length,
                        int64_t decoded_body_length) override;
  void DidFail(const WebURLError&,
               int64_t encoded_data_length,
               int64_t encoded_body_length,
               int64_t decoded_body_length) override;
  void HandleError(const ResourceError&);

  void DidFinishLoadingFirstPartInMultipart();

  // ResourceLoadSchedulerClient.
  void Run() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceLoaderTest, DetermineCORSStatus);

  friend class SubresourceIntegrityTest;

  // Assumes ResourceFetcher and Resource are non-null.
  ResourceLoader(ResourceFetcher*, ResourceLoadScheduler*, Resource*);

  void StartWith(const ResourceRequest&);

  void Release(ResourceLoadScheduler::ReleaseOption);

  // This method is currently only used for service worker fallback request and
  // cache-aware loading, other users should be careful not to break
  // ResourceLoader state.
  void Restart(const ResourceRequest&);

  FetchContext& Context() const;
  ResourceRequestBlockedReason CanAccessResponse(Resource*,
                                                 const ResourceResponse&,
                                                 const String&) const;

  CORSStatus DetermineCORSStatus(const ResourceResponse&, StringBuilder&) const;

  void CancelForRedirectAccessCheckError(const KURL&,
                                         ResourceRequestBlockedReason);
  void RequestSynchronously(const ResourceRequest&);
  void Dispose();

  std::unique_ptr<WebURLLoader> loader_;
  ResourceLoadScheduler::ClientId scheduler_client_id_;
  Member<ResourceFetcher> fetcher_;
  Member<ResourceLoadScheduler> scheduler_;
  Member<Resource> resource_;

  // Set when the request's "keepalive" is specified (e.g., for SendBeacon).
  // https://fetch.spec.whatwg.org/#request-keepalive-flag
  SelfKeepAlive<ResourceLoader> keepalive_;

  bool is_cache_aware_loading_activated_;
};

}  // namespace blink

#endif
