// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

namespace {

bool SchemeMaySupportRedirectingToHTTPS(BrowserContext* browser_context,
                                        const GURL& url) {
  // If there is a registered protocol handler for this scheme, the embedder is
  // expected to redirect `url` to a registered URL in a URLLoaderThrottle, and
  // the interceptor will operate on the registered URL. Note that the HTML
  // specification requires that the registered URL is HTTPS.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  if (GetContentClient()->browser()->HasCustomSchemeHandler(browser_context,
                                                            url.scheme()))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return url.SchemeIs(kExternalFileScheme);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Returns true if a ServiceWorkerMainResourceLoaderInterceptor should be
// created for a worker with this |url|.
bool ShouldCreateForWorker(
    const GURL& url,
    base::WeakPtr<ServiceWorkerContainerHost> parent_container_host) {
  // Blob URL can be controlled by a parent's controller.
  if (url.SchemeIsBlob() && parent_container_host)
    return true;
  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  return url.SchemeIsHTTPOrHTTPS() || OriginCanAccessServiceWorkers(url);
}

}  // namespace

std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerMainResourceLoaderInterceptor::CreateForNavigation(
    const GURL& url,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle,
    const NavigationRequestInfo& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ShouldCreateForNavigation(
          url, request_info.common_params->request_destination,
          navigation_handle->context_wrapper()->browser_context())) {
    return nullptr;
  }

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle),
      request_info.common_params->request_destination,
      request_info.begin_params->skip_service_worker,
      request_info.are_ancestors_secure, request_info.frame_tree_node_id,
      ChildProcessHost::kInvalidUniqueID, /* worker_token = */ nullptr,
      request_info.isolation_info));
}

std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerMainResourceLoaderInterceptor::CreateForWorker(
    const network::ResourceRequest& resource_request,
    const net::IsolationInfo& isolation_info,
    int process_id,
    const DedicatedOrSharedWorkerToken& worker_token,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(resource_request.destination ==
             network::mojom::RequestDestination::kWorker ||
         resource_request.destination ==
             network::mojom::RequestDestination::kSharedWorker)
      << resource_request.destination;

  if (!ShouldCreateForWorker(resource_request.url,
                             navigation_handle->parent_container_host()))
    return nullptr;

  return base::WrapUnique(new ServiceWorkerMainResourceLoaderInterceptor(
      std::move(navigation_handle), resource_request.destination,
      resource_request.skip_service_worker, /*are_ancestors_secure=*/false,
      FrameTreeNode::kFrameTreeNodeInvalidId, process_id, &worker_token,
      isolation_info));
}

ServiceWorkerMainResourceLoaderInterceptor::
    ~ServiceWorkerMainResourceLoaderInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerMainResourceLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback loader_callback,
    FallbackCallback fallback_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);

  ServiceWorkerContextCore* context_core =
      handle_->context_wrapper()->context();
  if (!context_core || !browser_context) {
    std::move(loader_callback).Run(/*handler=*/{});
    return;
  }

  // If this is the first request before redirects, a container info and
  // container host has not yet been created.
  if (!handle_->has_container_info()) {
    // Create `container_info`.
    auto container_info =
        blink::mojom::ServiceWorkerContainerInfoForClient::New();
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver =
            container_info->host_remote.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote;

    container_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    handle_->OnCreatedContainerHost(std::move(container_info));

    // Create the ServiceWorkerContainerHost. Its lifetime is bound to
    // `container_info`.
    DCHECK(!handle_->container_host());
    base::WeakPtr<ServiceWorkerContainerHost> container_host;
    bool inherit_controller_only = false;

    if (blink::IsRequestDestinationFrame(request_destination_)) {
      container_host = context_core->CreateContainerHostForWindow(
          std::move(host_receiver), are_ancestors_secure_,
          std::move(client_remote), frame_tree_node_id_);
    } else {
      DCHECK(request_destination_ ==
                 network::mojom::RequestDestination::kWorker ||
             request_destination_ ==
                 network::mojom::RequestDestination::kSharedWorker);

      ServiceWorkerClientInfo client_info =
          ServiceWorkerClientInfo(*worker_token_);

      container_host = context_core->CreateContainerHostForWorker(
          std::move(host_receiver), process_id_, std::move(client_remote),
          client_info);

      // For the blob worker case, inherit the controller from the worker's
      // parent. See
      // https://w3c.github.io/ServiceWorker/#control-and-use-worker-client
      base::WeakPtr<ServiceWorkerContainerHost> parent_container_host =
          handle_->parent_container_host();
      if (parent_container_host &&
          tentative_resource_request.url.SchemeIsBlob()) {
        container_host->InheritControllerFrom(*parent_container_host,
                                              tentative_resource_request.url);
        inherit_controller_only = true;
      }
    }
    DCHECK(container_host);
    handle_->set_container_host(container_host);

    // For the blob worker case, we only inherit the controller and do not let
    // it intercept the main resource request. Blob URLs are not eligible to
    // go through service worker interception. So just call the loader
    // callback now.
    if (inherit_controller_only) {
      std::move(loader_callback).Run(/*handler=*/{});
      return;
    }
  }

  // Update `isolation_info_` in case a redirect has occurred.
  url::Origin new_origin = url::Origin::Create(tentative_resource_request.url);
  switch (isolation_info_.request_type()) {
    case net::IsolationInfo::RequestType::kMainFrame:
    case net::IsolationInfo::RequestType::kSubFrame:
      isolation_info_ = isolation_info_.CreateForRedirect(new_origin);
      break;
    case net::IsolationInfo::RequestType::kOther:
      net::SiteForCookies new_site_for_cookies =
          isolation_info_.site_for_cookies();
      new_site_for_cookies.CompareWithFrameTreeOriginAndRevise(new_origin);
      // This is a request for a worker. Loading cross-origin workers is not
      // allowed, so it does not really matter what we put here (if this is
      // cross-origin redirect, it will be blocked later on), but we need to
      // update the storage key anyway so that following DCHECKs pass.
      //
      // TODO(https://crbug/1147281): If we will have a custom
      // `net::IsolationInfo::RequestType` for workers, it might become possible
      // to simplify this and move the logic into
      // `net::IsolationInfo::CreateForRedirect`.
      isolation_info_ =
          net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                     isolation_info_.top_frame_origin().value(),
                                     new_origin, new_site_for_cookies);
      break;
  }

  // If we know there's no service worker for the storage key, let's skip asking
  // the storage to check the existence.
  blink::StorageKey storage_key =
      blink::StorageKey::FromNetIsolationInfo(isolation_info_);

  bool skip_service_worker =
      skip_service_worker_ ||
      !handle_->context_wrapper()->MaybeHasRegistrationForStorageKey(
          storage_key);

  // Create and start the handler for this request. It will invoke the loader
  // callback or fallback callback.
  request_handler_ = std::make_unique<ServiceWorkerControlleeRequestHandler>(
      context_core->AsWeakPtr(), handle_->container_host(),
      request_destination_, skip_service_worker, frame_tree_node_id_,
      handle_->service_worker_accessed_callback());

  request_handler_->MaybeCreateLoader(
      tentative_resource_request, storage_key, browser_context,
      std::move(loader_callback), std::move(fallback_callback));
}

absl::optional<SubresourceLoaderParams>
ServiceWorkerMainResourceLoaderInterceptor::
    MaybeCreateSubresourceLoaderParams() {
  if (!handle_) {
    return absl::nullopt;
  }
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      handle_->container_host();

  // We didn't find a matching service worker for this request, and
  // ServiceWorkerContainerHost::SetControllerRegistration() was not called.
  if (!container_host || !container_host->controller()) {
    return absl::nullopt;
  }

  // Otherwise let's send the controller service worker information along
  // with the navigation commit.
  SubresourceLoaderParams params;
  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->mode = container_host->GetControllerMode();
  // Note that |controller_info->remote_controller| is null if the controller
  // has no fetch event handler. In that case the renderer frame won't get the
  // controller pointer upon the navigation commit, and subresource loading will
  // not be intercepted. (It might get intercepted later if the controller
  // changes due to skipWaiting() so SetController is sent.)
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote =
      container_host->GetRemoteControllerServiceWorker();
  if (remote.is_bound()) {
    controller_info->remote_controller = remote.Unbind();
  }

  controller_info->client_id = container_host->client_uuid();
  if (container_host->fetch_request_window_id()) {
    controller_info->fetch_request_window_id =
        absl::make_optional(container_host->fetch_request_window_id());
  }
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      container_host->GetOrCreateServiceWorkerObjectHost(
          container_host->controller());
  if (object_host) {
    params.controller_service_worker_object_host = object_host;
    controller_info->object_info = object_host->CreateIncompleteObjectInfo();
  }
  for (const auto feature : container_host->controller()->used_features()) {
    controller_info->used_features.push_back(feature);
  }
  params.controller_service_worker_info = std::move(controller_info);
  return absl::optional<SubresourceLoaderParams>(std::move(params));
}

ServiceWorkerMainResourceLoaderInterceptor::
    ServiceWorkerMainResourceLoaderInterceptor(
        base::WeakPtr<ServiceWorkerMainResourceHandle> handle,
        network::mojom::RequestDestination request_destination,
        bool skip_service_worker,
        bool are_ancestors_secure,
        int frame_tree_node_id,
        int process_id,
        const DedicatedOrSharedWorkerToken* worker_token,
        const net::IsolationInfo& isolation_info)
    : handle_(std::move(handle)),
      request_destination_(request_destination),
      skip_service_worker_(skip_service_worker),
      isolation_info_(isolation_info),
      are_ancestors_secure_(are_ancestors_secure),
      frame_tree_node_id_(frame_tree_node_id),
      process_id_(process_id),
      worker_token_(base::OptionalFromPtr(worker_token)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);
}

// static
bool ServiceWorkerMainResourceLoaderInterceptor::ShouldCreateForNavigation(
    const GURL& url,
    network::mojom::RequestDestination request_destination,
    BrowserContext* browser_context) {
  // <embed> and <object> navigations must bypass the service worker, per the
  // discussion in https://w3c.github.io/ServiceWorker/#implementer-concerns.
  if (request_destination == network::mojom::RequestDestination::kEmbed ||
      request_destination == network::mojom::RequestDestination::kObject) {
    return false;
  }

  // Create the interceptor even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  return url.SchemeIsHTTPOrHTTPS() || OriginCanAccessServiceWorkers(url) ||
         SchemeMaySupportRedirectingToHTTPS(browser_context, url);
}

}  // namespace content
