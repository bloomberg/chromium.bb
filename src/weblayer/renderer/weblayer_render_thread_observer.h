// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_WEBLAYER_RENDER_THREAD_OBSERVER_H_
#define WEBLAYER_RENDERER_WEBLAYER_RENDER_THREAD_OBSERVER_H_

#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "weblayer/common/renderer_configuration.mojom.h"

namespace weblayer {

// Listens for WebLayer-specific messages from the browser.
class WebLayerRenderThreadObserver : public content::RenderThreadObserver,
                                     public mojom::RendererConfiguration {
 public:
  WebLayerRenderThreadObserver();
  ~WebLayerRenderThreadObserver() override;

  const RendererContentSettingRules* content_setting_rules() const {
    return &content_setting_rules_;
  }

 private:
  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // weblayer::mojom::RendererConfiguration:
  void SetContentSettingRules(
      const RendererContentSettingRules& rules) override;

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::RendererConfiguration> receiver);

  RendererContentSettingRules content_setting_rules_;

  mojo::AssociatedReceiverSet<mojom::RendererConfiguration>
      renderer_configuration_receivers_;
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_WEBLAYER_RENDER_THREAD_OBSERVER_H_
