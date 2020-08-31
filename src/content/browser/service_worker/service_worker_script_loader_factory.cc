// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_loader_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/debug/crash_logging.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_installed_script_loader.h"
#include "content/browser/service_worker/service_worker_new_script_loader.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_updated_script_loader.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

ServiceWorkerScriptLoaderFactory::ServiceWorkerScriptLoaderFactory(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    scoped_refptr<network::SharedURLLoaderFactory>
        loader_factory_for_new_scripts)
    : context_(context),
      provider_host_(provider_host),
      loader_factory_for_new_scripts_(
          std::move(loader_factory_for_new_scripts)) {
  DCHECK(loader_factory_for_new_scripts_ ||
         ServiceWorkerVersion::IsInstalled(
             provider_host_->running_hosted_version()->status()));
}

ServiceWorkerScriptLoaderFactory::~ServiceWorkerScriptLoaderFactory() = default;

void ServiceWorkerScriptLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!CheckIfScriptRequestIsValid(resource_request)) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  // There are four cases of how to handle the request for the script.
  // A) service worker is installed, script is installed: serve from storage
  //    (use ServceWorkerInstalledScriptLoader). Typically this case is handled
  //    by ServiceWorkerInstalledScriptsSender, but we can still get here when a
  //    new service worker starts up and becomes installed while it is running.
  // B) service worker is installed, script is not installed: return a network
  //    error. This happens when the script is newly imported after
  //    installation, which is disallowed by the spec.
  // C) service worker is not installed, script is installed: serve from
  //    storage (use ServiceWorkerInstalledScriptLoader)
  // D) service worker is not installed, script is not installed:
  //    1) If compared script info exists and specifies that the script is
  //       installed in an old service worker and content is not changed, then
  //       copy the old script into the new service worker and load it using
  //       ServiceWorkerInstalledScriptLoader.
  //    2) If compared script info exists and specifies that the script is
  //       installed in an old service worker but content has changed, then
  //       ServiceWorkerUpdatedScriptLoader::CreateAndStart() is called to
  //       resume the paused state stored in the compared script info.
  //    3) For other cases, serve from network with installing the script by
  //       using ServiceWorkerNewScriptLoader.

  // Case A and C:
  scoped_refptr<ServiceWorkerVersion> version =
      provider_host_->running_hosted_version();
  int64_t resource_id =
      version->script_cache_map()->LookupResourceId(resource_request.url);
  if (resource_id != blink::mojom::kInvalidServiceWorkerResourceId) {
    std::unique_ptr<ServiceWorkerResponseReader> response_reader =
        context_->storage()->CreateResponseReader(resource_id);
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<ServiceWorkerInstalledScriptLoader>(
            options, std::move(client), std::move(response_reader), version,
            resource_request.url),
        std::move(receiver));
    return;
  }

  // Case B:
  if (ServiceWorkerVersion::IsInstalled(version->status())) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  // Case D:
  // Compared script info is used to decide which sub case should be used.
  // If there is no compared script info, goto D.3 directly.
  const auto& compared_script_info_map = version->compared_script_info_map();
  if (!compared_script_info_map.empty()) {
    auto it = compared_script_info_map.find(resource_request.url);
    if (it != compared_script_info_map.end()) {
      switch (it->second.result) {
        case ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical:
          // Case D.1:
          context_->storage()->GetNewResourceId(base::BindOnce(
              &ServiceWorkerScriptLoaderFactory::CopyScript,
              weak_factory_.GetWeakPtr(), it->first, it->second.old_resource_id,
              base::BindOnce(
                  &ServiceWorkerScriptLoaderFactory::OnCopyScriptFinished,
                  weak_factory_.GetWeakPtr(), std::move(receiver), options,
                  resource_request, std::move(client))));
          return;
        case ServiceWorkerSingleScriptUpdateChecker::Result::kFailed:
          // Network failure is treated as D.2
        case ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent:
          // Case D.2:
          mojo::MakeSelfOwnedReceiver(
              ServiceWorkerUpdatedScriptLoader::CreateAndStart(
                  options, resource_request, std::move(client), version),
              std::move(receiver));
          return;
        case ServiceWorkerSingleScriptUpdateChecker::Result::kNotCompared:
          // This is invalid, as scripts in compared script info must have been
          // compared.
          NOTREACHED();
          return;
      }
    }
  }

  // Case D.3:
  // Assign a new resource ID for the script from network.
  context_->storage()->GetNewResourceId(base::BindOnce(
      &ServiceWorkerScriptLoaderFactory::OnResourceIdAssignedForNewScriptLoader,
      weak_factory_.GetWeakPtr(), std::move(receiver), routing_id, request_id,
      options, resource_request, std::move(client), traffic_annotation));
}

void ServiceWorkerScriptLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerScriptLoaderFactory::Update(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  loader_factory_for_new_scripts_ = std::move(loader_factory);
}

bool ServiceWorkerScriptLoaderFactory::CheckIfScriptRequestIsValid(
    const network::ResourceRequest& resource_request) {
  if (!context_ || !provider_host_)
    return false;

  scoped_refptr<ServiceWorkerVersion> version =
      provider_host_->running_hosted_version();
  if (!version)
    return false;

  // Handle only the service worker main script
  // (network::mojom::RequestDestination::kServiceWorker) or importScripts()
  // (network::mojom::RequestDestination::kScript).
  if (resource_request.destination !=
          network::mojom::RequestDestination::kServiceWorker &&
      resource_request.destination !=
          network::mojom::RequestDestination::kScript) {
    static auto* key = base::debug::AllocateCrashKeyString(
        "swslf_bad_type", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        key, network::RequestDestinationToString(resource_request.destination));
    mojo::ReportBadMessage("SWSLF_BAD_REQUEST_DESTINATION");
    return false;
  }

  if (version->status() == ServiceWorkerVersion::REDUNDANT) {
    // This could happen if browser-side has set the status to redundant but
    // the worker has not yet stopped. The worker is already doomed so just
    // reject the request. Handle it specially here because otherwise it'd
    // be unclear whether "REDUNDANT" should count as installed or not
    // installed when making decisions about how to handle the request and
    // logging UMA.
    return false;
  }

  // TODO(falken): Make sure we don't handle a redirected request.

  return true;
}

void ServiceWorkerScriptLoaderFactory::CopyScript(
    const GURL& url,
    int64_t resource_id,
    base::OnceCallback<void(int64_t, net::Error)> callback,
    int64_t new_resource_id) {
  ServiceWorkerStorage* storage = context_->storage();

  cache_writer_ = ServiceWorkerCacheWriter::CreateForCopy(
      storage->CreateResponseReader(resource_id),
      storage->CreateResponseWriter(new_resource_id));

  scoped_refptr<ServiceWorkerVersion> version =
      provider_host_->running_hosted_version();
  version->script_cache_map()->NotifyStartedCaching(url, new_resource_id);

  net::Error error = cache_writer_->StartCopy(
      base::BindOnce(std::move(callback), new_resource_id));

  // Run the callback directly if the operation completed or failed
  // synchronously.
  if (net::ERR_IO_PENDING != error) {
    std::move(callback).Run(new_resource_id, error);
  }
}

void ServiceWorkerScriptLoaderFactory::OnCopyScriptFinished(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    int64_t new_resource_id,
    net::Error error) {
  int64_t resource_size = cache_writer_->bytes_written();
  cache_writer_.reset();
  scoped_refptr<ServiceWorkerVersion> version =
      provider_host_->running_hosted_version();

  if (error != net::OK) {
    version->script_cache_map()->NotifyFinishedCaching(
        resource_request.url, resource_size, error,
        ServiceWorkerConsts::kServiceWorkerCopyScriptError);

    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(error));
    return;
  }

  // The copy operation is successful, add the newly copied resource record to
  // the script cache map to identify that the script is installed.
  version->script_cache_map()->NotifyFinishedCaching(
      resource_request.url, resource_size, net::OK, std::string());

  // Use ServiceWorkerInstalledScriptLoader to load the new copy.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ServiceWorkerInstalledScriptLoader>(
          options, std::move(client),
          context_->storage()->CreateResponseReader(new_resource_id), version,
          resource_request.url),
      std::move(receiver));
}

void ServiceWorkerScriptLoaderFactory::OnResourceIdAssignedForNewScriptLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    int64_t resource_id) {
  if (resource_id == blink::mojom::kInvalidServiceWorkerResourceId) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  mojo::MakeSelfOwnedReceiver(
      ServiceWorkerNewScriptLoader::CreateAndStart(
          routing_id, request_id, options, resource_request, std::move(client),
          provider_host_->running_hosted_version(),
          loader_factory_for_new_scripts_, traffic_annotation, resource_id),
      std::move(receiver));
}

}  // namespace content
