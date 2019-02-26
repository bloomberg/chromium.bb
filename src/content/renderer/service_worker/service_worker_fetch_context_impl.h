// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/public/common/renderer_preference_watcher.mojom.h"
#include "content/public/common/renderer_preferences.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "url/gurl.h"

namespace content {
class ResourceDispatcher;
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;

class ServiceWorkerFetchContextImpl final
    : public blink::WebWorkerFetchContext,
      public mojom::RendererPreferenceWatcher {
 public:
  // |url_loader_factory_info| is used for regular loads from the service worker
  // (i.e., Fetch API). It typically goes to network, but it might internally
  // contain non-NetworkService factories for handling non-http(s) URLs like
  // chrome-extension://.
  // |script_loader_factory_info| is used for importScripts() from the service
  // worker when InstalledScriptsManager doesn't have the requested script. It
  // is a ServiceWorkerScriptLoaderFactory, which loads and installs the script.
  ServiceWorkerFetchContextImpl(
      RendererPreferences renderer_preferences,
      const GURL& worker_script_url,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          script_loader_factory_info,
      int service_worker_provider_id,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      mojom::RendererPreferenceWatcherRequest preference_watcher_request);

  // blink::WebWorkerFetchContext implementation:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread() override;
  blink::WebURLLoaderFactory* GetURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  blink::WebURLLoaderFactory* GetScriptLoaderFactory() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      const override;
  blink::WebURL SiteForCookies() const override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;

 private:
  ~ServiceWorkerFetchContextImpl() override;

  // Implements mojom::RendererPreferenceWatcher.
  void NotifyUpdate(const RendererPreferences& new_prefs) override;

  RendererPreferences renderer_preferences_;
  const GURL worker_script_url_;
  // Consumed on the worker thread to create |web_url_loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;
  // Consumed on the worker thread to create |web_script_loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
      script_loader_factory_info_;
  const int service_worker_provider_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Responsible for regular loads from the service worker (i.e., Fetch API).
  std::unique_ptr<blink::WebURLLoaderFactory> web_url_loader_factory_;
  // Responsible for handling importScripts().
  std::unique_ptr<blink::WebURLLoaderFactory> web_script_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  mojo::Binding<mojom::RendererPreferenceWatcher> preference_watcher_binding_;

  // Kept while staring up the worker thread. Valid until
  // InitializeOnWorkerThread().
  mojom::RendererPreferenceWatcherRequest preference_watcher_request_;

  // This is owned by ThreadedMessagingProxyBase on the main thread.
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
