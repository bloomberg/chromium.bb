// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/worker_host/dedicated_worker_host.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/net/cross_origin_embedder_policy_reporter.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/idle_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/isolation_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {

DedicatedWorkerHost::DedicatedWorkerHost(
    DedicatedWorkerServiceImpl* service,
    DedicatedWorkerId id,
    RenderProcessHost* worker_process_host,
    base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    const url::Origin& creator_origin,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host)
    : service_(service),
      id_(id),
      worker_process_host_(worker_process_host),
      scoped_process_host_observer_(this),
      creator_render_frame_host_id_(creator_render_frame_host_id),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_origin_(creator_origin),
      // TODO(https://crbug.com/1058759): Calculate the worker origin based on
      // the worker script URL.
      worker_origin_(creator_origin),
      cross_origin_embedder_policy_(cross_origin_embedder_policy),
      host_receiver_(this, std::move(host)),
      coep_reporter_(std::move(coep_reporter)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(worker_process_host_);
  DCHECK(worker_process_host_->IsInitializedAndNotDead());
  DCHECK(coep_reporter_);

  scoped_process_host_observer_.Add(worker_process_host_);

  service_->NotifyWorkerStarted(id_, worker_process_host_->GetID(),
                                ancestor_render_frame_host_id_);
}

DedicatedWorkerHost::~DedicatedWorkerHost() {
  service_->NotifyWorkerTerminating(id_, ancestor_render_frame_host_id_);
}

void DedicatedWorkerHost::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(receiver.is_valid());
  broker_receiver_.Bind(std::move(receiver));
  broker_receiver_.set_disconnect_handler(base::BindOnce(
      &DedicatedWorkerHost::OnMojoDisconnect, base::Unretained(this)));
}

void DedicatedWorkerHost::OnMojoDisconnect() {
  delete this;
}

void DedicatedWorkerHost::LifecycleStateChanged(
    blink::mojom::FrameLifecycleState state) {
  switch (state) {
    case blink::mojom::FrameLifecycleState::kFrozen:
    case blink::mojom::FrameLifecycleState::kFrozenAutoResumeMedia:
      is_frozen_ = true;
      break;
    case blink::mojom::FrameLifecycleState::kRunning:
      is_frozen_ = false;
      break;
    case blink::mojom::FrameLifecycleState::kPaused:
      // This shouldn't be reached, the render process does not send this
      // state.
      NOTREACHED();
      break;
  }
}

void DedicatedWorkerHost::RenderProcessExited(
    RenderProcessHost* render_process_host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_EQ(worker_process_host_, render_process_host);

  delete this;
}

void DedicatedWorkerHost::StartScriptLoad(
    const GURL& script_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  DCHECK(!client_);
  DCHECK(client);
  client_ = std::move(client);

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  // Get nearest ancestor render frame host in order to determine the
  // top-frame origin to use for the network isolation key.
  RenderFrameHostImpl* nearest_ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!nearest_ancestor_render_frame_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // Get a storage domain.
  SiteInstance* site_instance =
      nearest_ancestor_render_frame_host->GetSiteInstance();
  if (!site_instance) {
    client_->OnScriptLoadStartFailed();
    return;
  }
  std::string storage_domain;
  std::string partition_name;
  bool in_memory;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      storage_partition_impl->browser_context(), site_instance->GetSiteURL(),
      /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (script_url.SchemeIsBlob()) {
    if (!blob_url_token) {
      mojo::ReportBadMessage("DWH_NO_BLOB_URL_TOKEN");
      return;
    }
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            storage_partition_impl->browser_context(),
            std::move(blob_url_token));
  } else if (blob_url_token) {
    mojo::ReportBadMessage("DWH_NOT_BLOB_URL");
    return;
  }

  // If this is a nested worker, there is no creator frame.
  RenderFrameHostImpl* creator_render_frame_host = nullptr;
  if (creator_render_frame_host_id_) {
    creator_render_frame_host =
        RenderFrameHostImpl::FromID(creator_render_frame_host_id_.value());
    if (!creator_render_frame_host) {
      client_->OnScriptLoadStartFailed();
      return;
    }
  }

  // A dedicated worker depends on its nearest ancestor's appcache host.
  AppCacheHost* appcache_host = nullptr;
  const AppCacheNavigationHandle* appcache_handle =
      nearest_ancestor_render_frame_host->GetAppCacheNavigationHandle();
  if (appcache_handle) {
    appcache_host = storage_partition_impl->GetAppCacheService()->GetHost(
        appcache_handle->appcache_host_id());
  }

  // Set if the subresource loader factories support file URLs so that we can
  // recreate the factories after Network Service crashes.
  // TODO(nhiroki): Currently this flag is calculated based on the request
  // initiator origin to keep consistency with WorkerScriptFetchInitiator, but
  // probably this should be calculated based on the worker origin as the
  // factories be used for subresource loading on the worker.
  file_url_support_ = creator_origin_.scheme() == url::kFileScheme;

  service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
      storage_partition_impl->GetServiceWorkerContext(), base::DoNothing());

  WorkerScriptFetchInitiator::Start(
      worker_process_host_->GetID(), id_, SharedWorkerId(), script_url,
      creator_render_frame_host,
      nearest_ancestor_render_frame_host->ComputeSiteForCookies(),
      creator_origin_,
      nearest_ancestor_render_frame_host->GetIsolationInfoForSubresources(),
      credentials_mode, std::move(outside_fetch_client_settings_object),
      blink::mojom::ResourceType::kWorker,
      storage_partition_impl->GetServiceWorkerContext(),
      service_worker_handle_.get(),
      appcache_host ? appcache_host->GetWeakPtr() : nullptr,
      std::move(blob_url_loader_factory), nullptr, storage_partition_impl,
      storage_domain,
      base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the dedicated worker scope");
}

void DedicatedWorkerHost::DidStartScriptLoad(
    bool success,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    const GURL& final_response_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  if (!success) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // TODO(https://crbug.com/986188): Check if the main script's final response
  // URL is committable.
  service_->NotifyWorkerFinalResponseURLDetermined(id_, final_response_url);

  // TODO(cammie): Change this approach when we support shared workers
  // creating dedicated workers, as there might be no ancestor frame.
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    client_->OnScriptLoadStartFailed();
    return;
  }

  // Start observing Network Service crash when it's running out-of-process.
  if (IsOutOfProcessNetworkService()) {
    ObserveNetworkServiceCrash(static_cast<StoragePartitionImpl*>(
        worker_process_host_->GetStoragePartition()));
  }

  // Set up the default network loader factory.
  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(ancestor_render_frame_host,
                                          &bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  // Prepare the controller service worker info to pass to the renderer.
  // |object_info| can be nullptr when the service worker context or the
  // service worker version is gone during dedicated worker startup.
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
      service_worker_remote_object;
  blink::mojom::ServiceWorkerState service_worker_state;
  if (controller && controller->object_info) {
    controller->object_info->receiver =
        service_worker_remote_object.InitWithNewEndpointAndPassReceiver();
    service_worker_state = controller->object_info->state;
  }

  client_->OnScriptLoadStarted(
      service_worker_handle_->TakeProviderInfo(),
      std::move(main_script_load_params),
      std::move(subresource_loader_factories),
      subresource_loader_updater_.BindNewPipeAndPassReceiver(),
      std::move(controller));

  // |service_worker_remote_object| is an associated remote, so calls can't be
  // made on it until its receiver is sent. Now that the receiver was sent, it
  // can be used, so add it to ServiceWorkerObjectHost.
  if (service_worker_remote_object) {
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(
            &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
            controller_service_worker_object_host,
            std::move(service_worker_remote_object), service_worker_state));
  }
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
DedicatedWorkerHost::CreateNetworkFactoryForSubresources(
    RenderFrameHostImpl* ancestor_render_frame_host,
    bool* bypass_redirect_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(ancestor_render_frame_host);
  DCHECK(bypass_redirect_checks);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver =
          pending_default_factory.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  DCHECK(coep_reporter_);
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForFrame(
          ancestor_render_frame_host, worker_origin_,
          mojo::Clone(ancestor_render_frame_host
                          ->last_committed_client_security_state()),
          std::move(coep_reporter), worker_process_host_);
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      worker_process_host_->GetBrowserContext(),
      /*frame=*/nullptr, worker_process_host_->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource,
      worker_origin_, /*navigation_id=*/base::nullopt,
      &default_factory_receiver, &factory_params->header_client,
      bypass_redirect_checks, /*disable_secure_dns=*/nullptr,
      &factory_params->factory_override);

  // TODO(nhiroki): Call devtools_instrumentation::WillCreateURLLoaderFactory()
  // here.

  worker_process_host_->CreateURLLoaderFactory(
      std::move(default_factory_receiver), std::move(factory_params));

  return pending_default_factory;
}

void DedicatedWorkerHost::CreateWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  // The ancestor frame may have already been closed. In that case, the worker
  // will soon be terminated too, so abort the connection.
  if (!ancestor_render_frame_host)
    return;

  ancestor_render_frame_host->CreateWebUsbService(std::move(receiver));
}

void DedicatedWorkerHost::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    receiver.ResetWithReason(network::mojom::WebSocket::kInsufficientResources,
                             "The parent frame has already been gone.");
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebSocketConnectorImpl>(
          ancestor_render_frame_host_id_.child_id,
          ancestor_render_frame_host_id_.frame_routing_id, worker_origin_,
          ancestor_render_frame_host->GetIsolationInfoForSubresources()),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(
          worker_process_host_->GetID(), /*frame=*/nullptr, worker_origin_,
          ancestor_render_frame_host->GetIsolationInfoForSubresources()
              .network_isolation_key()),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateWakeLockService(
    mojo::PendingReceiver<blink::mojom::WakeLockService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Unconditionally disallow wake locks from workers until
  // WakeLockPermissionContext has been updated to no longer force the
  // permission to "denied" and WakeLockServiceImpl checks permissions on
  // every request.
  return;
}

void DedicatedWorkerHost::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());
  worker_process_host_->BindCacheStorage(cross_origin_embedder_policy_,
                                         std::move(coep_reporter),
                                         worker_origin_, std::move(receiver));
}

void DedicatedWorkerHost::CreateNestedDedicatedWorker(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());
  // There is no creator frame when the worker is nested.
  CreateDedicatedWorkerHostFactory(
      worker_process_host_->GetID(),
      /*creator_render_frame_host_id_=*/base::nullopt,
      ancestor_render_frame_host_id_, worker_origin_,
      cross_origin_embedder_policy_, std::move(coep_reporter),
      std::move(receiver));
}

void DedicatedWorkerHost::CreateIdleManager(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }
  if (!ancestor_render_frame_host->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kIdleDetection)) {
    mojo::ReportBadMessage("Feature policy blocks access to IdleDetection.");
    return;
  }
  static_cast<StoragePartitionImpl*>(
      ancestor_render_frame_host->GetProcess()->GetStoragePartition())
      ->GetIdleManager()
      ->CreateService(
          std::move(receiver),
          ancestor_render_frame_host->GetMainFrame()->GetLastCommittedOrigin());
}

void DedicatedWorkerHost::BindSmsReceiverReceiver(
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }

  ancestor_render_frame_host->BindSmsReceiverReceiver(std::move(receiver));
}

#if !defined(OS_ANDROID)
void DedicatedWorkerHost::BindSerialService(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host) {
    // The ancestor frame may have already been closed. In that case, the worker
    // will soon be terminated too, so abort the connection.
    return;
  }

  ancestor_render_frame_host->BindSerialService(std::move(receiver));
}
#endif

void DedicatedWorkerHost::ObserveNetworkServiceCrash(
    StoragePartitionImpl* storage_partition_impl) {
  auto params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = worker_process_host_->GetID();
  network_service_connection_error_handler_holder_.reset();
  storage_partition_impl->GetNetworkContext()->CreateURLLoaderFactory(
      network_service_connection_error_handler_holder_
          .BindNewPipeAndPassReceiver(),
      std::move(params));
  network_service_connection_error_handler_holder_.set_disconnect_handler(
      base::BindOnce(&DedicatedWorkerHost::UpdateSubresourceLoaderFactories,
                     weak_factory_.GetWeakPtr()));
}

void DedicatedWorkerHost::UpdateSubresourceLoaderFactories() {
  DCHECK(IsOutOfProcessNetworkService());
  DCHECK(subresource_loader_updater_.is_bound());
  DCHECK(network_service_connection_error_handler_holder_);
  DCHECK(!network_service_connection_error_handler_holder_.is_connected());

  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host_->GetStoragePartition());

  RenderFrameHostImpl* ancestor_render_frame_host =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id_);
  if (!ancestor_render_frame_host)
    return;

  SiteInstance* site_instance = ancestor_render_frame_host->GetSiteInstance();
  if (!site_instance)
    return;

  // Get a storage domain.
  std::string storage_domain;
  std::string partition_name;
  bool in_memory;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      storage_partition_impl->browser_context(), site_instance->GetSiteURL(),
      /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

  // Start observing Network Service crash again.
  ObserveNetworkServiceCrash(storage_partition_impl);

  // Recreate the default URLLoaderFactory. This doesn't support
  // AppCache-specific factory.
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories =
          WorkerScriptFetchInitiator::CreateFactoryBundle(
              WorkerScriptFetchInitiator::LoaderType::kSubResource,
              worker_process_host_->GetID(), storage_partition_impl,
              storage_domain, file_url_support_,
              /*filesystem_url_support=*/true);

  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(ancestor_render_frame_host,
                                          &bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  subresource_loader_updater_->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

namespace {
// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by the
// renderer over mojo via SelfOwnedReceiver. It lives on the UI thread.
class DedicatedWorkerHostFactoryImpl final
    : public blink::mojom::DedicatedWorkerHostFactory {
 public:
  DedicatedWorkerHostFactoryImpl(
      int worker_process_id,
      base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
      GlobalFrameRoutingId ancestor_render_frame_host_id,
      const url::Origin& creator_origin,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter)
      : worker_process_id_(worker_process_id),
        creator_render_frame_host_id_(creator_render_frame_host_id),
        ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
        creator_origin_(creator_origin),
        cross_origin_embedder_policy_(cross_origin_embedder_policy),
        coep_reporter_(std::move(coep_reporter)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  // blink::mojom::DedicatedWorkerHostFactory:
  void CreateWorkerHost(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver,
      base::OnceCallback<void(const network::CrossOriginEmbedderPolicy&)>
          callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
      mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
      return;
    }

    std::move(callback).Run(cross_origin_embedder_policy_);

    auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
    if (!worker_process_host ||
        !worker_process_host->IsInitializedAndNotDead()) {
      // Abort if the worker's process host is gone. This means that the calling
      // frame or worker is also either destroyed or in the process of being
      // destroyed.
      return;
    }

    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        worker_process_host->GetStoragePartition());

    // Get the dedicated worker service.
    DedicatedWorkerServiceImpl* service =
        storage_partition->GetDedicatedWorkerService();

    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter;
    coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

    auto* host = new DedicatedWorkerHost(
        service, service->GenerateNextDedicatedWorkerId(), worker_process_host,
        creator_render_frame_host_id_, ancestor_render_frame_host_id_,
        creator_origin_, cross_origin_embedder_policy_,
        std::move(coep_reporter), std::move(host_receiver));
    host->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));
  }

  // PlzDedicatedWorker:
  void CreateWorkerHostAndStartScriptLoad(
      const GURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient>
          client,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
      mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
      return;
    }

    // TODO(https://crbug.com/1058759): Compare |creator_origin_| to
    // |script_url|, and report as bad message if that fails.

    auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
    if (!worker_process_host ||
        !worker_process_host->IsInitializedAndNotDead()) {
      // Abort if the worker's process host is gone. This means that the calling
      // frame or worker is also either destroyed or in the process of being
      // destroyed.
      return;
    }

    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        worker_process_host->GetStoragePartition());

    // Get the dedicated worker service.
    DedicatedWorkerServiceImpl* service =
        storage_partition->GetDedicatedWorkerService();

    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter;
    coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

    auto* host = new DedicatedWorkerHost(
        service, service->GenerateNextDedicatedWorkerId(), worker_process_host,
        creator_render_frame_host_id_, ancestor_render_frame_host_id_,
        creator_origin_, cross_origin_embedder_policy_,
        std::move(coep_reporter), std::move(host_receiver));
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker;
    host->BindBrowserInterfaceBrokerReceiver(
        broker.InitWithNewPipeAndPassReceiver());
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> remote_client(
        std::move(client));
    remote_client->OnWorkerHostCreated(std::move(broker));
    host->StartScriptLoad(script_url, credentials_mode,
                          std::move(outside_fetch_client_settings_object),
                          std::move(blob_url_token), std::move(remote_client));
  }

 private:
  // The ID of the RenderProcessHost where the worker will live.
  const int worker_process_id_;

  // See comments on the corresponding members of DedicatedWorkerHost.
  const base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id_;
  const GlobalFrameRoutingId ancestor_render_frame_host_id_;

  const url::Origin creator_origin_;
  const network::CrossOriginEmbedderPolicy cross_origin_embedder_policy_;
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHostFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    int worker_process_id,
    base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    const url::Origin& creator_origin,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          worker_process_id, creator_render_frame_host_id,
          ancestor_render_frame_host_id, creator_origin,
          cross_origin_embedder_policy, std::move(coep_reporter)),
      std::move(receiver));
}

}  // namespace content
