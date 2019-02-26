// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/shared_worker_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "content/common/url_loader_factory_bundle.h"
#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// static
void SharedWorkerFactoryImpl::Create(
    mojom::SharedWorkerFactoryRequest request) {
  mojo::MakeStrongBinding<mojom::SharedWorkerFactory>(
      base::WrapUnique(new SharedWorkerFactoryImpl()), std::move(request));
}

SharedWorkerFactoryImpl::SharedWorkerFactoryImpl() {}

void SharedWorkerFactoryImpl::CreateSharedWorker(
    mojom::SharedWorkerInfoPtr info,
    bool pause_on_start,
    const base::UnguessableToken& devtools_worker_token,
    const RendererPreferences& renderer_preferences,
    mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings,
    mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
        service_worker_provider_info,
    int appcache_host_id,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        main_script_loader_factory,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    mojom::ControllerServiceWorkerInfoPtr controller_info,
    mojom::SharedWorkerHostPtr host,
    mojom::SharedWorkerRequest request,
    service_manager::mojom::InterfaceProviderPtr interface_provider) {
  // Bound to the lifetime of the underlying blink::WebSharedWorker instance.
  new EmbeddedSharedWorkerStub(
      std::move(info), pause_on_start, devtools_worker_token,
      renderer_preferences, std::move(preference_watcher_request),
      std::move(content_settings), std::move(service_worker_provider_info),
      appcache_host_id, std::move(main_script_loader_factory),
      std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller_info),
      std::move(host), std::move(request), std::move(interface_provider));
}

}  // namespace content
