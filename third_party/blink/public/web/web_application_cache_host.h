/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_APPLICATION_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_APPLICATION_CACHE_HOST_H_

#include "third_party/blink/public/mojom/appcache/appcache.mojom-shared.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace WTF {
class String;
}

namespace blink {

class WebApplicationCacheHostClient;
class WebLocalFrame;
class WebURL;
class WebURLResponse;

// This interface is used by webkit to call out to the embedder. Webkit uses
// the WebLocalFrameClient::CreateApplicationCacheHost method to create
// instances, and calls delete when the instance is no longer needed.
class WebApplicationCacheHost {
 public:
  virtual ~WebApplicationCacheHost() = default;

  // TODO(crbug.com/950159): Remove
  // CreateWebApplicationCacheHostFor{SharedWorker,Frame}.
  // Instantiate a WebApplicationCacheHost that interacts with the frame or the
  // shared worker.
  BLINK_EXPORT static std::unique_ptr<WebApplicationCacheHost>
  CreateWebApplicationCacheHostForFrame(
      WebLocalFrame* frame,
      WebApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  BLINK_EXPORT static std::unique_ptr<WebApplicationCacheHost>
  CreateWebApplicationCacheHostForSharedWorker(
      WebApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Called for every request made within the context.
  virtual void WillStartMainResourceRequest(
      const WebURL& url,
      const WTF::String& method,
      const WebApplicationCacheHost* spawning_host) {}

  // One or the other selectCache methods is called after having parsed the
  // <html> tag.  The latter returns false if the current document has been
  // identified as a "foreign" entry, in which case the frame navigation will be
  // restarted by webkit.
  virtual void SelectCacheWithoutManifest() {}
  virtual bool SelectCacheWithManifest(const WebURL& manifest_url) {
    return true;
  }

  // Called as the main resource is retrieved.
  virtual void DidReceiveResponseForMainResource(const WebURLResponse&) {}

  // Called on behalf of the scriptable interface.
  virtual mojom::AppCacheStatus GetStatus() {
    return mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
  }
  virtual bool StartUpdate() { return false; }
  virtual bool SwapCache() { return false; }
  virtual void Abort() {}

  // Structures and methods to support inspecting Application Caches.
  struct CacheInfo {
    WebURL manifest_url;  // Empty if there is no associated cache.
    double creation_time;
    double update_time;
    // Sums up the sizes of all the responses in this cache.
    int64_t response_sizes;
    // Sums up the padding sizes for all opaque responses in the cache.
    int64_t padding_sizes;
    CacheInfo()
        : creation_time(0),
          update_time(0),
          response_sizes(0),
          padding_sizes(0) {}
  };
  struct ResourceInfo {
    WebURL url;
    // Disk space consumed by this resource.
    int64_t response_size;
    // Padding added when the Quota API counts this resource.
    //
    // Non-zero only for opaque responses.
    int64_t padding_size;
    bool is_master;
    bool is_manifest;
    bool is_explicit;
    bool is_foreign;
    bool is_fallback;
    ResourceInfo()
        : response_size(0),
          padding_size(0),
          is_master(false),
          is_manifest(false),
          is_explicit(false),
          is_foreign(false),
          is_fallback(false) {}
  };
  virtual void GetAssociatedCacheInfo(CacheInfo*) {}
  virtual void GetResourceList(WebVector<ResourceInfo>*) {}
  virtual void DeleteAssociatedCacheGroup() {}
  virtual const base::UnguessableToken& GetHostID() const {
    return base::UnguessableToken::Null();
  }
  // TODO(crbug.com/950159): Remove this API once
  // ApplicationCacheHostForSharedWorker is created in WebSharedWorkerImpl.
  virtual void SelectCacheForSharedWorker(
      int64_t app_cache_id,
      base::OnceClosure completion_callback) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_APPLICATION_CACHE_HOST_H_
