// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_network_context.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/network_service.h"

namespace chromeos {

WilcoDtcSupportdNetworkContextImpl::WilcoDtcSupportdNetworkContextImpl()
    : proxy_config_monitor_(g_browser_process->local_state()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

WilcoDtcSupportdNetworkContextImpl::~WilcoDtcSupportdNetworkContextImpl() =
    default;

network::mojom::URLLoaderFactory*
WilcoDtcSupportdNetworkContextImpl::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
    EnsureNetworkContextExists();

    network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    url_loader_factory_params->is_trusted = true;
    url_loader_factory_.reset();
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
  }
  return url_loader_factory_.get();
}

void WilcoDtcSupportdNetworkContextImpl::FlushForTesting() {
  if (network_context_) {
    network_context_.FlushForTesting();
  }
  if (url_loader_factory_) {
    url_loader_factory_.FlushForTesting();
  }
}

void WilcoDtcSupportdNetworkContextImpl::EnsureNetworkContextExists() {
  if (network_context_ && network_context_.is_connected()) {
    return;
  }
  CreateNetworkContext();
}

void WilcoDtcSupportdNetworkContextImpl::CreateNetworkContext() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  network_context_params->context_name = "wilco_dtc";
  network_context_params->http_cache_enabled = false;

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  network_context_.reset();
  content::GetNetworkService()->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(),
      std::move(network_context_params));
}

}  // namespace chromeos
