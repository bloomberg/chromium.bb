// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_

#include <string>

#include "base/macros.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/variations/variations_http_header_provider.h"

class Profile;

namespace content {
class RenderProcessHost;
}

// The RendererUpdater is responsible for updating renderers about state change.
class RendererUpdater
    : public KeyedService,
      public SigninManagerBase::Observer,
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

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

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
  SigninManagerBase* signin_manager_;
  variations::VariationsHttpHeaderProvider* variations_http_header_provider_;

  // Prefs that we sync to the renderers.
  BooleanPrefMember force_google_safesearch_;
  IntegerPrefMember force_youtube_restrict_;
  StringPrefMember allowed_domains_for_apps_;

  std::string cached_variation_ids_header_;
  std::string cached_variation_ids_header_signed_in_;

  DISALLOW_COPY_AND_ASSIGN(RendererUpdater);
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
