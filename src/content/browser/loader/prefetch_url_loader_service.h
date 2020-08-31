// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"

namespace storage {
class BlobStorageContext;
}

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class BrowserContext;
class PrefetchedSignedExchangeCache;
class RenderFrameHostImpl;
class URLLoaderFactoryGetter;

class CONTENT_EXPORT PrefetchURLLoaderService final
    : public base::RefCountedThreadSafe<
          PrefetchURLLoaderService,
          content::BrowserThread::DeleteOnUIThread>,
      public blink::mojom::RendererPreferenceWatcher,
      public network::mojom::URLLoaderFactory {
 public:
  explicit PrefetchURLLoaderService(BrowserContext* browser_context);

  void GetFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      int frame_tree_node_id,
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      base::WeakPtr<RenderFrameHostImpl> render_frame_host,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache);

  // Register a callback that is fired right before a prefetch load is started
  // by this service.
  void RegisterPrefetchLoaderCallbackForTest(
      const base::RepeatingClosure& prefetch_load_callback) {
    prefetch_load_callback_for_testing_ = prefetch_load_callback;
  }

  scoped_refptr<SignedExchangePrefetchMetricRecorder>
  signed_exchange_prefetch_metric_recorder() {
    return signed_exchange_prefetch_metric_recorder_;
  }
  void SetAcceptLanguages(const std::string& accept_langs) {
    accept_langs_ = accept_langs;
  }

 private:
  friend class base::DeleteHelper<content::PrefetchURLLoaderService>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  struct BindContext;

  ~PrefetchURLLoaderService() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request_in,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // This ensures that the BindContext's |cross_origin_factory| member exists
  // by setting it to a special URLLoaderFactory created by the current
  // context's RenderFrameHost.
  void EnsureCrossOriginFactory();
  bool IsValidCrossOriginPrefetch(
      const network::ResourceRequest& resource_request);

  base::UnguessableToken GenerateRecursivePrefetchToken(
      base::WeakPtr<BindContext> bind_context,
      const network::ResourceRequest& request);

  // blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(blink::mojom::RendererPreferencesPtr new_prefs) override;

  // For URLLoaderThrottlesGetter.
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(const network::ResourceRequest& request,
                           int frame_tree_node_id);

  const std::unique_ptr<BindContext>& current_bind_context() const {
    return loader_factory_receivers_.current_context();
  }

  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;
  BrowserContext* browser_context_ = nullptr;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    std::unique_ptr<BindContext>>
      loader_factory_receivers_;
  // Used in the IO thread.
  mojo::Receiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_{this};

  base::RepeatingClosure prefetch_load_callback_for_testing_;

  scoped_refptr<SignedExchangePrefetchMetricRecorder>
      signed_exchange_prefetch_metric_recorder_;

  std::string accept_langs_;

  // Used to create a BlobDataHandle from a DataPipe of signed exchange's inner
  // response body to store to |prefetched_signed_exchange_cache_| when
  // SignedExchangeSubresourcePrefetch is enabled.
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchURLLoaderService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_H_
