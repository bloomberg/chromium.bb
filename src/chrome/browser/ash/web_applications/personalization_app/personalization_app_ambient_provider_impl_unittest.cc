// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"

#include <memory>
#include <vector>

#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@example.com";

class TestAmbientObserver
    : public ash::personalization_app::mojom::AmbientObserver {
 public:
  void OnAmbientModeEnabledChanged(bool ambient_mode_enabled) override {
    ambient_mode_enabled_ = ambient_mode_enabled;
  }

  void OnTopicSourceChanged(ash::AmbientModeTopicSource topic_source) override {
    topic_source_ = topic_source;
  }

  void OnAlbumsChanged(
      std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums)
      override {
    albums_ = std::move(albums);
  }

  mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
  pending_remote() {
    if (ambient_observer_receiver_.is_bound()) {
      ambient_observer_receiver_.reset();
    }

    return ambient_observer_receiver_.BindNewPipeAndPassRemote();
  }

  bool is_ambient_mode_enabled() {
    ambient_observer_receiver_.FlushForTesting();
    return ambient_mode_enabled_;
  }

  ash::AmbientModeTopicSource topic_source() {
    ambient_observer_receiver_.FlushForTesting();
    return topic_source_;
  }

  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums() {
    ambient_observer_receiver_.FlushForTesting();
    return std::move(albums_);
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::AmbientObserver>
      ambient_observer_receiver_{this};

  bool ambient_mode_enabled_ = false;
  ash::AmbientModeTopicSource topic_source_ =
      ash::AmbientModeTopicSource::kArtGallery;
  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums_;
};

}  // namespace

class PersonalizationAppAmbientProviderImplTest : public testing::Test {
 public:
  PersonalizationAppAmbientProviderImplTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kPersonalizationHub);
  }
  PersonalizationAppAmbientProviderImplTest(
      const PersonalizationAppAmbientProviderImplTest&) = delete;
  PersonalizationAppAmbientProviderImplTest& operator=(
      const PersonalizationAppAmbientProviderImplTest&) = delete;
  ~PersonalizationAppAmbientProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    ambient_provider_ =
        std::make_unique<PersonalizationAppAmbientProviderImpl>(&web_ui_);

    ambient_provider_->BindInterface(
        ambient_provider_remote_.BindNewPipeAndPassReceiver());

    SetEnabledPref(true);
    fake_backend_controller_ =
        std::make_unique<ash::FakeAmbientBackendControllerImpl>();
    ambient_ash_test_helper_ = std::make_unique<ash::AmbientAshTestHelper>();
    ambient_ash_test_helper_->ambient_client().SetAutomaticalyIssueToken(true);
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::AmbientProvider>&
  ambient_provider_remote() {
    return ambient_provider_remote_;
  }

  content::TestWebUI* web_ui() { return &web_ui_; }

  void SetAmbientObserver() {
    ambient_provider_remote_->SetAmbientObserver(
        test_ambient_observer_.pending_remote());
  }

  bool ObservedAmbientModeEnabled() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.is_ambient_mode_enabled();
  }

  ash::AmbientModeTopicSource ObservedTopicSource() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.topic_source();
  }

  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr>
  ObservedAlbums() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.albums();
  }

  absl::optional<ash::AmbientSettings>& settings() {
    return ambient_provider_->settings_;
  }

  void SetEnabledPref(bool enabled) {
    profile()->GetPrefs()->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled,
                                      enabled);
  }

  void FetchSettings() { ambient_provider_->FetchSettingsAndAlbums(); }

  void UpdateSettings() {
    if (!ambient_provider_->settings_)
      ambient_provider_->settings_ = ash::AmbientSettings();

    ambient_provider_->UpdateSettings();
  }

  void SetTopicSource(ash::AmbientModeTopicSource topic_source) {
    ambient_provider_->SetTopicSource(topic_source);
  }

  ash::AmbientModeTopicSource TopicSource() {
    return ambient_provider_->settings_->topic_source;
  }

  void SetSelectedAlbums(const std::vector<std::string>& ids) {
    ambient_provider_->settings_->selected_album_ids = ids;
  }

  bool HasPendingFetchRequestAtProvider() const {
    return ambient_provider_->has_pending_fetch_request_;
  }

  bool IsUpdateSettingsPendingAtProvider() const {
    return ambient_provider_->is_updating_backend_;
  }

  bool HasPendingUpdatesAtProvider() const {
    return ambient_provider_->has_pending_updates_for_backend_;
  }

  base::TimeDelta GetFetchSettingsDelay() {
    return ambient_provider_->fetch_settings_retry_backoff_
        .GetTimeUntilRelease();
  }

  base::TimeDelta GetUpdateSettingsDelay() {
    return ambient_provider_->update_settings_retry_backoff_
        .GetTimeUntilRelease();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  bool IsFetchSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsFetchSettingsAndAlbumsPending();
  }

  void ReplyFetchSettingsAndAlbums(
      bool success,
      absl::optional<ash::AmbientSettings> settings = absl::nullopt) {
    fake_backend_controller_->ReplyFetchSettingsAndAlbums(success,
                                                          std::move(settings));
  }

  bool IsUpdateSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsUpdateSettingsPending();
  }

  void ReplyUpdateSettings(bool success) {
    fake_backend_controller_->ReplyUpdateSettings(success);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::AmbientProvider>
      ambient_provider_remote_;
  std::unique_ptr<PersonalizationAppAmbientProviderImpl> ambient_provider_;
  TestAmbientObserver test_ambient_observer_;

  std::unique_ptr<ash::AmbientAshTestHelper> ambient_ash_test_helper_;
  std::unique_ptr<ash::FakeAmbientBackendControllerImpl>
      fake_backend_controller_;
};

TEST_F(PersonalizationAppAmbientProviderImplTest, IsAmbientModeEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, true);
  bool called = false;
  ambient_provider_remote()->IsAmbientModeEnabled(
      base::BindLambdaForTesting([&called](bool enabled) {
        called = true;
        EXPECT_TRUE(enabled);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);

  called = false;
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);
  ambient_provider_remote()->IsAmbientModeEnabled(
      base::BindLambdaForTesting([&called](bool enabled) {
        called = true;
        EXPECT_FALSE(enabled);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetAmbientModeEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  // Clear pref.
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);

  ambient_provider_remote()->SetAmbientModeEnabled(true);
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));

  ambient_provider_remote()->SetAmbientModeEnabled(false);
  ambient_provider_remote().FlushForTesting();
  EXPECT_FALSE(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAmbientModeEnabledChanged) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);
  SetAmbientObserver();
  ambient_provider_remote().FlushForTesting();
  EXPECT_FALSE(ObservedAmbientModeEnabled());

  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, true);
  SetAmbientObserver();
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(ObservedAmbientModeEnabled());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnTopicSourceChanged) {
  // Will fetch settings when observer is set.
  SetAmbientObserver();
  ambient_provider_remote().FlushForTesting();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, ObservedTopicSource());

  SetTopicSource(ash::AmbientModeTopicSource::kArtGallery);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, ObservedTopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, ShouldCallOnAlbumsChanged) {
  // Will fetch settings when observer is set.
  SetAmbientObserver();
  ambient_provider_remote().FlushForTesting();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  auto albums = ObservedAlbums();
  // The fake albums are set in FakeAmbientBackendControllerImpl. Hidden setting
  // will be sent to JS side.
  EXPECT_EQ(4, albums.size());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetTopicSource) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());

  SetTopicSource(ash::AmbientModeTopicSource::kArtGallery);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, TopicSource());

  SetTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());

  // If `settings_->selected_album_ids` is empty, will fallback to kArtGallery.
  SetSelectedAlbums(/*ids=*/{});
  SetTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, TopicSource());

  SetSelectedAlbums(/*ids=*/{"1"});
  SetTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestFetchSettings) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsFailedWillRetry) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsSecondRetryWillBackoff) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay1 = GetFetchSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay2 = GetFetchSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsWillNotRetryMoreThanThreeTimes) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 1st retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 2nd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 3rd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // Will not retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestUpdateSettings) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestUpdateSettingsTwice) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(HasPendingUpdatesAtProvider());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsFailedWillRetry) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsSecondRetryWillBackoff) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  base::TimeDelta delay1 = GetUpdateSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  base::TimeDelta delay2 = GetUpdateSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsWillNotRetryMoreThanThreeTimes) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // Will not retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestNoFetchRequestWhenUpdatingSettings) {
  EXPECT_FALSE(HasPendingFetchRequestAtProvider());
  UpdateSettings();
  EXPECT_FALSE(HasPendingFetchRequestAtProvider());

  FetchSettings();
  EXPECT_TRUE(HasPendingFetchRequestAtProvider());
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestWeatherFalseTriggersUpdateSettings) {
  ash::AmbientSettings weather_off_settings;
  weather_off_settings.show_weather = false;

  // Simulate initial page request with weather settings false. Because Ambient
  // mode pref is enabled and |settings.show_weather| is false, this should
  // trigger a call to |UpdateSettings| that sets |settings.show_weather| to
  // true.
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true, weather_off_settings);

  // A call to |UpdateSettings| should have happened.
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // |settings.show_weather| should now be true after the successful settings
  // update.
  EXPECT_TRUE(settings()->show_weather);
}
