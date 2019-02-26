// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/variations/variations_http_header_provider.h"
#include "services/identity/public/cpp/identity_manager.h"

class Profile;

namespace content {
class RenderProcessHost;
}

// The RendererUpdater is responsible for updating renderers about state change.
class RendererUpdater
    : public KeyedService,
      public identity::IdentityManager::Observer,
      public variations::VariationsHttpHeaderProvider::Observer {
 public:
  explicit RendererUpdater(Profile* profile);
  ~RendererUpdater() override;

  // KeyedService:
  void Shutdown() override;

  // Initialize a newly-started renderer process.
  void InitializeRenderer(content::RenderProcessHost* render_process_host);

 private:
  std::vector<chrome::mojom::RendererConfigurationAssociatedPtr>
  GetRendererConfigurations();

  chrome::mojom::RendererConfigurationAssociatedPtr GetRendererConfiguration(
      content::RenderProcessHost* render_process_host);

  // IdentityManager::Observer:
  void OnPrimaryAccountSet(const AccountInfo& account_info) override;
  void OnPrimaryAccountCleared(const AccountInfo& account_info) override;

  // VariationsHttpHeaderProvider::Observer:
  void VariationIdsHeaderUpdated(
      const std::string& variation_ids_header,
      const std::string& variation_ids_header_signed_in) override;

  // Update all renderers due to a configuration change.
  void UpdateAllRenderers();

  // Update the given renderer due to a configuration change.
  void UpdateRenderer(chrome::mojom::RendererConfigurationAssociatedPtr*
                          renderer_configuration);

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;
  variations::VariationsHttpHeaderProvider* variations_http_header_provider_;

  // Prefs that we sync to the renderers.
  BooleanPrefMember force_google_safesearch_;
  IntegerPrefMember force_youtube_restrict_;
  StringPrefMember allowed_domains_for_apps_;

  std::string cached_variation_ids_header_;
  std::string cached_variation_ids_header_signed_in_;

  ScopedObserver<identity::IdentityManager, identity::IdentityManager::Observer>
      identity_manager_observer_;
  identity::IdentityManager* identity_manager_;

  DISALLOW_COPY_AND_ASSIGN(RendererUpdater);
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
