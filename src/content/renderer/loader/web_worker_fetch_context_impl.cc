// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_worker_fetch_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "content/child/child_thread_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/loader/code_cache_loader_impl.h"
#include "content/renderer/loader/frame_request_blocker.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/worker/service_worker_network_provider_for_worker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_types.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"

namespace content {

WebWorkerFetchContextImpl::RewriteURLFunction g_rewrite_url = nullptr;

namespace {

// Runs on a background thread created in ResetServiceWorkerURLLoaderFactory().
void CreateServiceWorkerSubresourceLoaderFactory(
    blink::mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
    const std::string& client_id,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory,
    network::mojom::URLLoaderFactoryRequest request,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  ServiceWorkerSubresourceLoaderFactory::Create(
      base::MakeRefCounted<ControllerServiceWorkerConnector>(
          std::move(container_host_info), nullptr /* controller_ptr */,
          client_id),
      network::SharedURLLoaderFactory::Create(std::move(fallback_factory)),
      std::move(request), std::move(task_runner));
}

}  // namespace

// static
void WebWorkerFetchContextImpl::InstallRewriteURLFunction(
    RewriteURLFunction rewrite_url) {
  CHECK(!g_rewrite_url);
  g_rewrite_url = rewrite_url;
}

// An implementation of WebURLLoaderFactory that is aware of service workers. In
// the usual case, it creates a loader that uses |loader_factory_|. But if the
// worker fetch context is controlled by a service worker, it creates a loader
// that uses |service_worker_loader_factory_| for requests that should be
// intercepted by the service worker.
class WebWorkerFetchContextImpl::Factory : public blink::WebURLLoaderFactory {
 public:
  Factory(base::WeakPtr<ResourceDispatcher> resource_dispatcher,
          scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
      : resource_dispatcher_(std::move(resource_dispatcher)),
        loader_factory_(std::move(loader_factory)),
        weak_ptr_factory_(this) {}
  ~Factory() override = default;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override {
    DCHECK(task_runner_handle);
    DCHECK(resource_dispatcher_);
    if (CanCreateServiceWorkerURLLoader(request)) {
      // Create our own URLLoader to route the request to the controller service
      // worker.
      return std::make_unique<WebURLLoaderImpl>(resource_dispatcher_.get(),
                                                std::move(task_runner_handle),
                                                service_worker_loader_factory_);
    }

    return std::make_unique<WebURLLoaderImpl>(resource_dispatcher_.get(),
                                              std::move(task_runner_handle),
                                              loader_factory_);
  }

  void SetServiceWorkerURLLoaderFactory(
      network::mojom::URLLoaderFactoryPtr service_worker_loader_factory) {
    if (!service_worker_loader_factory) {
      service_worker_loader_factory_ = nullptr;
      return;
    }
    service_worker_loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(service_worker_loader_factory));
  }

  base::WeakPtr<Factory> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  bool CanCreateServiceWorkerURLLoader(const blink::WebURLRequest& request) {
    // TODO(horo): Unify this code path with
    // ServiceWorkerNetworkProviderForFrame::CreateURLLoader that is used
    // for document cases.

    // We need the service worker loader factory populated in order to create
    // our own URLLoader for subresource loading via a service worker.
    if (!service_worker_loader_factory_)
      return false;

    // If the URL is not http(s) or otherwise whitelisted, do not intercept the
    // request. Schemes like 'blob' and 'file' are not eligible to be
    // intercepted by service workers.
    // TODO(falken): Let ServiceWorkerSubresourceLoaderFactory handle the
    // request and move this check there (i.e., for such URLs, it should use
    // its fallback factory).
    if (!GURL(request.Url()).SchemeIsHTTPOrHTTPS() &&
        !OriginCanAccessServiceWorkers(request.Url())) {
      return false;
    }

    // If GetSkipServiceWorker() returns true, no need to intercept the request.
    if (request.GetSkipServiceWorker())
      return false;

    return true;
  }

  base::WeakPtr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> service_worker_loader_factory_;
  base::WeakPtrFactory<Factory> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(Factory);
};

scoped_refptr<WebWorkerFetchContextImpl> WebWorkerFetchContextImpl::Create(
    ServiceWorkerProviderContext* provider_context,
    blink::mojom::RendererPreferences renderer_preferences,
    blink::mojom::RendererPreferenceWatcherRequest watcher_request,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        fallback_factory_info) {
  blink::mojom::ServiceWorkerWorkerClientRequest service_worker_client_request;
  blink::mojom::ServiceWorkerWorkerClientRegistryPtrInfo
      service_worker_worker_client_registry_ptr_info;
  blink::mojom::ServiceWorkerContainerHostPtrInfo container_host_ptr_info;

  // Some sandboxed iframes are not allowed to use service worker so don't have
  // a real service worker provider, so the provider context is null.
  if (provider_context) {
    provider_context->CloneWorkerClientRegistry(
        mojo::MakeRequest(&service_worker_worker_client_registry_ptr_info));

    blink::mojom::ServiceWorkerWorkerClientPtr worker_client_ptr;
    service_worker_client_request = mojo::MakeRequest(&worker_client_ptr);
    provider_context->RegisterWorkerClient(std::move(worker_client_ptr));

    container_host_ptr_info = provider_context->CloneContainerHostPtrInfo();
  }

  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      base::AdoptRef(new WebWorkerFetchContextImpl(
          std::move(renderer_preferences), std::move(watcher_request),
          std::move(service_worker_client_request),
          std::move(service_worker_worker_client_registry_ptr_info),
          std::move(container_host_ptr_info), std::move(loader_factory_info),
          std::move(fallback_factory_info),
          GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
              URLLoaderThrottleProviderType::kWorker),
          GetContentClient()
              ->renderer()
              ->CreateWebSocketHandshakeThrottleProvider(),
          ChildThreadImpl::current()->thread_safe_sender(),
          ChildThreadImpl::current()->GetConnector()->Clone()));
  if (provider_context) {
    worker_fetch_context->set_is_controlled_by_service_worker(
        provider_context->IsControlledByServiceWorker());
    worker_fetch_context->set_client_id(provider_context->client_id());
  } else {
    worker_fetch_context->set_is_controlled_by_service_worker(
        blink::mojom::ControllerServiceWorkerMode::kNoController);
  }
  return worker_fetch_context;
}

WebWorkerFetchContextImpl::WebWorkerFetchContextImpl(
    blink::mojom::RendererPreferences renderer_preferences,
    blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    blink::mojom::ServiceWorkerWorkerClientRequest
        service_worker_client_request,
    blink::mojom::ServiceWorkerWorkerClientRegistryPtrInfo
        service_worker_worker_client_registry_info,
    blink::mojom::ServiceWorkerContainerHostPtrInfo
        service_worker_container_host_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory_info,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    ThreadSafeSender* thread_safe_sender,
    std::unique_ptr<service_manager::Connector> service_manager_connection)
    : binding_(this),
      service_worker_client_request_(std::move(service_worker_client_request)),
      service_worker_worker_client_registry_info_(
          std::move(service_worker_worker_client_registry_info)),
      service_worker_container_host_info_(
          std::move(service_worker_container_host_info)),
      loader_factory_info_(std::move(loader_factory_info)),
      fallback_factory_info_(std::move(fallback_factory_info)),
      thread_safe_sender_(thread_safe_sender),
      renderer_preferences_(std::move(renderer_preferences)),
      preference_watcher_binding_(this),
      preference_watcher_request_(std::move(preference_watcher_request)),
      throttle_provider_(std::move(throttle_provider)),
      websocket_handshake_throttle_provider_(
          std::move(websocket_handshake_throttle_provider)),
      service_manager_connection_(std::move(service_manager_connection)) {}

WebWorkerFetchContextImpl::~WebWorkerFetchContextImpl() {}

void WebWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

scoped_refptr<blink::WebWorkerFetchContext>
WebWorkerFetchContextImpl::CloneForNestedWorker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  blink::mojom::ServiceWorkerWorkerClientRequest service_worker_client_request;
  blink::mojom::ServiceWorkerWorkerClientRegistryPtrInfo
      service_worker_worker_client_registry_ptr_info;
  if (service_worker_worker_client_registry_) {
    blink::mojom::ServiceWorkerWorkerClientPtr service_worker_client_ptr;
    service_worker_client_request =
        mojo::MakeRequest(&service_worker_client_ptr);
    service_worker_worker_client_registry_->RegisterWorkerClient(
        std::move(service_worker_client_ptr));
    service_worker_worker_client_registry_->CloneWorkerClientRegistry(
        mojo::MakeRequest(&service_worker_worker_client_registry_ptr_info));
  }

  blink::mojom::ServiceWorkerContainerHostPtrInfo host_ptr_info;
  if (service_worker_container_host_) {
    service_worker_container_host_->CloneContainerHost(
        mojo::MakeRequest(&host_ptr_info));
  }

  blink::mojom::RendererPreferenceWatcherPtr preference_watcher;
  auto new_context = base::AdoptRef(new WebWorkerFetchContextImpl(
      renderer_preferences_, mojo::MakeRequest(&preference_watcher),
      std::move(service_worker_client_request),
      std::move(service_worker_worker_client_registry_ptr_info),
      std::move(host_ptr_info), loader_factory_->Clone(),
      fallback_factory_->Clone(),
      throttle_provider_ ? throttle_provider_->Clone() : nullptr,
      websocket_handshake_throttle_provider_
          ? websocket_handshake_throttle_provider_->Clone(
                std::move(task_runner))
          : nullptr,
      thread_safe_sender_.get(), service_manager_connection_->Clone()));
  new_context->is_controlled_by_service_worker_ =
      is_controlled_by_service_worker_;
  new_context->is_on_sub_frame_ = is_on_sub_frame_;
  new_context->ancestor_frame_id_ = ancestor_frame_id_;
  new_context->frame_request_blocker_ = frame_request_blocker_;
  new_context->appcache_host_id_ = appcache_host_id_;
  new_context->top_frame_origin_ = top_frame_origin_;

  child_preference_watchers_.AddPtr(std::move(preference_watcher));

  return new_context;
}

void WebWorkerFetchContextImpl::InitializeOnWorkerThread(
    blink::AcceptLanguagesWatcher* watcher) {
  DCHECK(!resource_dispatcher_);
  DCHECK(!binding_.is_bound());
  DCHECK(!preference_watcher_binding_.is_bound());
  resource_dispatcher_ = std::make_unique<ResourceDispatcher>();
  resource_dispatcher_->set_terminate_sync_load_event(
      terminate_sync_load_event_);

  loader_factory_ =
      network::SharedURLLoaderFactory::Create(std::move(loader_factory_info_));
  fallback_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(fallback_factory_info_));
  if (service_worker_client_request_.is_pending())
    binding_.Bind(std::move(service_worker_client_request_));

  if (service_worker_worker_client_registry_info_) {
    service_worker_worker_client_registry_.Bind(
        std::move(service_worker_worker_client_registry_info_));
  }

  if (preference_watcher_request_.is_pending())
    preference_watcher_binding_.Bind(std::move(preference_watcher_request_));

  if (service_worker_container_host_info_) {
    service_worker_container_host_.Bind(
        std::move(service_worker_container_host_info_));
  }

  blink::mojom::BlobRegistryPtr blob_registry_ptr;
  service_manager_connection_->BindInterface(
      mojom::kBrowserServiceName, mojo::MakeRequest(&blob_registry_ptr));
  blob_registry_ =
      base::MakeRefCounted<base::RefCountedData<blink::mojom::BlobRegistryPtr>>(
          std::move(blob_registry_ptr));

  accept_languages_watcher_ = watcher;

  DCHECK(loader_factory_);
  DCHECK(!web_loader_factory_);
  web_loader_factory_ = std::make_unique<Factory>(
      resource_dispatcher_->GetWeakPtr(), loader_factory_);

  ResetServiceWorkerURLLoaderFactory();
}

blink::WebURLLoaderFactory* WebWorkerFetchContextImpl::GetURLLoaderFactory() {
  return web_loader_factory_.get();
}

std::unique_ptr<blink::WebURLLoaderFactory>
WebWorkerFetchContextImpl::WrapURLLoaderFactory(
    mojo::ScopedMessagePipeHandle url_loader_factory_handle) {
  return std::make_unique<WebURLLoaderFactoryImpl>(
      resource_dispatcher_->GetWeakPtr(),
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          network::mojom::URLLoaderFactoryPtrInfo(
              std::move(url_loader_factory_handle),
              network::mojom::URLLoaderFactory::Version_)));
}

std::unique_ptr<blink::CodeCacheLoader>
WebWorkerFetchContextImpl::CreateCodeCacheLoader() {
  return std::make_unique<CodeCacheLoaderImpl>(terminate_sync_load_event_);
}

void WebWorkerFetchContextImpl::WillSendRequest(blink::WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(blink::WebString::FromUTF8(kDoNotTrackHeader),
                               "1");
  }

  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_render_frame_id(ancestor_frame_id_);
  extra_data->set_frame_request_blocker(frame_request_blocker_);
  extra_data->set_initiated_in_secure_context(is_secure_context_);
  if (throttle_provider_) {
    extra_data->set_url_loader_throttles(throttle_provider_->CreateThrottles(
        ancestor_frame_id_, request, WebURLRequestToResourceType(request)));
  }
  if (response_override_) {
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled() ||
           blink::features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
    using RequestContextType = blink::mojom::RequestContextType;
    DCHECK(request.GetRequestContext() == RequestContextType::WORKER ||
           request.GetRequestContext() == RequestContextType::SHARED_WORKER)
        << request.GetRequestContext();
    extra_data->set_navigation_response_override(std::move(response_override_));
  }
  request.SetExtraData(std::move(extra_data));
  request.SetAppCacheHostID(appcache_host_id_);

  if (g_rewrite_url)
    request.SetUrl(g_rewrite_url(request.Url().GetString().Utf8(), false));

  if (!renderer_preferences_.enable_referrers) {
    request.SetHttpReferrer(blink::WebString(),
                            network::mojom::ReferrerPolicy::kDefault);
  }
}

blink::mojom::ControllerServiceWorkerMode
WebWorkerFetchContextImpl::IsControlledByServiceWorker() const {
  return is_controlled_by_service_worker_;
}

void WebWorkerFetchContextImpl::SetIsOnSubframe(bool is_on_sub_frame) {
  is_on_sub_frame_ = is_on_sub_frame;
}

bool WebWorkerFetchContextImpl::IsOnSubframe() const {
  return is_on_sub_frame_;
}

blink::WebURL WebWorkerFetchContextImpl::SiteForCookies() const {
  return site_for_cookies_;
}

base::Optional<blink::WebSecurityOrigin>
WebWorkerFetchContextImpl::TopFrameOrigin() const {
  // TODO(jkarlin): set_top_frame_origin is only called for dedicated workers.
  // Determine the top-frame-origin of a shared worker as well. See
  // https://crbug.com/918868.
  return top_frame_origin_;
}

void WebWorkerFetchContextImpl::DidRunContentWithCertificateErrors() {
  Send(new FrameHostMsg_DidRunContentWithCertificateErrors(ancestor_frame_id_));
}

void WebWorkerFetchContextImpl::DidDisplayContentWithCertificateErrors() {
  Send(new FrameHostMsg_DidDisplayContentWithCertificateErrors(
      ancestor_frame_id_));
}

void WebWorkerFetchContextImpl::DidRunInsecureContent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& url) {
  Send(new FrameHostMsg_DidRunInsecureContent(
      ancestor_frame_id_, GURL(origin.ToString().Utf8()), url));
}

void WebWorkerFetchContextImpl::SetSubresourceFilterBuilder(
    std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
        subresource_filter_builder) {
  subresource_filter_builder_ = std::move(subresource_filter_builder);
}

std::unique_ptr<blink::WebDocumentSubresourceFilter>
WebWorkerFetchContextImpl::TakeSubresourceFilter() {
  if (!subresource_filter_builder_)
    return nullptr;
  return std::move(subresource_filter_builder_)->Build();
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
WebWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      ancestor_frame_id_, std::move(task_runner));
}

void WebWorkerFetchContextImpl::set_is_controlled_by_service_worker(
    blink::mojom::ControllerServiceWorkerMode mode) {
  is_controlled_by_service_worker_ = mode;
}

void WebWorkerFetchContextImpl::set_ancestor_frame_id(int id) {
  ancestor_frame_id_ = id;
}

void WebWorkerFetchContextImpl::set_frame_request_blocker(
    scoped_refptr<FrameRequestBlocker> frame_request_blocker) {
  frame_request_blocker_ = frame_request_blocker;
}

void WebWorkerFetchContextImpl::set_site_for_cookies(
    const blink::WebURL& site_for_cookies) {
  site_for_cookies_ = site_for_cookies;
}

void WebWorkerFetchContextImpl::set_top_frame_origin(
    const blink::WebSecurityOrigin& top_frame_origin) {
  top_frame_origin_ = top_frame_origin;
}

void WebWorkerFetchContextImpl::set_is_secure_context(bool flag) {
  is_secure_context_ = flag;
}

void WebWorkerFetchContextImpl::set_origin_url(const GURL& origin_url) {
  origin_url_ = origin_url;
}

void WebWorkerFetchContextImpl::set_client_id(const std::string& client_id) {
  client_id_ = client_id;
}

void WebWorkerFetchContextImpl::SetResponseOverrideForMainScript(
    std::unique_ptr<NavigationResponseOverrideParameters> response_override) {
  DCHECK(blink::features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
  DCHECK(!response_override_);
  response_override_ = std::move(response_override);
}

void WebWorkerFetchContextImpl::SetApplicationCacheHostID(
    const base::UnguessableToken& id) {
  appcache_host_id_ = id;
}

void WebWorkerFetchContextImpl::OnControllerChanged(
    blink::mojom::ControllerServiceWorkerMode mode) {
  set_is_controlled_by_service_worker(mode);
  ResetServiceWorkerURLLoaderFactory();
}

bool WebWorkerFetchContextImpl::Send(IPC::Message* message) {
  return thread_safe_sender_->Send(message);
}

void WebWorkerFetchContextImpl::ResetServiceWorkerURLLoaderFactory() {
  if (!web_loader_factory_)
    return;
  if (IsControlledByServiceWorker() !=
      blink::mojom::ControllerServiceWorkerMode::kControlled) {
    web_loader_factory_->SetServiceWorkerURLLoaderFactory(nullptr);
    return;
  }
  if (!service_worker_container_host_)
    return;

  network::mojom::URLLoaderFactoryPtr service_worker_url_loader_factory;
  blink::mojom::ServiceWorkerContainerHostPtrInfo host_ptr_info;
  service_worker_container_host_->CloneContainerHost(
      mojo::MakeRequest(&host_ptr_info));
  // To avoid potential dead-lock while synchronous loading, create the
  // SubresourceLoaderFactory on a background thread.
  auto task_runner = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CreateServiceWorkerSubresourceLoaderFactory,
          std::move(host_ptr_info), client_id_, fallback_factory_->Clone(),
          mojo::MakeRequest(&service_worker_url_loader_factory), task_runner));
  web_loader_factory_->SetServiceWorkerURLLoaderFactory(
      std::move(service_worker_url_loader_factory));
}

void WebWorkerFetchContextImpl::NotifyUpdate(
    blink::mojom::RendererPreferencesPtr new_prefs) {
  if (accept_languages_watcher_ &&
      renderer_preferences_.accept_languages != new_prefs->accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = *new_prefs;
  child_preference_watchers_.ForAllPtrs(
      [&new_prefs](blink::mojom::RendererPreferenceWatcher* watcher) {
        watcher->NotifyUpdate(new_prefs.Clone());
      });
}

blink::WebString WebWorkerFetchContextImpl::GetAcceptLanguages() const {
  return blink::WebString::FromUTF8(renderer_preferences_.accept_languages);
}

}  // namespace content
