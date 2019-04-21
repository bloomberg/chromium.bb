/*
 * Copyright (c) 2009, Google Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/platform/web_application_cache_host_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class ApplicationCache;
class DocumentLoader;
class ResourceRequest;
class ResourceResponse;

class CORE_EXPORT ApplicationCacheHost final
    : public GarbageCollectedFinalized<ApplicationCacheHost>,
      public WebApplicationCacheHostClient {
 public:
  explicit ApplicationCacheHost(DocumentLoader*);
  ~ApplicationCacheHost() override;
  void DetachFromDocumentLoader();

  struct CacheInfo {
    STACK_ALLOCATED();

   public:
    CacheInfo(const KURL& manifest,
              double creation_time,
              double update_time,
              int64_t response_sizes,
              int64_t padding_sizes)
        : manifest_(manifest),
          creation_time_(creation_time),
          update_time_(update_time),
          response_sizes_(response_sizes),
          padding_sizes_(padding_sizes) {}
    KURL manifest_;
    double creation_time_;
    double update_time_;
    int64_t response_sizes_;
    int64_t padding_sizes_;
  };

  struct ResourceInfo {
    DISALLOW_NEW();
    ResourceInfo(const KURL& resource,
                 bool is_master,
                 bool is_manifest,
                 bool is_fallback,
                 bool is_foreign,
                 bool is_explicit,
                 int64_t response_size,
                 int64_t padding_size)
        : resource_(resource),
          is_master_(is_master),
          is_manifest_(is_manifest),
          is_fallback_(is_fallback),
          is_foreign_(is_foreign),
          is_explicit_(is_explicit),
          response_size_(response_size),
          padding_size_(padding_size) {}
    KURL resource_;
    bool is_master_;
    bool is_manifest_;
    bool is_fallback_;
    bool is_foreign_;
    bool is_explicit_;
    int64_t response_size_;
    int64_t padding_size_;
  };

  typedef Vector<ResourceInfo> ResourceInfoList;

  void SelectCacheWithoutManifest();
  void SelectCacheWithManifest(const KURL& manifest_url);

  // Annotate request for ApplicationCache. This internally calls
  // willStartLoadingMainResource if it's for frame resource or
  // willStartLoadingResource for subresource requests.
  void WillStartLoading(ResourceRequest&);
  void WillStartLoadingMainResource(DocumentLoader*,
                                    const KURL&,
                                    const String& method);

  void DidReceiveResponseForMainResource(const ResourceResponse&);
  void MainResourceDataReceived(const char* data, size_t length);

  mojom::AppCacheStatus GetStatus() const;
  bool Update();
  bool SwapCache();
  void Abort();

  void SetApplicationCache(ApplicationCache*);
  void NotifyApplicationCache(mojom::AppCacheEventID,
                              int progress_total,
                              int progress_done,
                              mojom::AppCacheErrorReason,
                              const String& error_url,
                              int error_status,
                              const String& error_message);

  void
  StopDeferringEvents();  // Also raises the events that have been queued up.

  void FillResourceList(ResourceInfoList*);
  CacheInfo ApplicationCacheInfo();
  int GetHostID() const;

  void Trace(blink::Visitor*);

 private:
  // WebApplicationCacheHostClient implementation
  void DidChangeCacheAssociation() final;
  void NotifyEventListener(mojom::AppCacheEventID) final;
  void NotifyProgressEventListener(const WebURL&,
                                   int progress_total,
                                   int progress_done) final;
  void NotifyErrorEventListener(mojom::AppCacheErrorReason,
                                const WebURL&,
                                int status,
                                const WebString& message) final;

  bool IsApplicationCacheEnabled();
  DocumentLoader* GetDocumentLoader() const { return document_loader_; }

  struct DeferredEvent {
    mojom::AppCacheEventID event_id;
    int progress_total;
    int progress_done;
    mojom::AppCacheErrorReason error_reason;
    String error_url;
    int error_status;
    String error_message;
    DeferredEvent(mojom::AppCacheEventID id,
                  int progress_total,
                  int progress_done,
                  mojom::AppCacheErrorReason error_reason,
                  const String& error_url,
                  int error_status,
                  const String& error_message)
        : event_id(id),
          progress_total(progress_total),
          progress_done(progress_done),
          error_reason(error_reason),
          error_url(error_url),
          error_status(error_status),
          error_message(error_message) {}
  };

  WeakMember<ApplicationCache> dom_application_cache_;
  Member<DocumentLoader> document_loader_;
  bool defers_events_;  // Events are deferred until after document onload.
  Vector<DeferredEvent> deferred_events_;

  void DispatchDOMEvent(mojom::AppCacheEventID,
                        int progress_total,
                        int progress_done,
                        mojom::AppCacheErrorReason,
                        const String& error_url,
                        int error_status,
                        const String& error_message);

  std::unique_ptr<WebApplicationCacheHost> host_;

  FRIEND_TEST_ALL_PREFIXES(DocumentTest, SandboxDisablesAppCache);

  DISALLOW_COPY_AND_ASSIGN(ApplicationCacheHost);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_
