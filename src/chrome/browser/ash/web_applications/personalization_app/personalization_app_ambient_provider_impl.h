// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class Profile;

class PersonalizationAppAmbientProviderImpl
    : public ash::PersonalizationAppAmbientProvider {
 public:
  explicit PersonalizationAppAmbientProviderImpl(content::WebUI* web_ui);

  PersonalizationAppAmbientProviderImpl(
      const PersonalizationAppAmbientProviderImpl&) = delete;
  PersonalizationAppAmbientProviderImpl& operator=(
      const PersonalizationAppAmbientProviderImpl&) = delete;

  ~PersonalizationAppAmbientProviderImpl() override;

  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::AmbientProvider>
          receiver) override;

  // ash::personalization_app::mojom:AmbientProvider:
  void IsAmbientModeEnabled(IsAmbientModeEnabledCallback callback) override;
  void SetAmbientObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
          observer) override;
  void SetAmbientModeEnabled(bool enabled) override;
  void SetTopicSource(ash::AmbientModeTopicSource topic_source) override;

  // TODO(b/216307771): Will need to add observer for this.
  void OnAmbientModeEnabledChanged(bool ambient_mode_enabled);

  // Notify WebUI the latest values.
  void OnTemperatureUnitChanged();
  void OnTopicSourceChanged();
  void OnAlbumsChanged();
  void OnAlbumPreviewChanged(const std::string& album_id,
                             std::string&& png_data_url);
  void OnRecentHighlightsPreviewsChanged();

 private:
  friend class PersonalizationAppAmbientProviderImplTest;

  bool IsAmbientModeEnabled();

  // Update the local `settings_` to server.
  void UpdateSettings();

  // Called when the settings is updated.
  // `success` is true when update successfully.
  void OnUpdateSettings(bool success);

  // Return true if a new update needed.
  // `success` is true when update successfully.
  bool MaybeScheduleNewUpdateSettings(bool success);

  // `success` is true when update successfully.
  void UpdateUIWithCachedSettings(bool success);

  // Fetch settings and albums from Backdrop server.
  void FetchSettingsAndAlbums();
  void OnSettingsAndAlbumsFetched(
      const absl::optional<ash::AmbientSettings>& settings,
      ash::PersonalAlbums personal_albums);

  // The `settings_` could be stale when the albums in Google Photos changes.
  // Prune the `selected_album_id` which does not exist any more.
  // Populate albums with selected info which will be shown on Settings UI.
  void SyncSettingsAndAlbums();

  // Update topic source if needed.
  void MaybeUpdateTopicSource(ash::AmbientModeTopicSource topic_source);

  void DownloadAlbumPreviewImage();
  void DownloadRecentHighlightsPreviewImages(
      const std::vector<std::string>& urls);

  void OnAlbumPreviewImageDownloaded(const std::string& album_id,
                                     const gfx::ImageSkia& image);

  ash::PersonalAlbum* FindPersonalAlbumById(const std::string& album_id);

  ash::ArtSetting* FindArtAlbumById(const std::string& album_id);

  // Reset local settings to start a new session.
  void ResetLocalSettings();

  mojo::Receiver<ash::personalization_app::mojom::AmbientProvider>
      ambient_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::AmbientObserver>
      ambient_observer_remote_;

  raw_ptr<Profile> const profile_ = nullptr;

  // Backoff retries for `FetchSettingsAndAlbums()`.
  net::BackoffEntry fetch_settings_retry_backoff_;

  // Backoff retries for `UpdateSettings()`.
  net::BackoffEntry update_settings_retry_backoff_;

  // Local settings which may contain changes from WebUI but have not sent to
  // server. Only one `UpdateSettgings()` at a time.
  absl::optional<ash::AmbientSettings> settings_;

  // The cached settings from the server. Should be the same as the server side.
  // This value will be updated when `RequestSettingsAndAlbums()` and
  // `UpdateSettings()` return successfully.
  // If `UpdateSettings()` fails, will restore to this value.
  absl::optional<ash::AmbientSettings> cached_settings_;

  // A temporary settings sent to the server in `UpdateSettings()`.
  absl::optional<ash::AmbientSettings> settings_sent_for_update_;

  ash::PersonalAlbums personal_albums_;

  // Whether to update UI when `UpdateSettings()` returns successfully.
  bool has_pending_fetch_request_ = false;

  // Whether the Settings updating is ongoing.
  bool is_updating_backend_ = false;

  // Whether there are pending updates.
  bool has_pending_updates_for_backend_ = false;

  std::vector<gfx::ImageSkia> recent_highlights_preview_images_;

  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      write_weak_factory_{this};
  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      read_weak_factory_{this};
  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      album_preview_weak_factory_{this};
  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      recent_highlights_previews_weak_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_
