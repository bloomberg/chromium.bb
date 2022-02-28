// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_

#include <vector>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
struct AmbientSettings;
}  // namespace ash

namespace gfx {
class ImageSkia;
}  // namespace gfx

class PrefService;

namespace chromeos {
namespace settings {

// Chrome OS ambient mode settings page UI handler, to allow users to customize
// photo frame and other related functionalities.
class AmbientModeHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit AmbientModeHandler(PrefService* pref_service);
  AmbientModeHandler(const AmbientModeHandler&) = delete;
  AmbientModeHandler& operator=(const AmbientModeHandler&) = delete;
  ~AmbientModeHandler() override;

  // settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class AmbientModeHandlerTest;

  bool IsAmbientModeEnabled();

  void OnEnabledPrefChanged();

  // WebUI call to request topic source and temperature unit related data.
  void HandleRequestSettings(base::Value::ConstListView args);

  // WebUI call to request albums related data.
  void HandleRequestAlbums(base::Value::ConstListView args);

  // WebUI call to sync temperature unit with server.
  void HandleSetSelectedTemperatureUnit(base::Value::ConstListView args);

  // WebUI call to sync albums with server.
  void HandleSetSelectedAlbums(base::Value::ConstListView args);

  // Send the "temperature-unit-changed" WebUIListener event to update the
  // WebUI.
  void SendTemperatureUnit();

  // Send the "topic-source-changed" WebUIListener event to update the WebUI.
  void SendTopicSource();

  // Send the "albums-changed" WebUIListener event with albums info
  // in the |topic_source|.
  void SendAlbums(ash::AmbientModeTopicSource topic_source);

  // Send the "album-preview-changed" WebUIListener event with album preview
  // in the |topic_source|.
  void SendAlbumPreview(ash::AmbientModeTopicSource topic_source,
                        const std::string& album_id,
                        std::string&& png_data_url);

  // Send the "album-preview-changed" WebUIListener event with Recent Highlights
  // previews in the |topic_source|.
  void SendRecentHighlightsPreviews();

  // Update the local |settings_| to server.
  void UpdateSettings();

  // Called when the settings is updated.
  // |success| is true when update successfully.
  void OnUpdateSettings(bool success);

  // Return true if a new update needed.
  // |success| is true when update successfully.
  bool MaybeScheduleNewUpdateSettings(bool success);

  // |success| is true when update successfully.
  void UpdateUIWithCachedSettings(bool success);

  // Will be called from ambientMode/photos subpage and ambientMode subpage.
  // |topic_source| is used to request the albums in that source and identify
  // the callers:
  //   1. |kGooglePhotos|: ambientMode/photos?topicSource=0
  //   2. |kArtGallery|:   ambientMode/photos?topicSource=1
  //   3. absl::nullopt:   ambientMode/
  void RequestSettingsAndAlbums(
      absl::optional<ash::AmbientModeTopicSource> topic_source);
  void OnSettingsAndAlbumsFetched(
      absl::optional<ash::AmbientModeTopicSource> topic_source,
      const absl::optional<ash::AmbientSettings>& settings,
      ash::PersonalAlbums personal_albums);

  // The |settings_| could be stale when the albums in Google Photos changes.
  // Prune the |selected_album_id| which does not exist any more.
  // Populate albums with selected info which will be shown on Settings UI.
  void SyncSettingsAndAlbums();

  // Update topic source if needed.
  void UpdateTopicSource(ash::AmbientModeTopicSource topic_source);
  void MaybeUpdateTopicSource(ash::AmbientModeTopicSource topic_source);

  void DownloadAlbumPreviewImage(ash::AmbientModeTopicSource topic_source);
  void DownloadRecentHighlightsPreviewImages(
      const std::vector<std::string>& urls);

  void OnAlbumPreviewImageDownloaded(ash::AmbientModeTopicSource topic_source,
                                     const std::string& album_id,
                                     const gfx::ImageSkia& image);

  ash::PersonalAlbum* FindPersonalAlbumById(const std::string& album_id);

  ash::ArtSetting* FindArtAlbumById(const std::string& album_id);

  // Local settings which may contain changes from WebUI but have not sent to
  // server.
  absl::optional<ash::AmbientSettings> settings_;

  // The cached settings from the server. Should be the same as the server side.
  // This value will be updated when RequestSettingsAndAlbums() and
  // UpdateSettings() return successfully.
  // If UpdateSettings() fails, will restore to this value.
  absl::optional<ash::AmbientSettings> cached_settings_;

  // A temporary settings sent to the server in UpdateSettings().
  absl::optional<ash::AmbientSettings> settings_sent_for_update_;

  ash::PersonalAlbums personal_albums_;

  // Backoff retries for RequestSettingsAndAlbums().
  net::BackoffEntry fetch_settings_retry_backoff_;

  // Whether to update UI when UpdateSettings() returns successfully.
  bool has_pending_fetch_request_ = false;

  // The topic source in the latest RequestSettingsAndAlbums().
  absl::optional<ash::AmbientModeTopicSource> last_fetch_request_topic_source_;

  // Whether the Settings updating is ongoing.
  bool is_updating_backend_ = false;

  // Whether there are pending updates.
  bool has_pending_updates_for_backend_ = false;

  // Backoff retries for UpdateSettings().
  net::BackoffEntry update_settings_retry_backoff_;

  std::vector<gfx::ImageSkia> recent_highlights_preview_images_;

  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<AmbientModeHandler> backend_weak_factory_{this};
  base::WeakPtrFactory<AmbientModeHandler> ui_update_weak_factory_{this};
  base::WeakPtrFactory<AmbientModeHandler>
      recent_highlights_previews_weak_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_AMBIENT_MODE_HANDLER_H_
