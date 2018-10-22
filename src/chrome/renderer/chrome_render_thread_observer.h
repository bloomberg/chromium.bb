// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"

namespace content {
class ResourceDispatcherDelegate;
}

namespace visitedlink {
class VisitedLinkSlave;
}

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderThreadObserver : public content::RenderThreadObserver,
                                   public chrome::mojom::RendererConfiguration {
 public:
  ChromeRenderThreadObserver();
  ~ChromeRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }
  static bool force_safe_search() { return force_safe_search_; }
  static int32_t youtube_restrict() { return youtube_restrict_; }
  static const std::string& allowed_domains_for_apps() {
    return *allowed_domains_for_apps_;
  }
  static const std::string& variation_ids_header() {
    return *variation_ids_header_;
  }

  // Returns a pointer to the content setting rules owned by
  // |ChromeRenderThreadObserver|.
  const RendererContentSettingRules* content_setting_rules() const;

  visitedlink::VisitedLinkSlave* visited_link_slave() {
    return visited_link_slave_.get();
  }

 private:
  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // chrome::mojom::RendererConfiguration:
  void SetInitialConfiguration(bool is_incognito_process) override;
  void SetConfiguration(bool force_safe_search,
                        int32_t youtube_restrict,
                        const std::string& allowed_domains_for_apps,
                        const std::string& variation_ids_header) override;
  void SetContentSettingRules(
      const RendererContentSettingRules& rules) override;
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;

  void OnRendererConfigurationAssociatedRequest(
      chrome::mojom::RendererConfigurationAssociatedRequest request);

  static bool is_incognito_process_;
  static bool force_safe_search_;
  static int32_t youtube_restrict_;
  static std::string* allowed_domains_for_apps_;
  static std::string* variation_ids_header_;
  std::unique_ptr<content::ResourceDispatcherDelegate> resource_delegate_;
  RendererContentSettingRules content_setting_rules_;

  std::unique_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;

  mojo::AssociatedBindingSet<chrome::mojom::RendererConfiguration>
      renderer_configuration_bindings_;

  base::WeakPtrFactory<ChromeRenderThreadObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderThreadObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
