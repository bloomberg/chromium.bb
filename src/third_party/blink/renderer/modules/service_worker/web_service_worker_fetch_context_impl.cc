// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/web_service_worker_fetch_context_impl.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/internet_disconnected_web_url_loader.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

namespace blink {

// static
scoped_refptr<WebServiceWorkerFetchContext>
WebServiceWorkerFetchContext::Create(
    const RendererPreferences& renderer_preferences,
    const WebURL& worker_script_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_script_loader_factory,
    const WebURL& script_url_to_skip_throttling,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    CrossVariantMojoReceiver<
        mojom::blink::RendererPreferenceWatcherInterfaceBase>
        preference_watcher_receiver,
    CrossVariantMojoReceiver<
        mojom::blink::SubresourceLoaderUpdaterInterfaceBase>
        pending_subresource_loader_updater,
    const WebVector<WebString>& cors_exempt_header_list) {
  // Create isolated copies for `worker_script_url` and
  // `script_url_to_skip_throttling` as the fetch context will be used on a
  // service worker thread that is different from the current thread.
  return base::MakeRefCounted<WebServiceWorkerFetchContextImpl>(
      renderer_preferences, KURL::CreateIsolated(worker_script_url.GetString()),
      std::move(pending_url_loader_factory),
      std::move(pending_script_loader_factory),
      KURL::CreateIsolated(script_url_to_skip_throttling.GetString()),
      std::move(throttle_provider),
      std::move(websocket_handshake_throttle_provider),
      std::move(preference_watcher_receiver),
      std::move(pending_subresource_loader_updater), cors_exempt_header_list);
}

WebServiceWorkerFetchContextImpl::WebServiceWorkerFetchContextImpl(
    const RendererPreferences& renderer_preferences,
    const KURL& worker_script_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_script_loader_factory,
    const KURL& script_url_to_skip_throttling,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    mojo::PendingReceiver<mojom::blink::RendererPreferenceWatcher>
        preference_watcher_receiver,
    mojo::PendingReceiver<mojom::blink::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    const WebVector<WebString>& cors_exempt_header_list)
    : renderer_preferences_(renderer_preferences),
      worker_script_url_(worker_script_url),
      pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      pending_script_loader_factory_(std::move(pending_script_loader_factory)),
      script_url_to_skip_throttling_(script_url_to_skip_throttling),
      throttle_provider_(std::move(throttle_provider)),
      websocket_handshake_throttle_provider_(
          std::move(websocket_handshake_throttle_provider)),
      preference_watcher_pending_receiver_(
          std::move(preference_watcher_receiver)),
      pending_subresource_loader_updater_(
          std::move(pending_subresource_loader_updater)),
      cors_exempt_header_list_(cors_exempt_header_list) {}

WebServiceWorkerFetchContextImpl::~WebServiceWorkerFetchContextImpl() = default;

void WebServiceWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

void WebServiceWorkerFetchContextImpl::InitializeOnWorkerThread(
    AcceptLanguagesWatcher* watcher) {
  preference_watcher_receiver_.Bind(
      std::move(preference_watcher_pending_receiver_));
  subresource_loader_updater_.Bind(
      std::move(pending_subresource_loader_updater_));

  web_url_loader_factory_ = std::make_unique<WebURLLoaderFactory>(
      network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory_)),
      cors_exempt_header_list_, terminate_sync_load_event_);

  internet_disconnected_web_url_loader_factory_ =
      std::make_unique<InternetDisconnectedWebURLLoaderFactory>();

  if (pending_script_loader_factory_) {
    web_script_loader_factory_ = std::make_unique<WebURLLoaderFactory>(
        network::SharedURLLoaderFactory::Create(
            std::move(pending_script_loader_factory_)),
        cors_exempt_header_list_, terminate_sync_load_event_);
  }

  accept_languages_watcher_ = watcher;
}

WebURLLoaderFactory* WebServiceWorkerFetchContextImpl::GetURLLoaderFactory() {
  if (is_offline_mode_)
    return internet_disconnected_web_url_loader_factory_.get();
  return web_url_loader_factory_.get();
}

std::unique_ptr<WebURLLoaderFactory>
WebServiceWorkerFetchContextImpl::WrapURLLoaderFactory(
    CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        url_loader_factory) {
  return std::make_unique<WebURLLoaderFactory>(
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          std::move(url_loader_factory)),
      cors_exempt_header_list_, terminate_sync_load_event_);
}

WebURLLoaderFactory*
WebServiceWorkerFetchContextImpl::GetScriptLoaderFactory() {
  return web_script_loader_factory_.get();
}

void WebServiceWorkerFetchContextImpl::WillSendRequest(WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(WebString::FromUTF8(kDoNotTrackHeader), "1");
  }
  auto url_request_extra_data = base::MakeRefCounted<WebURLRequestExtraData>();
  url_request_extra_data->set_originated_from_service_worker(true);

  const bool needs_to_skip_throttling =
      static_cast<KURL>(request.Url()) == script_url_to_skip_throttling_ &&
      (request.GetRequestContext() ==
           mojom::blink::RequestContextType::SERVICE_WORKER ||
       request.GetRequestContext() == mojom::blink::RequestContextType::SCRIPT);
  if (needs_to_skip_throttling) {
    // Throttling is needed when the skipped script is loaded again because it's
    // served from ServiceWorkerInstalledScriptLoader after the second time,
    // while at the first time the script comes from
    // ServiceWorkerUpdatedScriptLoader which uses ThrottlingURLLoader in the
    // browser process. See also comments at
    // EmbeddedWorkerStartParams::script_url_to_skip_throttling.
    // TODO(https://crbug.com/993641): need to simplify throttling for service
    // worker scripts.
    script_url_to_skip_throttling_ = KURL();
  } else if (throttle_provider_) {
    url_request_extra_data->set_url_loader_throttles(
        throttle_provider_->CreateThrottles(MSG_ROUTING_NONE, request));
  }

  request.SetURLRequestExtraData(std::move(url_request_extra_data));

  if (!renderer_preferences_.enable_referrers) {
    request.SetReferrerString(WebString());
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }
}

mojom::ControllerServiceWorkerMode
WebServiceWorkerFetchContextImpl::GetControllerServiceWorkerMode() const {
  return mojom::ControllerServiceWorkerMode::kNoController;
}

net::SiteForCookies WebServiceWorkerFetchContextImpl::SiteForCookies() const {
  // According to the spec, we can use the |worker_script_url_| for
  // SiteForCookies, because "site for cookies" for the service worker is
  // the service worker's origin's host's registrable domain.
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-07#section-2.1.2
  return net::SiteForCookies::FromUrl(worker_script_url_);
}

absl::optional<WebSecurityOrigin>
WebServiceWorkerFetchContextImpl::TopFrameOrigin() const {
  return absl::nullopt;
}

std::unique_ptr<WebSocketHandshakeThrottle>
WebServiceWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      MSG_ROUTING_NONE, std::move(task_runner));
}

void WebServiceWorkerFetchContextImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  web_url_loader_factory_ = std::make_unique<WebURLLoaderFactory>(
      network::SharedURLLoaderFactory::Create(
          std::move(subresource_loader_factories)),
      cors_exempt_header_list_, terminate_sync_load_event_);
}

void WebServiceWorkerFetchContextImpl::NotifyUpdate(
    const RendererPreferences& new_prefs) {
  DCHECK(accept_languages_watcher_);
  if (renderer_preferences_.accept_languages != new_prefs.accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = new_prefs;
}

WebString WebServiceWorkerFetchContextImpl::GetAcceptLanguages() const {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
}

CrossVariantMojoReceiver<mojom::WorkerTimingContainerInterfaceBase>
WebServiceWorkerFetchContextImpl::TakePendingWorkerTimingReceiver(
    int request_id) {
  // No receiver exists because requests from service workers are never handled
  // by a service worker.
  return {};
}

void WebServiceWorkerFetchContextImpl::SetIsOfflineMode(bool is_offline_mode) {
  is_offline_mode_ = is_offline_mode;
}

}  // namespace blink
