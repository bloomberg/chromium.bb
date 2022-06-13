// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
#define ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

#include <array>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// A fake implementation of AmbientBackendController.
class ASH_PUBLIC_EXPORT FakeAmbientBackendControllerImpl
    : public AmbientBackendController {
 public:
  FakeAmbientBackendControllerImpl();
  ~FakeAmbientBackendControllerImpl() override;

  // AmbientBackendController:
  void FetchScreenUpdateInfo(
      int num_topics,
      OnScreenUpdateInfoFetchedCallback callback) override;
  void GetSettings(GetSettingsCallback callback) override;
  void UpdateSettings(const AmbientSettings& settings,
                      UpdateSettingsCallback callback) override;
  void FetchPersonalAlbums(int banner_width,
                           int banner_height,
                           int num_albums,
                           const std::string& resume_token,
                           OnPersonalAlbumsFetchedCallback callback) override;
  void FetchSettingsAndAlbums(
      int banner_width,
      int banner_height,
      int num_albums,
      OnSettingsAndAlbumsFetchedCallback callback) override;
  void FetchWeather(FetchWeatherCallback callback) override;
  const std::array<const char*, 2>& GetBackupPhotoUrls() const override;

  // Simulate to reply the request of FetchSettingsAndAlbums().
  // If |success| is true, will return fake data.
  // If |success| is false, will return null |settings| data.
  // If |settings| contains a value, that will be used as the argument to
  // the pending callback.
  void ReplyFetchSettingsAndAlbums(
      bool success,
      const absl::optional<AmbientSettings>& settings = absl::nullopt);

  // Whether there is a pending FetchSettingsAndAlbums() request.
  bool IsFetchSettingsAndAlbumsPending() const;

  // Simulate to reply the request of UpdateSettings() with |success|.
  void ReplyUpdateSettings(bool success);

  // Whether there is a pending UpdateSettings() request.
  bool IsUpdateSettingsPending() const;

  // Sets the weather info that will be returned in subsequent calls to
  // `FetchWeather`.
  void SetWeatherInfo(absl::optional<WeatherInfo> info);

  void SetPhotoOrientation(bool portrait);

  void SetPhotoTopicType(::ambient::TopicType topic_type);

 private:
  OnSettingsAndAlbumsFetchedCallback pending_fetch_settings_albums_callback_;

  UpdateSettingsCallback pending_update_callback_;

  absl::optional<WeatherInfo> weather_info_;

  bool is_portrait_ = false;

  bool has_related_image_ = true;

  ::ambient::TopicType topic_type_ = ::ambient::TopicType::kCulturalInstitute;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
