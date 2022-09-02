// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ambient_animation_theme.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_metrics.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/backoff_entry.h"
#include "personalization_app_ambient_provider_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash {
namespace personalization_app {

namespace {

// Width and height of the preview image for personal album.
constexpr int kBannerWidthPx = 160;
constexpr int kBannerHeightPx = 160;

constexpr int kMaxRetries = 3;

constexpr char kRecentHighlightsPhotoContainerId[] = "RECENT_PHOTOS";

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    500,        // Initial delay in ms.
    2.0,        // Factor by which the waiting time will be multiplied.
    0.2,        // Fuzzing percentage.
    60 * 1000,  // Maximum delay in ms.
    -1,         // Never discard the entry.
    true,       // Use initial delay.
};

}  // namespace

PersonalizationAppAmbientProviderImpl::PersonalizationAppAmbientProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)),
      fetch_settings_retry_backoff_(&kRetryBackoffPolicy),
      update_settings_retry_backoff_(&kRetryBackoffPolicy) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ash::ambient::prefs::kAmbientModeEnabled,
      base::BindRepeating(
          &PersonalizationAppAmbientProviderImpl::OnAmbientModeEnabledChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::ambient::prefs::kAmbientAnimationTheme,
      base::BindRepeating(
          &PersonalizationAppAmbientProviderImpl::OnAnimationThemeChanged,
          base::Unretained(this)));
}

PersonalizationAppAmbientProviderImpl::
    ~PersonalizationAppAmbientProviderImpl() {
  if (page_viewed_) {
    ::ash::personalization_app::PersonalizationAppManagerFactory::
        GetForBrowserContext(profile_)
            ->MaybeStartHatsTimer(
                ::ash::personalization_app::HatsSurveyType::kScreensaver);
  }
}

void PersonalizationAppAmbientProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::AmbientProvider>
        receiver) {
  ambient_receiver_.reset();
  ambient_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppAmbientProviderImpl::IsAmbientModeEnabled(
    IsAmbientModeEnabledCallback callback) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  std::move(callback).Run(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

void PersonalizationAppAmbientProviderImpl::SetAmbientObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
        observer) {
  // May already be bound if user refreshes page.
  ambient_observer_remote_.reset();
  ambient_observer_remote_.Bind(std::move(observer));

  // Call it once to get the current ambient mode enabled status.
  OnAmbientModeEnabledChanged();

  // Call it once to get the current animation theme.
  OnAnimationThemeChanged();

  ResetLocalSettings();
  // Will notify WebUI when fetches successfully.
  FetchSettingsAndAlbums();
}

void PersonalizationAppAmbientProviderImpl::SetAmbientModeEnabled(
    bool enabled) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, enabled);
}

void PersonalizationAppAmbientProviderImpl::SetAnimationTheme(
    ash::AmbientAnimationTheme animation_theme) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  LogAmbientModeAnimationTheme(animation_theme);
  pref_service->SetInteger(ash::ambient::prefs::kAmbientAnimationTheme,
                           static_cast<int>(animation_theme));
}

void PersonalizationAppAmbientProviderImpl::SetTopicSource(
    ash::AmbientModeTopicSource topic_source) {
  // If this is an Art gallery album page, will select art gallery topic source.
  if (topic_source == ash::AmbientModeTopicSource::kArtGallery) {
    MaybeUpdateTopicSource(topic_source);
    return;
  }

  // If this is a Google Photos album page, will
  // 1. Select art gallery topic source if no albums or no album is selected.
  if (settings_->selected_album_ids.empty()) {
    MaybeUpdateTopicSource(ash::AmbientModeTopicSource::kArtGallery);
    return;
  }

  // 2. Select Google Photos topic source if at least one album is selected.
  MaybeUpdateTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
}

void PersonalizationAppAmbientProviderImpl::SetTemperatureUnit(
    ash::AmbientModeTemperatureUnit temperature_unit) {
  if (settings_->temperature_unit != temperature_unit) {
    settings_->temperature_unit = temperature_unit;
    UpdateSettings();
    OnTemperatureUnitChanged();
  }
}

void PersonalizationAppAmbientProviderImpl::SetAlbumSelected(
    const std::string& id,
    ash::AmbientModeTopicSource topic_source,
    bool selected) {
  switch (topic_source) {
    case (ash::AmbientModeTopicSource::kGooglePhotos): {
      ash::PersonalAlbum* personal_album = FindPersonalAlbumById(id);
      if (!personal_album) {
        mojo::ReportBadMessage("Invalid album id.");
        return;
      }
      personal_album->selected = selected;

      // For Google Photos, we will populate the |selected_album_ids| with IDs
      // of selected albums.
      settings_->selected_album_ids.clear();
      for (const auto& personal_album : personal_albums_.albums) {
        if (personal_album.selected) {
          std::string album_id = personal_album.album_id;

          // Convert the fake album ID back to actual ID from IMAX so we can
          // download the previews.
          if (album_id == ash::kAmbientModeRecentHighlightsAlbumId)
            album_id = kRecentHighlightsPhotoContainerId;
          settings_->selected_album_ids.emplace_back(album_id);
        }
      }

      // Update topic source based on selections.
      if (settings_->selected_album_ids.empty()) {
        settings_->topic_source = ash::AmbientModeTopicSource::kArtGallery;
      } else {
        settings_->topic_source = ash::AmbientModeTopicSource::kGooglePhotos;
      }

      ash::ambient::RecordAmbientModeTotalNumberOfAlbums(
          personal_albums_.albums.size());
      ash::ambient::RecordAmbientModeSelectedNumberOfAlbums(
          settings_->selected_album_ids.size());
      break;
    }
    case (ash::AmbientModeTopicSource::kArtGallery): {
      // For Art gallery, we set the corresponding setting to be enabled or not
      // based on the selections.
      auto* art_setting = FindArtAlbumById(id);
      if (!art_setting || !art_setting->visible) {
        mojo::ReportBadMessage("Invalid album id.");
        return;
      }
      art_setting->enabled = selected;
      break;
    }
  }

  UpdateSettings();
  OnTopicSourceChanged();
}

void PersonalizationAppAmbientProviderImpl::SetPageViewed() {
  page_viewed_ = true;
}

void PersonalizationAppAmbientProviderImpl::OnAmbientModeEnabledChanged() {
  const bool enabled = IsAmbientModeEnabled();
  if (ambient_observer_remote_.is_bound()) {
    ambient_observer_remote_->OnAmbientModeEnabledChanged(enabled);
  }

  // Call |UpdateSettings| when Ambient mode is enabled to make sure that
  // settings are properly synced to the server even if the user never touches
  // the other controls. Please see b/177456397.
  if (settings_ && enabled) {
    UpdateSettings();
  }
}

void PersonalizationAppAmbientProviderImpl::OnAnimationThemeChanged() {
  if (!ambient_observer_remote_.is_bound())
    return;

  ambient_observer_remote_->OnAnimationThemeChanged(GetCurrentAnimationTheme());
}

void PersonalizationAppAmbientProviderImpl::OnTemperatureUnitChanged() {
  if (!ambient_observer_remote_.is_bound())
    return;

  ambient_observer_remote_->OnTemperatureUnitChanged(
      settings_->temperature_unit);
}

void PersonalizationAppAmbientProviderImpl::OnTopicSourceChanged() {
  if (!ambient_observer_remote_.is_bound())
    return;

  // First, empty the WebUI store so it doesn't show the previously selected
  // albums' previews. If |settings_->topic_source| is Google photos, refetch
  // the previews because the selected albums may have changed. Otherwise, we
  // fallback to the preview urls that comes with the albums.
  OnGooglePhotosAlbumsPreviewsFetched(std::vector<GURL>());
  if (settings_->topic_source == ash::AmbientModeTopicSource::kGooglePhotos)
    FetchGooglePhotosAlbumsPreviews(settings_->selected_album_ids);

  ambient_observer_remote_->OnTopicSourceChanged(settings_->topic_source);
}

void PersonalizationAppAmbientProviderImpl::OnAlbumsChanged() {
  if (!ambient_observer_remote_.is_bound())
    return;

  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums;
  // Google photos:
  for (const auto& personal_album : personal_albums_.albums) {
    // `url` will be updated when preview image is downloaded.
    ash::personalization_app::mojom::AmbientModeAlbumPtr album =
        ash::personalization_app::mojom::AmbientModeAlbum::New();
    album->id = personal_album.album_id;
    album->checked = personal_album.selected;
    album->title = personal_album.album_name;
    album->description = personal_album.description;
    album->number_of_photos = personal_album.number_of_photos;
    album->url = GURL(personal_album.banner_image_url);
    album->topic_source = ash::AmbientModeTopicSource::kGooglePhotos;
    albums.emplace_back(std::move(album));
  }

  // Art gallery:
  for (const auto& setting : settings_->art_settings) {
    if (!setting.visible)
      continue;

    // `url` will be updated when preview image is downloaded.
    ash::personalization_app::mojom::AmbientModeAlbumPtr album =
        ash::personalization_app::mojom::AmbientModeAlbum::New();
    album->id = setting.album_id;
    album->checked = setting.enabled;
    album->title = setting.title;
    album->description = setting.description;
    album->url = GURL(setting.preview_image_url);
    album->topic_source = ash::AmbientModeTopicSource::kArtGallery;
    albums.emplace_back(std::move(album));
  }

  ambient_observer_remote_->OnAlbumsChanged(std::move(albums));
}

void PersonalizationAppAmbientProviderImpl::
    OnRecentHighlightsPreviewsChanged() {
  NOTIMPLEMENTED();
}

bool PersonalizationAppAmbientProviderImpl::IsAmbientModeEnabled() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  return pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled);
}

ash::AmbientAnimationTheme
PersonalizationAppAmbientProviderImpl::GetCurrentAnimationTheme() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  return static_cast<ash::AmbientAnimationTheme>(
      pref_service->GetInteger(ash::ambient::prefs::kAmbientAnimationTheme));
}

void PersonalizationAppAmbientProviderImpl::UpdateSettings() {
  DCHECK(IsAmbientModeEnabled())
      << "Ambient mode must be enabled to update settings";
  DCHECK(settings_);

  // Prevent fetch settings callback changing `settings_` and `personal_albums_`
  // while updating.
  read_weak_factory_.InvalidateWeakPtrs();

  if (is_updating_backend_) {
    has_pending_updates_for_backend_ = true;
    return;
  }

  has_pending_updates_for_backend_ = false;
  is_updating_backend_ = true;

  // Explicitly set show_weather to true to force server to respond with
  // weather information. See: b/158630188.
  settings_->show_weather = true;

  settings_sent_for_update_ = settings_;
  ash::AmbientBackendController::Get()->UpdateSettings(
      *settings_,
      base::BindOnce(&PersonalizationAppAmbientProviderImpl::OnUpdateSettings,
                     write_weak_factory_.GetWeakPtr()));
}

void PersonalizationAppAmbientProviderImpl::OnUpdateSettings(bool success) {
  is_updating_backend_ = false;

  if (success) {
    update_settings_retry_backoff_.Reset();
    cached_settings_ = settings_sent_for_update_;
  } else {
    update_settings_retry_backoff_.InformOfRequest(/*succeeded=*/false);
  }

  if (MaybeScheduleNewUpdateSettings(success))
    return;

  UpdateUIWithCachedSettings(success);
}

bool PersonalizationAppAmbientProviderImpl::MaybeScheduleNewUpdateSettings(
    bool success) {
  // If it was unsuccessful to update settings, but have not reached
  // `kMaxRetries`, then it will retry.
  const bool need_retry_update_settings_at_backend =
      !success && update_settings_retry_backoff_.failure_count() <= kMaxRetries;

  // If there has pending updates or need to retry, then updates settings again.
  const bool should_update_settings_at_backend =
      has_pending_updates_for_backend_ || need_retry_update_settings_at_backend;

  if (!should_update_settings_at_backend)
    return false;

  const base::TimeDelta kDelay =
      update_settings_retry_backoff_.GetTimeUntilRelease();
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PersonalizationAppAmbientProviderImpl::UpdateSettings,
                     write_weak_factory_.GetWeakPtr()),
      kDelay);
  return true;
}

void PersonalizationAppAmbientProviderImpl::UpdateUIWithCachedSettings(
    bool success) {
  // If it was unsuccessful to update settings with `kMaxRetries`, need to
  // restore to cached settings.
  const bool should_restore_previous_settings =
      !success && update_settings_retry_backoff_.failure_count() > kMaxRetries;

  // Otherwise, if there has pending fetching request or need to restore
  // cached settings, then updates the WebUi.
  const bool should_update_web_ui =
      has_pending_fetch_request_ || should_restore_previous_settings;

  if (!should_update_web_ui)
    return;

  OnSettingsAndAlbumsFetched(cached_settings_, std::move(personal_albums_));
  has_pending_fetch_request_ = false;
}

void PersonalizationAppAmbientProviderImpl::FetchSettingsAndAlbums() {
  // If there is an ongoing update, do not fetch. If update succeeds, it will
  // update the UI with the new settings. If update fails, it will restore
  // previous settings and update UI.
  if (is_updating_backend_) {
    has_pending_fetch_request_ = true;
    return;
  }

  // TODO(b/161044021): Add a helper function to get all the albums. Currently
  // only load 100 latest modified albums.
  ash::AmbientBackendController::Get()->FetchSettingsAndAlbums(
      kBannerWidthPx, kBannerHeightPx, /*num_albums=*/100,
      base::BindOnce(
          &PersonalizationAppAmbientProviderImpl::OnSettingsAndAlbumsFetched,
          read_weak_factory_.GetWeakPtr()));
}

void PersonalizationAppAmbientProviderImpl::OnSettingsAndAlbumsFetched(
    const absl::optional<ash::AmbientSettings>& settings,
    ash::PersonalAlbums personal_albums) {
  // `settings` value implies success.
  if (!settings) {
    fetch_settings_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    if (fetch_settings_retry_backoff_.failure_count() > kMaxRetries)
      return;

    const base::TimeDelta kDelay =
        fetch_settings_retry_backoff_.GetTimeUntilRelease();
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PersonalizationAppAmbientProviderImpl::FetchSettingsAndAlbums,
            read_weak_factory_.GetWeakPtr()),
        kDelay);
    return;
  }

  fetch_settings_retry_backoff_.Reset();
  settings_ = settings;
  cached_settings_ = settings;
  personal_albums_ = std::move(personal_albums);
  SyncSettingsAndAlbums();

  OnTemperatureUnitChanged();

  // Notify `OnAlbumsChanged()` first because the albums info is needed to
  // render the description text of the topic source buttons. E.g. if the Google
  // Photos album is empty, it will show different text.
  OnAlbumsChanged();
  OnTopicSourceChanged();

  // If weather info is disabled, call `UpdateSettings()` immediately to force
  // it to true. Please see b/177456397.
  if (!settings_->show_weather && IsAmbientModeEnabled()) {
    UpdateSettings();
  }
}

void PersonalizationAppAmbientProviderImpl::SyncSettingsAndAlbums() {
  // Clear the `selected` field, which will be populated with new value below.
  // It is neceessary if `UpdateSettings()` failed and we need to reset the
  // cached settings.
  for (auto& album : personal_albums_.albums) {
    album.selected = false;
  }

  auto it = settings_->selected_album_ids.begin();
  while (it != settings_->selected_album_ids.end()) {
    const std::string& album_id = *it;
    ash::PersonalAlbum* album = FindPersonalAlbumById(album_id);
    if (album) {
      album->selected = true;
      ++it;
    } else {
      // The selected album does not exist any more.
      it = settings_->selected_album_ids.erase(it);
    }
  }

  if (settings_->selected_album_ids.empty())
    MaybeUpdateTopicSource(ash::AmbientModeTopicSource::kArtGallery);
}

void PersonalizationAppAmbientProviderImpl::MaybeUpdateTopicSource(
    ash::AmbientModeTopicSource topic_source) {
  // If the setting is the same, no need to update.
  if (settings_->topic_source != topic_source) {
    settings_->topic_source = topic_source;
    UpdateSettings();
  }

  OnTopicSourceChanged();
}

void PersonalizationAppAmbientProviderImpl::FetchGooglePhotosAlbumsPreviews(
    const std::vector<std::string>& album_ids) {
  DCHECK(!album_ids.empty());
  google_photos_albums_previews_weak_factory_.InvalidateWeakPtrs();
  ash::AmbientBackendController::Get()->GetGooglePhotosAlbumsPreview(
      album_ids, kBannerWidthPx, kBannerHeightPx,
      /*num_previews=*/4,
      base::BindOnce(&PersonalizationAppAmbientProviderImpl::
                         OnGooglePhotosAlbumsPreviewsFetched,
                     google_photos_albums_previews_weak_factory_.GetWeakPtr()));
}

void PersonalizationAppAmbientProviderImpl::OnGooglePhotosAlbumsPreviewsFetched(
    const std::vector<GURL>& preview_urls) {
  DVLOG(4) << __func__ << " preview_urls_size=" << preview_urls.size();
  ambient_observer_remote_->OnGooglePhotosAlbumsPreviewsFetched(preview_urls);
}

ash::PersonalAlbum*
PersonalizationAppAmbientProviderImpl::FindPersonalAlbumById(
    const std::string& album_id) {
  auto it = std::find_if(
      personal_albums_.albums.begin(), personal_albums_.albums.end(),
      [&album_id](const auto& album) { return album.album_id == album_id; });

  if (it == personal_albums_.albums.end())
    return nullptr;

  return &(*it);
}

ash::ArtSetting* PersonalizationAppAmbientProviderImpl::FindArtAlbumById(
    const std::string& album_id) {
  auto it = std::find_if(
      settings_->art_settings.begin(), settings_->art_settings.end(),
      [&album_id](const auto& album) { return album.album_id == album_id; });
  // Album does not exist any more.
  if (it == settings_->art_settings.end())
    return nullptr;

  return &(*it);
}

void PersonalizationAppAmbientProviderImpl::ResetLocalSettings() {
  write_weak_factory_.InvalidateWeakPtrs();
  read_weak_factory_.InvalidateWeakPtrs();
  google_photos_albums_previews_weak_factory_.InvalidateWeakPtrs();

  settings_.reset();
  cached_settings_.reset();
  settings_sent_for_update_.reset();
  has_pending_fetch_request_ = false;
  is_updating_backend_ = false;
  has_pending_updates_for_backend_ = false;
}

}  // namespace personalization_app
}  // namespace ash
