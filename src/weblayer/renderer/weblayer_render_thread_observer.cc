// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/weblayer_render_thread_observer.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace weblayer {

WebLayerRenderThreadObserver::WebLayerRenderThreadObserver() = default;

WebLayerRenderThreadObserver::~WebLayerRenderThreadObserver() = default;

void WebLayerRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::BindRepeating(
      &WebLayerRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void WebLayerRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::RendererConfiguration::Name_);
}

// weblayer::mojom::RendererConfiguration:
void WebLayerRenderThreadObserver::SetContentSettingRules(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = rules;
}

void WebLayerRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::RendererConfiguration> receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}

}  // namespace weblayer
