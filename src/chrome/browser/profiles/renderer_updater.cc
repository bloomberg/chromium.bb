// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/renderer_updater.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)

// By default, JavaScript, images and autoplay are enabled in guest content.
void GetGuestViewDefaultContentSettingRules(
    bool incognito,
    RendererContentSettingRules* rules) {
  rules->image_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), incognito));

  rules->script_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), incognito));
  rules->autoplay_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), incognito));
  rules->client_hints_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), incognito));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}  // namespace

RendererUpdater::RendererUpdater(Profile* profile) : profile_(profile) {
  signin_manager_ = SigninManagerFactory::GetForProfile(profile_);
  signin_manager_->AddObserver(this);
  variations_http_header_provider_ =
      variations::VariationsHttpHeaderProvider::GetInstance();
  variations_http_header_provider_->AddObserver(this);
  cached_variation_ids_header_ =
      variations_http_header_provider_->GetClientDataHeader(
          false /* is_signed_in */);
  cached_variation_ids_header_signed_in_ =
      variations_http_header_provider_->GetClientDataHeader(
          true /* is_signed_in */);

  PrefService* pref_service = profile->GetPrefs();
  force_google_safesearch_.Init(prefs::kForceGoogleSafeSearch, pref_service);
  force_youtube_restrict_.Init(prefs::kForceYouTubeRestrict, pref_service);
  allowed_domains_for_apps_.Init(prefs::kAllowedDomainsForApps, pref_service);

  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kForceGoogleSafeSearch,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kForceYouTubeRestrict,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAllowedDomainsForApps,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
}

RendererUpdater::~RendererUpdater() {
  DCHECK(!signin_manager_);
}

void RendererUpdater::Shutdown() {
  signin_manager_->RemoveObserver(this);
  signin_manager_ = nullptr;
  variations_http_header_provider_->RemoveObserver(this);
  variations_http_header_provider_ = nullptr;
}

void RendererUpdater::InitializeRenderer(
    content::RenderProcessHost* render_process_host) {
  auto renderer_configuration = GetRendererConfiguration(render_process_host);

  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  bool is_incognito_process = profile->IsOffTheRecord();

  renderer_configuration->SetInitialConfiguration(is_incognito_process);
  UpdateRenderer(&renderer_configuration);

  RendererContentSettingRules rules;
  if (render_process_host->IsForGuestsOnly()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    GetGuestViewDefaultContentSettingRules(is_incognito_process, &rules);
#else
    NOTREACHED();
#endif
  } else {
    content_settings::GetRendererContentSettingRules(
        HostContentSettingsMapFactory::GetForProfile(profile), &rules);
  }
  renderer_configuration->SetContentSettingRules(rules);
}

std::vector<chrome::mojom::RendererConfigurationAssociatedPtr>
RendererUpdater::GetRendererConfigurations() {
  std::vector<chrome::mojom::RendererConfigurationAssociatedPtr> rv;
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    Profile* renderer_profile =
        static_cast<Profile*>(it.GetCurrentValue()->GetBrowserContext());
    if (renderer_profile == profile_ ||
        renderer_profile->GetOriginalProfile() == profile_) {
      auto renderer_configuration =
          GetRendererConfiguration(it.GetCurrentValue());
      if (renderer_configuration)
        rv.push_back(std::move(renderer_configuration));
    }
  }
  return rv;
}

chrome::mojom::RendererConfigurationAssociatedPtr
RendererUpdater::GetRendererConfiguration(
    content::RenderProcessHost* render_process_host) {
  IPC::ChannelProxy* channel = render_process_host->GetChannel();
  if (!channel)
    return nullptr;

  chrome::mojom::RendererConfigurationAssociatedPtr renderer_configuration;
  channel->GetRemoteAssociatedInterface(&renderer_configuration);
  return renderer_configuration;
}

void RendererUpdater::GoogleSigninSucceeded(const AccountInfo& account_info) {
  UpdateAllRenderers();
}

void RendererUpdater::GoogleSignedOut(const AccountInfo& account_info) {
  UpdateAllRenderers();
}

void RendererUpdater::VariationIdsHeaderUpdated(
    const std::string& variation_ids_header,
    const std::string& variation_ids_header_signed_in) {
  cached_variation_ids_header_ = variation_ids_header;
  cached_variation_ids_header_signed_in_ = variation_ids_header_signed_in;
  UpdateAllRenderers();
}

void RendererUpdater::UpdateAllRenderers() {
  auto renderer_configurations = GetRendererConfigurations();
  for (auto& renderer_configuration : renderer_configurations)
    UpdateRenderer(&renderer_configuration);
}

void RendererUpdater::UpdateRenderer(
    chrome::mojom::RendererConfigurationAssociatedPtr* renderer_configuration) {
  bool is_signed_in = signin_manager_->IsAuthenticated();
  (*renderer_configuration)
      ->SetConfiguration(force_google_safesearch_.GetValue(),
                         force_youtube_restrict_.GetValue(),
                         allowed_domains_for_apps_.GetValue(),
                         is_signed_in ? cached_variation_ids_header_signed_in_
                                      : cached_variation_ids_header_);
}
