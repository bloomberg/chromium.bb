// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_APPLICATION_CACHE_HOST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_APPLICATION_CACHE_HOST_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_application_cache_host.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class WebApplicationCacheHostClient;

class WebApplicationCacheHostImpl : public WebApplicationCacheHost,
                                    public mojom::blink::AppCacheFrontend {
 public:
  WebApplicationCacheHostImpl(
      // |web_frame| is used for accessing to DocumentInterfaceBroker. As it's
      // not used for workers, |web_frame| is null for workers.
      WebLocalFrame* web_frame,
      WebApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebApplicationCacheHostImpl() override;

  const base::UnguessableToken& host_id() const { return host_id_; }
  WebApplicationCacheHostClient* client() const { return client_; }

  // mojom::blink::AppCacheFrontend
  void CacheSelected(mojom::blink::AppCacheInfoPtr info) override;
  void EventRaised(mojom::blink::AppCacheEventID event_id) override;
  void ProgressEventRaised(const KURL& url,
                           int32_t num_total,
                           int32_t num_complete) override;
  void ErrorEventRaised(mojom::blink::AppCacheErrorDetailsPtr details) override;

  // WebApplicationCacheHost:
  void WillStartMainResourceRequest(
      const WebURL& url,
      const String& method,
      const WebApplicationCacheHost* spawning_host) override;
  void SelectCacheWithoutManifest() override;
  bool SelectCacheWithManifest(const WebURL& manifestURL) override;
  void DidReceiveResponseForMainResource(const WebURLResponse&) override;
  mojom::blink::AppCacheStatus GetStatus() override;
  bool StartUpdate() override;
  bool SwapCache() override;
  void GetResourceList(WebVector<ResourceInfo>* resources) override;
  void GetAssociatedCacheInfo(CacheInfo* info) override;
  const base::UnguessableToken& GetHostID() const override;
  void SelectCacheForSharedWorker(
      int64_t app_cache_id,
      base::OnceClosure completion_callback) override;

 private:
  enum IsNewMasterEntry { MAYBE_NEW_ENTRY, NEW_ENTRY, OLD_ENTRY };

  mojo::Binding<mojom::blink::AppCacheFrontend> binding_;
  WebApplicationCacheHostClient* client_;
  mojom::blink::AppCacheHostPtr backend_host_;
  base::UnguessableToken host_id_;
  mojom::blink::AppCacheStatus status_;
  WebURLResponse document_response_;
  KURL document_url_;
  bool is_scheme_supported_;
  bool is_get_method_;
  IsNewMasterEntry is_new_master_entry_;
  mojom::blink::AppCacheInfo cache_info_;
  KURL original_main_resource_url_;  // Used to detect redirection.
  bool was_select_cache_called_;
  // Invoked when CacheSelected() is called.
  base::OnceClosure select_cache_for_shared_worker_completion_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_APPLICATION_CACHE_HOST_IMPL_H_
