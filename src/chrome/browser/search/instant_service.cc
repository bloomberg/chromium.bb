// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/chrome_colors/chrome_colors_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/ntp_icon_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/search.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/ntp_tiles/constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

const char kNtpCustomBackgroundURL[] = "background_url";
const char kNtpCustomBackgroundAttributionLine1[] = "attribution_line_1";
const char kNtpCustomBackgroundAttributionLine2[] = "attribution_line_2";
const char kNtpCustomBackgroundAttributionActionURL[] =
    "attribution_action_url";
const char kNtpCustomBackgroundCollectionId[] = "collection_id";
const char kNtpCustomBackgroundResumeToken[] = "resume_token";
const char kNtpCustomBackgroundRefreshTimestamp[] = "refresh_timestamp";

const char kCustomBackgroundsUmaClientName[] = "NtpCustomBackgrounds";

base::DictionaryValue GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const base::Optional<std::string>& collection_id,
    const base::Optional<std::string>& resume_token,
    const base::Optional<int> refresh_timestamp) {
  base::DictionaryValue background_info;
  background_info.SetKey(kNtpCustomBackgroundURL,
                         base::Value(background_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                         base::Value(attribution_line_1));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                         base::Value(attribution_line_2));
  background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                         base::Value(action_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundCollectionId,
                         base::Value(collection_id.value_or("")));
  background_info.SetKey(kNtpCustomBackgroundResumeToken,
                         base::Value(resume_token.value_or("")));
  background_info.SetKey(kNtpCustomBackgroundRefreshTimestamp,
                         base::Value(refresh_timestamp.value_or(0)));

  return background_info;
}

// |GetBackgroundInfoWithColor| has to return new object so that updated version
// gets synced.
base::DictionaryValue GetBackgroundInfoWithColor(
    const base::DictionaryValue* background_info,
    const SkColor color) {
  base::DictionaryValue new_background_info;
  auto url = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundURL));
  auto attribution_line_1 = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundAttributionLine1));
  auto attribution_line_2 = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundAttributionLine2));
  auto action_url = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundAttributionActionURL));
  auto collection_id = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundCollectionId));
  auto resume_token = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundResumeToken));
  auto refresh_timestamp = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundRefreshTimestamp));

  new_background_info.SetKey(kNtpCustomBackgroundURL, url.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                             attribution_line_1.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                             attribution_line_2.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                             action_url.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundMainColor,
                             base::Value((int)color));
  new_background_info.SetKey(kNtpCustomBackgroundCollectionId,
                             collection_id.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundResumeToken,
                             resume_token.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundRefreshTimestamp,
                             refresh_timestamp.Clone());
  return new_background_info;
}

base::Value NtpCustomBackgroundDefaults() {
  base::Value defaults(base::Value::Type::DICTIONARY);
  defaults.SetKey(kNtpCustomBackgroundURL,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionLine1,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionLine2,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionActionURL,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundCollectionId,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundResumeToken,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundRefreshTimestamp,
                  base::Value(base::Value::Type::INTEGER));
  return defaults;
}

void CopyFileToProfilePath(const base::FilePath& from_path,
                           const base::FilePath& profile_path) {
  base::CopyFile(from_path,
                 profile_path.AppendASCII(
                     chrome::kChromeSearchLocalNtpBackgroundFilename));
}

// |GetBitmapMainColor| just wraps |CalculateKMeanColorOfBitmap|.
// As |CalculateKMeanColorOfBitmap| is overloaded, it cannot be bind for async
// call.
SkColor GetBitmapMainColor(const SkBitmap& bitmap) {
  return color_utils::CalculateKMeanColorOfBitmap(bitmap);
}

}  // namespace

const char kNtpCustomBackgroundMainColor[] = "background_main_color";

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      most_visited_info_(std::make_unique<InstantMostVisitedInfo>()),
      pref_service_(profile_->GetPrefs()),
      native_theme_(ui::NativeTheme::GetInstanceForNativeUi()),
      background_updated_timestamp_(base::TimeTicks::Now()),
      clock_(base::DefaultClock::GetInstance()) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile_);
  if (most_visited_sites_) {
    // Determine if we are using a third-party NTP. Custom links should only be
    // enabled for the default NTP.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    if (template_url_service) {
      search_provider_observer_ = std::make_unique<SearchProviderObserver>(
          template_url_service,
          base::BindRepeating(&InstantService::OnSearchProviderChanged,
                              weak_ptr_factory_.GetWeakPtr()));
    }

    // 9 tiles are required for the custom links feature in order to balance the
    // Most Visited rows (this is due to an additional "Add" button).
    most_visited_sites_->SetMostVisitedURLsObserver(
        this, ntp_tiles::kMaxNumMostVisited);
    most_visited_sites_->EnableCustomLinks(IsCustomLinksEnabled());
  }

  most_visited_info_->use_most_visited = !IsCustomLinksEnabled();
  most_visited_info_->is_visible =
      pref_service_->GetBoolean(prefs::kNtpShortcutsVisible);

  background_service_ = NtpBackgroundServiceFactory::GetForProfile(profile_);

  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));

  // Set up the data sources that Instant uses on the NTP.
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<LocalNtpSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<NtpIconSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFaviconLegacy));
  content::URLDataSource::Add(profile_,
                              std::make_unique<MostVisitedIframeSource>());

  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNtpCustomBackgroundDict,
      base::BindRepeating(&InstantService::UpdateBackgroundFromSync,
                          weak_ptr_factory_.GetWeakPtr()));

  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess());

  theme_observer_.Add(native_theme_);

  if (background_service_)
    background_service_observer_.Add(background_service_);
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::AddObserver(InstantServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void InstantService::RemoveObserver(InstantServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstantService::OnNewTabPageOpened() {
  if (most_visited_sites_) {
    most_visited_sites_->Refresh();
  }
}

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, true);
  }
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, false);
  }
}

void InstantService::UndoAllMostVisitedDeletions() {
  if (most_visited_sites_) {
    most_visited_sites_->ClearBlacklistedUrls();
  }
}

bool InstantService::AddCustomLink(const GURL& url, const std::string& title) {
  return most_visited_sites_ &&
         most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
}

bool InstantService::UpdateCustomLink(const GURL& url,
                                      const GURL& new_url,
                                      const std::string& new_title) {
  return most_visited_sites_ && most_visited_sites_->UpdateCustomLink(
                                    url, new_url, base::UTF8ToUTF16(new_title));
}

bool InstantService::ReorderCustomLink(const GURL& url, int new_pos) {
  return most_visited_sites_ &&
         most_visited_sites_->ReorderCustomLink(url, new_pos);
}

bool InstantService::DeleteCustomLink(const GURL& url) {
  return most_visited_sites_ && most_visited_sites_->DeleteCustomLink(url);
}

bool InstantService::UndoCustomLinkAction() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  most_visited_sites_->UndoCustomLinkAction();
  return true;
}

bool InstantService::ResetCustomLinks() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  most_visited_sites_->UninitializeCustomLinks();
  return true;
}

bool InstantService::ToggleMostVisitedOrCustomLinks() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  bool use_most_visited =
      !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
  pref_service_->SetBoolean(prefs::kNtpUseMostVisitedTiles, use_most_visited);
  most_visited_info_->use_most_visited = use_most_visited;
  bool was_initialized = most_visited_sites_->IsCustomLinksInitialized();

  // Custom links is enabled if Most Visited is disabled.
  // Note: This will eventually call |NotifyAboutMostVisitedInfo|, except in the
  // case below.
  most_visited_sites_->EnableCustomLinks(!use_most_visited);

  // If custom links is enabled but not initialized, MostVisitedSites will not
  // notify |OnURLsAvailable| and |NotifyAboutMostVisitedInfo| will not be
  // called.
  //
  // This is because custom links are considered Most Visited items before
  // initialization. As such their NTPTile metadata is the same, and observers
  // are not notified if the list of NTPTiles was not changed.
  //
  // Therefore, we need to manually call |NotifyAboutMostVisitedInfo| if the
  // user has never customized their shortcuts.
  //
  // For more details, see custom_links_mananger.h and most_visited_sites.h.
  if (!was_initialized && !most_visited_sites_->IsCustomLinksInitialized()) {
    NotifyAboutMostVisitedInfo();
  }

  return true;
}

bool InstantService::ToggleShortcutsVisibility(bool do_notify) {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  bool is_visible = !pref_service_->GetBoolean(prefs::kNtpShortcutsVisible);
  pref_service_->SetBoolean(prefs::kNtpShortcutsVisible, is_visible);
  most_visited_info_->is_visible = is_visible;

  if (do_notify) {
    NotifyAboutMostVisitedInfo();
  }
  return true;
}

void InstantService::UpdateNtpTheme() {
  ApplyOrResetCustomBackgroundNtpTheme();
  SetNtpElementsNtpTheme();

  GetInitializedNtpTheme()->custom_background_disabled_by_policy =
      IsCustomBackgroundDisabledByPolicy();

  NotifyAboutNtpTheme();
}

void InstantService::UpdateBackgroundFromSync() {
  // Any incoming change to synced background data should clear the local image.
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();
  UpdateNtpTheme();
}

void InstantService::UpdateMostVisitedInfo() {
  NotifyAboutMostVisitedInfo();
}

void InstantService::SendNewTabPageURLToRenderer(
    content::RenderProcessHost* rph) {
  if (auto* channel = rph->GetChannel()) {
    mojo::AssociatedRemote<search::mojom::SearchBouncer> client;
    channel->GetRemoteAssociatedInterface(&client);
    client->SetNewTabPageURL(search::GetNewTabPageURL(profile_));
  }
}

void InstantService::ResetCustomBackgroundInfo() {
  SetCustomBackgroundInfo(GURL(), std::string(), std::string(), GURL(),
                          std::string());
}

void InstantService::SetCustomBackgroundInfo(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }
  bool is_backdrop_collection =
      background_service_ &&
      background_service_->IsValidBackdropCollection(collection_id);
  bool is_backdrop_url =
      background_service_ &&
      background_service_->IsValidBackdropUrl(background_url);

  bool need_forced_refresh =
      pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) &&
      pref_service_->FindPreference(prefs::kNtpCustomBackgroundDict)
          ->IsDefaultValue();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();

  background_updated_timestamp_ = base::TimeTicks::Now();

  if (!collection_id.empty() && is_backdrop_collection) {
    background_service_->FetchNextCollectionImage(collection_id, base::nullopt);
  } else if (background_url.is_valid() && is_backdrop_url) {
    const GURL& thumbnail_url =
        background_service_->GetThumbnailUrl(background_url);
    FetchCustomBackground(
        background_updated_timestamp_,
        thumbnail_url.is_valid() ? thumbnail_url : background_url);

    base::DictionaryValue background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url,
        base::nullopt, base::nullopt, base::nullopt);
    pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
  } else {
    pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);

    // If this device was using a local image and did not have a non-local
    // background saved, UpdateBackgroundFromSync will not fire. Therefore, we
    // need to force a refresh here.
    if (need_forced_refresh) {
      UpdateNtpTheme();
    }
  }
}

void InstantService::SetBackgroundToLocalResource() {
  background_updated_timestamp_ = base::TimeTicks::Now();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  UpdateNtpTheme();
}

void InstantService::SelectLocalBackgroundImage(const base::FilePath& path) {
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CopyFileToProfilePath, path, profile_->GetPath()),
      base::BindOnce(&InstantService::SetBackgroundToLocalResource,
                     weak_ptr_factory_.GetWeakPtr()));
}

NtpTheme* InstantService::GetInitializedNtpTheme() {
  RefreshBackgroundIfNeeded();

  if (!theme_)
    BuildNtpTheme();
  return theme_.get();
}

void InstantService::SetNativeThemeForTesting(ui::NativeTheme* theme) {
  theme_observer_.RemoveAll();
  native_theme_ = theme;
  theme_observer_.Add(native_theme_);
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (most_visited_sites_) {
    most_visited_sites_.reset();
  }
}

void InstantService::OnNextCollectionImageAvailable() {
  auto image = background_service_->next_image();
  std::string attribution1;
  std::string attribution2;
  if (image.attribution.size() > 0)
    attribution1 = image.attribution[0];
  if (image.attribution.size() > 1)
    attribution2 = image.attribution[1];

  std::string resume_token = background_service_->next_image_resume_token();
  int64_t timestamp = (clock_->Now() + base::TimeDelta::FromDays(1)).ToTimeT();

  base::DictionaryValue background_info = GetBackgroundInfoAsDict(
      image.image_url, attribution1, attribution2, image.attribution_action_url,
      image.collection_id, resume_token, timestamp);

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
}

void InstantService::OnNtpBackgroundServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  background_service_observer_.RemoveAll();
  background_service_ = nullptr;
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(rph->GetBrowserContext());
      if (profile_ == renderer_profile)
        SendNewTabPageURLToRenderer(rph);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(rph->GetBrowserContext());
      if (profile_ == renderer_profile)
        OnRendererProcessTerminated(rph->GetID());
      break;
    }
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      theme_ = nullptr;
      UpdateNtpTheme();
      break;
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::OnRendererProcessTerminated(int process_id) {
  process_ids_.erase(process_id);
}

void InstantService::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK_EQ(observed_theme, native_theme_);
  // Force the theme information to rebuild so the correct using_dark_colors
  // value is sent to the renderer.
  BuildNtpTheme();
  UpdateNtpTheme();
}

void InstantService::OnSearchProviderChanged() {
  DCHECK(most_visited_sites_);
  most_visited_sites_->EnableCustomLinks(IsCustomLinksEnabled());
}

void InstantService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  most_visited_info_->items.clear();
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    InstantMostVisitedItem item;
    item.url = tile.url;
    item.title = tile.title;
    item.favicon = tile.favicon_url;
    item.source = tile.source;
    item.title_source = tile.title_source;
    item.data_generation_time = tile.data_generation_time;
    most_visited_info_->items.push_back(item);
  }
  most_visited_info_->items_are_custom_links =
      (most_visited_sites_ && most_visited_sites_->IsCustomLinksInitialized());

  NotifyAboutMostVisitedInfo();
}

void InstantService::OnIconMadeAvailable(const GURL& site_url) {}

void InstantService::NotifyAboutMostVisitedInfo() {
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedInfoChanged(*most_visited_info_);
}

void InstantService::NotifyAboutNtpTheme() {
  for (InstantServiceObserver& observer : observers_)
    observer.NtpThemeChanged(*theme_);
}

bool InstantService::IsCustomLinksEnabled() {
  return search_provider_observer_ && search_provider_observer_->is_google() &&
         !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
}

void InstantService::BuildNtpTheme() {
  // Get theme information from theme service.
  theme_ = std::make_unique<NtpTheme>();

  // Get if the current theme is the default theme.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_->using_default_theme = theme_service->UsingDefaultTheme();

  theme_->using_dark_colors = native_theme_->ShouldUseDarkColors();

  // Get theme colors.
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);

  // Set colors.
  theme_->background_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  theme_->text_color_light =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT_LIGHT);
  SetNtpElementsNtpTheme();

  if (base::FeatureList::IsEnabled(ntp_features::kRealboxMatchOmniboxTheme)) {
    theme_->search_box.bg =
        GetOmniboxColor(&theme_provider, OmniboxPart::LOCATION_BAR_BACKGROUND);
    theme_->search_box.icon =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_ICON);
    theme_->search_box.icon_selected = GetOmniboxColor(
        &theme_provider, OmniboxPart::RESULTS_ICON, OmniboxPartState::SELECTED);
    theme_->search_box.placeholder =
        GetOmniboxColor(&theme_provider, OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
    theme_->search_box.results_bg =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_BACKGROUND);
    theme_->search_box.results_bg_hovered =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_BACKGROUND,
                        OmniboxPartState::HOVERED);
    theme_->search_box.results_bg_selected =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_BACKGROUND,
                        OmniboxPartState::SELECTED);
    theme_->search_box.results_dim =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_TEXT_DIMMED);
    theme_->search_box.results_dim_selected =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_TEXT_DIMMED,
                        OmniboxPartState::SELECTED);
    theme_->search_box.results_text =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_TEXT_DEFAULT);
    theme_->search_box.results_text_selected =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_TEXT_DEFAULT,
                        OmniboxPartState::SELECTED);
    theme_->search_box.results_url =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_TEXT_URL);
    theme_->search_box.results_url_selected =
        GetOmniboxColor(&theme_provider, OmniboxPart::RESULTS_TEXT_URL,
                        OmniboxPartState::SELECTED);
    theme_->search_box.text = GetOmniboxColor(
        &theme_provider, OmniboxPart::LOCATION_BAR_TEXT_DEFAULT);
  }

  if (theme_service->UsingExtensionTheme()) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions()
            .GetByID(theme_service->GetThemeID());
    if (extension) {
      theme_->theme_id = theme_service->GetThemeID();
      theme_->theme_name = extension->name();

      if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
        theme_->has_theme_image = true;

        // Set theme background image horizontal alignment.
        int alignment = theme_provider.GetDisplayProperty(
            ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
        if (alignment & ThemeProperties::ALIGN_LEFT)
          theme_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
        else if (alignment & ThemeProperties::ALIGN_RIGHT)
          theme_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
        else
          theme_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

        // Set theme background image vertical alignment.
        if (alignment & ThemeProperties::ALIGN_TOP)
          theme_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
        else if (alignment & ThemeProperties::ALIGN_BOTTOM)
          theme_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
        else
          theme_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

        // Set theme background image tiling.
        int tiling = theme_provider.GetDisplayProperty(
            ThemeProperties::NTP_BACKGROUND_TILING);
        switch (tiling) {
          case ThemeProperties::NO_REPEAT:
            theme_->image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
            break;
          case ThemeProperties::REPEAT_X:
            theme_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
            break;
          case ThemeProperties::REPEAT_Y:
            theme_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
            break;
          case ThemeProperties::REPEAT:
            theme_->image_tiling = THEME_BKGRND_IMAGE_REPEAT;
            break;
        }

        theme_->has_attribution =
            theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
      }
    }
  } else if (theme_service->UsingAutogeneratedTheme()) {
    theme_->color_picked = theme_service->GetAutogeneratedThemeColor();
    theme_->color_id =
        chrome_colors::ChromeColorsService::GetColorId(theme_->color_picked);
    theme_->color_dark =
        theme_provider.GetColor(ThemeProperties::COLOR_FRAME_ACTIVE);
    theme_->color_light =
        theme_provider.GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  }
}

void InstantService::ApplyOrResetCustomBackgroundNtpTheme() {
  // Custom backgrounds for non-Google search providers are not supported.
  if (!search::DefaultSearchProviderIsGoogle(profile_)) {
    ResetCustomBackgroundNtpTheme();
    return;
  }

  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    // Add a timestamp to the url to prevent the browser from using a cached
    // version when "Upload an image" is used multiple times.
    std::string time_string = std::to_string(base::Time::Now().ToTimeT());
    std::string local_string(chrome::kChromeSearchLocalNtpBackgroundUrl);
    GURL timestamped_url(local_string + "?ts=" + time_string);
    GetInitializedNtpTheme()->custom_background_url = timestamped_url;
    GetInitializedNtpTheme()->custom_background_attribution_line_1 =
        std::string();
    GetInitializedNtpTheme()->custom_background_attribution_line_2 =
        std::string();
    GetInitializedNtpTheme()->custom_background_attribution_action_url = GURL();
    return;
  }

  // Attempt to get custom background URL from preferences.
  GURL custom_background_url;
  if (!IsCustomBackgroundPrefValid(custom_background_url)) {
    ResetCustomBackgroundNtpTheme();
    return;
  }

  ApplyCustomBackgroundNtpTheme();
}

void InstantService::ApplyCustomBackgroundNtpTheme() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* background_info =
      pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
  GURL custom_background_url(
      background_info->FindKey(kNtpCustomBackgroundURL)->GetString());

  std::string collection_id;
  const base::Value* id_value =
      background_info->FindKey(kNtpCustomBackgroundCollectionId);
  if (id_value)
    collection_id = id_value->GetString();

  // Set custom background information in theme info (attributions are
  // optional).
  const base::Value* attribution_line_1 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine1);
  const base::Value* attribution_line_2 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine2);
  const base::Value* attribution_action_url =
      background_info->FindKey(kNtpCustomBackgroundAttributionActionURL);
  NtpTheme* theme = GetInitializedNtpTheme();
  theme->custom_background_url = custom_background_url;
  theme->collection_id = collection_id;

  if (attribution_line_1) {
    theme->custom_background_attribution_line_1 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine1)
            ->GetString();
  }
  if (attribution_line_2) {
    theme->custom_background_attribution_line_2 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine2)
            ->GetString();
  }
  if (attribution_action_url) {
    GURL action_url(
        background_info->FindKey(kNtpCustomBackgroundAttributionActionURL)
            ->GetString());

    if (!action_url.SchemeIsCryptographic()) {
      theme->custom_background_attribution_action_url = GURL();
    } else {
      theme->custom_background_attribution_action_url = action_url;
    }
  }
}

void InstantService::ResetCustomBackgroundNtpTheme() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();
  FallbackToDefaultNtpTheme();
}

void InstantService::FallbackToDefaultNtpTheme() {
  NtpTheme* theme = GetInitializedNtpTheme();
  theme->custom_background_url = GURL();
  theme->custom_background_attribution_line_1 = std::string();
  theme->custom_background_attribution_line_2 = std::string();
  theme->custom_background_attribution_action_url = GURL();
  theme->collection_id = std::string();
}

bool InstantService::IsCustomBackgroundDisabledByPolicy() {
  // |prefs::kNtpCustomBackgroundDict| is managed by policy only if
  // |policy::key::kNTPCustomBackgroundEnabled| is set to false and therefore
  // should be empty.
  bool managed =
      pref_service_->IsManagedPreference(prefs::kNtpCustomBackgroundDict);
  if (managed) {
    DCHECK(
        pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict)->empty());
  }
  return managed;
}

bool InstantService::IsCustomBackgroundSet() {
  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice))
    return true;

  GURL custom_background_url;
  if (!IsCustomBackgroundPrefValid(custom_background_url))
    return false;

  return true;
}

bool InstantService::AreShortcutsCustomized() {
  return most_visited_info_->items_are_custom_links;
}

std::pair<bool, bool> InstantService::GetCurrentShortcutSettings() {
  bool using_most_visited =
      pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
  bool is_visible = pref_service_->GetBoolean(prefs::kNtpShortcutsVisible);
  return std::make_pair(using_most_visited, is_visible);
}

void InstantService::ResetToDefault() {
  ResetCustomLinks();
  ResetCustomBackgroundNtpTheme();
  pref_service_->SetBoolean(prefs::kNtpUseMostVisitedTiles, false);
  pref_service_->SetBoolean(prefs::kNtpShortcutsVisible, true);
}

void InstantService::UpdateCustomBackgroundColorAsync(
    base::TimeTicks timestamp,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  // Calculate the bitmap color asynchronously as it is slow (1-2 seconds for
  // the thumbnail). However, prefs should be updated on the main thread.
  if (!fetched_image.IsEmpty()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&GetBitmapMainColor, *fetched_image.ToSkBitmap()),
        base::BindOnce(&InstantService::UpdateCustomBackgroundPrefsWithColor,
                       weak_ptr_factory_.GetWeakPtr(), timestamp));
  }
}

void InstantService::FetchCustomBackground(base::TimeTicks timestamp,
                                           const GURL& fetch_url) {
  DCHECK(!fetch_url.is_empty());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_custom_background",
                                          R"(
    semantics {
      sender: "Desktop Chrome background fetcher"
      description:
        "Fetch New Tab Page custom background for color calculation."
      trigger:
        "User selects new background on the New Tab Page."
      data: "The only data sent is the path to an image"
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users cannot disable this feature. The feature is enabled by "
        "default."
      policy_exception_justification: "Not implemented."
    })");

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kCustomBackgroundsUmaClientName);
  image_fetcher_->FetchImage(
      fetch_url,
      base::BindOnce(&InstantService::UpdateCustomBackgroundColorAsync,
                     weak_ptr_factory_.GetWeakPtr(), timestamp),
      std::move(params));
}

bool InstantService::IsCustomBackgroundPrefValid(GURL& custom_background_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  if (!background_info)
    return false;

  const base::Value* background_url =
      background_info->FindKey(kNtpCustomBackgroundURL);
  if (!background_url)
    return false;

  custom_background_url = GURL(background_url->GetString());
  return custom_background_url.is_valid();
}

void InstantService::RemoveLocalBackgroundImageCopy() {
  base::FilePath path = profile_->GetPath().AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(IgnoreResult(&base::DeleteFile), path, false));
}

void InstantService::AddValidBackdropUrlForTesting(const GURL& url) const {
  background_service_->AddValidBackdropUrlForTesting(url);
}

void InstantService::AddValidBackdropCollectionForTesting(
    const std::string& collection_id) const {
  background_service_->AddValidBackdropCollectionForTesting(collection_id);
}

void InstantService::SetNextCollectionImageForTesting(
    const CollectionImage& image) const {
  background_service_->SetNextCollectionImageForTesting(image);
}

// static
void InstantService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpCustomBackgroundDict, NtpCustomBackgroundDefaults(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kNtpCustomBackgroundLocalToDevice,
                                false);
  registry->RegisterBooleanPref(prefs::kNtpUseMostVisitedTiles, false);
  registry->RegisterBooleanPref(prefs::kNtpShortcutsVisible, true);
}

// static
bool InstantService::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* instant_service = InstantServiceFactory::GetForProfile(
      static_cast<Profile*>(browser_context));

  if (!instant_service)
    return false;

  // The process_id for the navigation request will be -1. If
  // so, allow this request since it's not going to another renderer.
  return render_process_id == -1 ||
         instant_service->IsInstantProcess(render_process_id);
}

void InstantService::UpdateCustomBackgroundPrefsWithColor(
    base::TimeTicks timestamp,
    SkColor color) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Update background color only if the selected background is still the same.
  const base::DictionaryValue* background_info =
      pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
  if (!background_info)
    return;

  if (timestamp == background_updated_timestamp_) {
    pref_service_->Set(prefs::kNtpCustomBackgroundDict,
                       GetBackgroundInfoWithColor(background_info, color));
  }
}

void InstantService::RefreshBackgroundIfNeeded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  int64_t refresh_timestamp = 0;
  const base::Value* timestamp_value =
      background_info->FindKey(kNtpCustomBackgroundRefreshTimestamp);
  if (timestamp_value)
    refresh_timestamp = timestamp_value->GetInt();
  if (refresh_timestamp == 0)
    return;

  if (clock_->Now().ToTimeT() > refresh_timestamp) {
    std::string collection_id =
        background_info->FindKey(kNtpCustomBackgroundCollectionId)->GetString();
    std::string resume_token =
        background_info->FindKey(kNtpCustomBackgroundResumeToken)->GetString();
    background_service_->FetchNextCollectionImage(collection_id, resume_token);
  }
}

void InstantService::SetImageFetcherForTesting(
    image_fetcher::ImageFetcher* image_fetcher) {
  image_fetcher_ = base::WrapUnique(image_fetcher);
}

void InstantService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void InstantService::SetNtpElementsNtpTheme() {
  NtpTheme* theme = GetInitializedNtpTheme();
  if (IsCustomBackgroundSet()) {
    theme->text_color = gfx::kGoogleGrey050;
    theme->logo_alternate = true;
    theme->logo_color = ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_NTP_LOGO, false);
    theme->shortcut_color = ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_NTP_SHORTCUT, false);
  } else {
    const ui::ThemeProvider& theme_provider =
        ThemeService::GetThemeProviderForProfile(profile_);
    theme->text_color =
        theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);
    theme->logo_alternate = theme_provider.GetDisplayProperty(
                                ThemeProperties::NTP_LOGO_ALTERNATE) == 1;
    theme->logo_color =
        theme_provider.GetColor(ThemeProperties::COLOR_NTP_LOGO);
    theme->shortcut_color =
        theme_provider.GetColor(ThemeProperties::COLOR_NTP_SHORTCUT);
  }
}
