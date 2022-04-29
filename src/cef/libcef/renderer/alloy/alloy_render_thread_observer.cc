/// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/alloy/alloy_render_thread_observer.h"

#include "libcef/common/net/net_resource_provider.h"

#include "base/no_destructor.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace {

chrome::mojom::DynamicParams* GetDynamicConfigParams() {
  static base::NoDestructor<chrome::mojom::DynamicParams> dynamic_params;
  return dynamic_params.get();
}

}  // namespace

bool AlloyRenderThreadObserver::is_incognito_process_ = false;

AlloyRenderThreadObserver::AlloyRenderThreadObserver() {
  net::NetModule::SetResourceProvider(NetResourceProvider);
}

AlloyRenderThreadObserver::~AlloyRenderThreadObserver() {}

// static
const chrome::mojom::DynamicParams&
AlloyRenderThreadObserver::GetDynamicParams() {
  return *GetDynamicConfigParams();
}

void AlloyRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::BindRepeating(
      &AlloyRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void AlloyRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      chrome::mojom::RendererConfiguration::Name_);
}

void AlloyRenderThreadObserver::SetInitialConfiguration(
    bool is_incognito_process,
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener> chromeos_listener,
    mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
        content_settings_manager) {
  is_incognito_process_ = is_incognito_process;
}

void AlloyRenderThreadObserver::SetConfiguration(
    chrome::mojom::DynamicParamsPtr params) {
  *GetDynamicConfigParams() = std::move(*params);
}

void AlloyRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
        receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}
