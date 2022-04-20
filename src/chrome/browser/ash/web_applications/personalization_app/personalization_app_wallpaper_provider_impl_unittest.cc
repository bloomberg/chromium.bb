// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_wallpaper_provider_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/web_applications/personalization_app/mock_personalization_app_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome_device_policy.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {
namespace personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";
constexpr char kGooglePhotosAlbumsFullResponse[] =
    "{"
    "   \"collection\": [ {"
    "      \"collectionId\": {"
    "         \"mediaKey\": \"albumId\""
    "      },"
    "      \"coverItemServingUrl\": \"https://www.google.com/\","
    "      \"name\": \"title\","
    "      \"numPhotos\": \"1\""
    "   } ],"
    "   \"resumeToken\": \"token\""
    "}";
constexpr char kGooglePhotosPhotosFullResponse[] =
    "{"
    "   \"item\": [ {"
    "      \"itemId\": {"
    "         \"mediaKey\": \"photoId\""
    "      },"
    "      \"filename\": \"photoName.png\","
    "      \"creationTimestamp\": \"2021-12-31T07:07:07.000Z\","
    "      \"photo\": {"
    "         \"servingUrl\": \"https://www.google.com/\""
    "      },"
    "      \"locationFeature\": {"
    "         \"name\": [ {"
    "            \"text\": \"home\""
    "         } ]"
    "      }"
    "   } ],"
    "   \"resumeToken\": \"token\""
    "}";
constexpr char kGooglePhotosPhotosSingleItemResponse[] =
    "{"
    "   \"item\": {"
    "      \"itemId\": {"
    "         \"mediaKey\": \"photoId\""
    "      },"
    "      \"filename\": \"photoName.png\","
    "      \"creationTimestamp\": \"2021-12-31T07:07:07.000Z\","
    "      \"photo\": {"
    "         \"servingUrl\": \"https://www.google.com/\""
    "      },"
    "      \"locationFeature\": {"
    "         \"name\": [ {"
    "            \"text\": \"home\""
    "         } ]"
    "      }"
    "   }"
    "}";
constexpr char kGooglePhotosResumeTokenOnlyResponse[] =
    "{\"resumeToken\": \"token\"}";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  ash::device_settings_cache::RegisterPrefs(local_state->registry());
  user_manager::KnownUser::RegisterPrefs(local_state->registry());
  ash::WallpaperControllerImpl::RegisterLocalStatePrefs(
      local_state->registry());
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(
      local_state->registry());
  ProfileAttributesStorage::RegisterPrefs(local_state->registry());
  return local_state;
}

void AddAndLoginUser(const AccountId& account_id) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());

  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
}

// Create a test 1x1 image with a given |color|.
gfx::ImageSkia CreateSolidImageSkia(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Parses `json` as a value dictionary. A test calling this function will fail
// if `json` is not appropriately formatted.
base::Value::Dict JsonToDict(base::StringPiece json) {
  absl::optional<base::Value> parsed_json = base::JSONReader::Read(json);
  EXPECT_TRUE(parsed_json.has_value() && parsed_json->is_dict());
  return std::move(parsed_json->GetDict());
}

// Returns a non-null pointer to the album in a hypothetical Google Photos
// albums query response. A test calling this function will fail if the response
// does not contain exactly one album.
base::Value::Dict* GetAlbumFromGooglePhotosAlbumsResponse(
    base::Value::Dict* response) {
  EXPECT_TRUE(response);
  auto* albums = response->FindList("collection");
  EXPECT_TRUE(albums && albums->size() == 1);
  auto* album = albums->front().GetIfDict();
  EXPECT_TRUE(album);
  return album;
}

// Returns a non-null pointer to the photo in a hypothetical Google Photos
// photos query response. A test calling this function will fail if the response
// does not contain exactly one photo.
base::Value::Dict* GetPhotoFromGooglePhotosPhotosResponse(
    base::Value::Dict* response) {
  EXPECT_TRUE(response);
  auto* photos = response->FindList("item");
  EXPECT_TRUE(photos && photos->size() == 1);
  auto* photo = photos->front().GetIfDict();
  EXPECT_TRUE(photo);
  return photo;
}

std::unique_ptr<KeyedService> MakeMockPersonalizationAppManager(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<MockPersonalizationAppManager>>();
}

class TestWallpaperObserver
    : public ash::personalization_app::mojom::WallpaperObserver {
 public:
  void OnWallpaperChanged(
      ash::personalization_app::mojom::CurrentWallpaperPtr image) override {
    current_wallpaper_ = std::move(image);
  }

  mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
  pending_remote() {
    DCHECK(!wallpaper_observer_receiver_.is_bound());
    return wallpaper_observer_receiver_.BindNewPipeAndPassRemote();
  }

  ash::personalization_app::mojom::CurrentWallpaper* current_wallpaper() {
    if (!wallpaper_observer_receiver_.is_bound())
      return nullptr;

    wallpaper_observer_receiver_.FlushForTesting();
    return current_wallpaper_.get();
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_receiver_{this};

  ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper_ =
      nullptr;
};

}  // namespace

class PersonalizationAppWallpaperProviderImplTest
    : public testing::Test,
      public testing::WithParamInterface<bool /* google_photos_enabled */> {
 public:
  PersonalizationAppWallpaperProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    std::vector<base::Feature> features = {ash::features::kWallpaperWebUI};
    if (GooglePhotosEnabled())
      features.push_back(ash::features::kWallpaperGooglePhotosIntegration);
    // Note: `scoped_feature_list_` should be initialized before `SetUp()`
    // (see crbug.com/846380).
    scoped_feature_list_.InitWithFeatures(features, /*disabled_features=*/{});
  }

  PersonalizationAppWallpaperProviderImplTest(
      const PersonalizationAppWallpaperProviderImplTest&) = delete;
  PersonalizationAppWallpaperProviderImplTest& operator=(
      const PersonalizationAppWallpaperProviderImplTest&) = delete;
  ~PersonalizationAppWallpaperProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClientImpl>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(
        kFakeTestEmail,
        {{ash::personalization_app::PersonalizationAppManagerFactory::
              GetInstance(),
          base::BindRepeating(&MakeMockPersonalizationAppManager)}});

    AddAndLoginUser(
        AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    wallpaper_provider_ =
        std::make_unique<PersonalizationAppWallpaperProviderImpl>(&web_ui_);

    wallpaper_provider_->BindInterface(
        wallpaper_provider_remote_.BindNewPipeAndPassReceiver());
  }

  PersonalizationAppWallpaperProviderImpl::ImageInfo GetDefaultImageInfo() {
    return {
        /*in_image_url=*/GURL("http://test_url"),
        /*in_collection_id=*/"collection_id",
        /*in_asset_id=*/1,
        /*in_unit_id=*/1,
        /*in_type=*/backdrop::Image::IMAGE_TYPE_UNKNOWN,
    };
  }

  void AddWallpaperImage(
      const PersonalizationAppWallpaperProviderImpl::ImageInfo& image_info) {
    wallpaper_provider_->image_asset_id_map_.insert(
        {image_info.asset_id, image_info});
  }

  // Returns true if the test should run with the Google Photos Wallpaper
  // integration enabled, false otherwise.
  bool GooglePhotosEnabled() const { return GetParam(); }

  TestWallpaperController* test_wallpaper_controller() {
    return &test_wallpaper_controller_;
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::WallpaperProvider>*
  wallpaper_provider_remote() {
    return &wallpaper_provider_remote_;
  }

  PersonalizationAppWallpaperProviderImpl* delegate() {
    return wallpaper_provider_.get();
  }

  void ResetWallpaperProvider() { wallpaper_provider_.reset(); }

  ::testing::NiceMock<MockPersonalizationAppManager>*
  MockPersonalizationAppManager() {
    return static_cast<::testing::NiceMock<
        ::ash::personalization_app::MockPersonalizationAppManager>*>(
        ::ash::personalization_app::PersonalizationAppManagerFactory::
            GetForBrowserContext(profile_));
  }

  void SetWallpaperObserver() {
    wallpaper_provider_remote_->SetWallpaperObserver(
        test_wallpaper_observer_.pending_remote());
  }

  ash::personalization_app::mojom::CurrentWallpaper* current_wallpaper() {
    wallpaper_provider_remote_.FlushForTesting();
    return test_wallpaper_observer_.current_wallpaper();
  }

 private:
  // Note: `scoped_feature_list_` should be destroyed after `task_environment_`
  // (see crbug.com/846380).
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  // Required for |ScopedTestCrosSettings|.
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  // Required for |ScopedTestCrosSettings|.
  ash::ScopedTestDeviceSettingsService scoped_device_settings_;
  // Required for |WallpaperControllerClientImpl|.
  ash::ScopedTestCrosSettings scoped_testing_cros_settings_{
      RegisterPrefs(&pref_service_)};
  user_manager::ScopedUserManager scoped_user_manager_;
  TestWallpaperController test_wallpaper_controller_;
  // |wallpaper_controller_client_| must be destructed before
  // |test_wallpaper_controller_|.
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_provider_remote_;
  TestWallpaperObserver test_wallpaper_observer_;
  std::unique_ptr<PersonalizationAppWallpaperProviderImpl> wallpaper_provider_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PersonalizationAppWallpaperProviderImplTest,
                         /*google_photos_enabled=*/::testing::Values(false));

TEST_P(PersonalizationAppWallpaperProviderImplTest, SelectWallpaper) {
  test_wallpaper_controller()->ClearCounts();

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  wallpaper_provider_remote()->get()->SelectWallpaper(
      image_info.asset_id, /*preview_mode=*/false,
      base::BindLambdaForTesting([](bool success) { EXPECT_TRUE(success); }));
  wallpaper_provider_remote()->FlushForTesting();

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_EQ(
      ash::WallpaperInfo(
          {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
           absl::make_optional(image_info.asset_id), image_info.image_url,
           "collection_id",
           ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
           /*preview_mode=*/false, /*from_user=*/true,
           /*daily_refresh_enabled=*/false, image_info.unit_id, variants}),
      test_wallpaper_controller()->wallpaper_info().value());
}

TEST_P(PersonalizationAppWallpaperProviderImplTest, PreviewWallpaper) {
  test_wallpaper_controller()->ClearCounts();

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        image_info.type);

  AddWallpaperImage(image_info);

  wallpaper_provider_remote()->get()->SelectWallpaper(
      image_info.asset_id, /*preview_mode=*/true,
      base::BindLambdaForTesting([](bool success) { EXPECT_TRUE(success); }));
  wallpaper_provider_remote()->FlushForTesting();

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_EQ(
      ash::WallpaperInfo(
          {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
           absl::make_optional(image_info.asset_id), image_info.image_url,
           "collection_id",
           ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
           /*preview_mode=*/true, /*from_user=*/true,
           /*daily_refresh_enabled=*/false, image_info.unit_id, variants}),
      test_wallpaper_controller()->wallpaper_info().value());
}

TEST_P(PersonalizationAppWallpaperProviderImplTest,
       ObserveWallpaperFiresWhenBound) {
  // This will create the data url referenced below in expectation.
  test_wallpaper_controller()->ShowWallpaperImage(
      CreateSolidImageSkia(/*width=*/1, /*height=*/1, SK_ColorBLACK));

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  test_wallpaper_controller()->SetOnlineWallpaper(
      {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
       absl::make_optional(image_info.asset_id), image_info.image_url,
       "collection_id", ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*from_user=*/true,
       /*daily_refresh_enabled=*/false, image_info.unit_id, variants},
      base::DoNothing());

  EXPECT_EQ(nullptr, current_wallpaper());

  SetWallpaperObserver();

  // WallpaperObserver Should have received an image through mojom.
  ash::personalization_app::mojom::CurrentWallpaper* current =
      current_wallpaper();

  EXPECT_EQ(ash::WallpaperType::kOnline, current->type);
  EXPECT_EQ(ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
            current->layout);
  // Data url of a solid black image scaled up to 256x256.
  EXPECT_EQ(webui::GetBitmapDataUrl(
                *CreateSolidImageSkia(256, 256, SK_ColorBLACK).bitmap()),
            current->url);
}

TEST_P(PersonalizationAppWallpaperProviderImplTest, SetCurrentWallpaperLayout) {
  auto* ctrl = test_wallpaper_controller();

  EXPECT_EQ(ctrl->update_current_wallpaper_layout_count(), 0);
  EXPECT_EQ(ctrl->update_current_wallpaper_layout_layout(), absl::nullopt);

  auto layout = ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER;
  wallpaper_provider_remote()->get()->SetCurrentWallpaperLayout(layout);
  wallpaper_provider_remote()->FlushForTesting();

  EXPECT_EQ(ctrl->update_current_wallpaper_layout_count(), 1);
  EXPECT_EQ(ctrl->update_current_wallpaper_layout_layout(), layout);
}

TEST_P(PersonalizationAppWallpaperProviderImplTest,
       CallsMaybeStartHatsTimerOnDestruction) {
  // Insert a wallpaper image info to indicate that the user opened the
  // wallpaper app and requested wallpapers.
  AddWallpaperImage(GetDefaultImageInfo());

  EXPECT_CALL(*MockPersonalizationAppManager(),
              MaybeStartHatsTimer(HatsSurveyType::kWallpaper))
      .Times(1);

  ResetWallpaperProvider();
}

TEST_P(PersonalizationAppWallpaperProviderImplTest,
       DoesNotCallMaybeStartHatsTimerIfNoWallpaperFetched) {
  EXPECT_CALL(*MockPersonalizationAppManager(),
              MaybeStartHatsTimer(HatsSurveyType::kWallpaper))
      .Times(0);

  ResetWallpaperProvider();
}

class PersonalizationAppWallpaperProviderImplGooglePhotosTest
    : public PersonalizationAppWallpaperProviderImplTest {
 protected:
  // Mocks an attempt to fetch the Google Photos enterprise setting from the
  // server. A successful attempt, which happens when the Google Photos
  // wallpaper integration is enabled, will enable wallpaper selection and
  // other Google Photos data fetches to go through.
  void FetchGooglePhotosEnabled(size_t num_fetches = 1) {
    using ash::personalization_app::mojom::GooglePhotosEnablementState;

    // Mock a fetcher for the enablement state query.
    auto* const google_photos_enabled_fetcher = static_cast<::testing::NiceMock<
        wallpaper_handlers::MockGooglePhotosEnabledFetcher>*>(
        delegate()->SetGooglePhotosEnabledFetcherForTest(
            std::make_unique<::testing::NiceMock<
                wallpaper_handlers::MockGooglePhotosEnabledFetcher>>(
                profile())));

    EXPECT_CALL(*google_photos_enabled_fetcher, AddRequestAndStartIfNecessary)
        .Times(GooglePhotosEnabled() ? num_fetches : 0);

    for (size_t i = 0; i < num_fetches; ++i) {
      wallpaper_provider_remote()->get()->FetchGooglePhotosEnabled(
          base::BindLambdaForTesting([this](GooglePhotosEnablementState state) {
            EXPECT_EQ(state, GooglePhotosEnabled()
                                 ? GooglePhotosEnablementState::kEnabled
                                 : GooglePhotosEnablementState::kError);
          }));
    }
    wallpaper_provider_remote()->FlushForTesting();
  }

  // The number of times to start each idempotent API query.
  static constexpr size_t kNumFetches = 2;
  // Resume token value used across several tests.
  const std::string kResumeToken = "token";
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PersonalizationAppWallpaperProviderImplGooglePhotosTest,
    /*google_photos_enabled=*/::testing::Bool());

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchAlbums) {
  // Mock a fetcher for the albums query.
  auto* const google_photos_albums_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosAlbumsFetcher>*>(
      delegate()->SetGooglePhotosAlbumsFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosAlbumsFetcher>>(profile())));

  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  EXPECT_CALL(*google_photos_albums_fetcher,
              AddRequestAndStartIfNecessary(absl::make_optional(kResumeToken),
                                            ::testing::_))
      .Times(GooglePhotosEnabled() ? kNumFetches : 0);

  // Test fetching Google Photos albums before fetching the enterprise setting.
  // No requests should be made.
  for (size_t i = 0; i < kNumFetches; ++i) {
    wallpaper_provider_remote()->get()->FetchGooglePhotosAlbums(
        kResumeToken, base::BindLambdaForTesting(
                          [](ash::personalization_app::mojom::
                                 FetchGooglePhotosAlbumsResponsePtr response) {
                            EXPECT_FALSE(response->albums.has_value());
                          }));
  }
  wallpaper_provider_remote()->FlushForTesting();

  // Test fetching Google Photos albums after fetching the enterprise setting.
  // Requests should be made if and only if the Google Photos wallpaper
  // integration is enabled.
  FetchGooglePhotosEnabled();
  for (size_t i = 0; i < kNumFetches; ++i) {
    wallpaper_provider_remote()->get()->FetchGooglePhotosAlbums(
        kResumeToken,
        base::BindLambdaForTesting(
            [this](ash::personalization_app::mojom::
                       FetchGooglePhotosAlbumsResponsePtr response) {
              EXPECT_EQ(response->albums.has_value(), GooglePhotosEnabled());
            }));
  }
  wallpaper_provider_remote()->FlushForTesting();
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchCount) {
  // Mock a fetcher for the photos count query.
  auto* const google_photos_count_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosCountFetcher>*>(
      delegate()->SetGooglePhotosCountFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosCountFetcher>>(profile())));

  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  EXPECT_CALL(*google_photos_count_fetcher, AddRequestAndStartIfNecessary)
      .Times(GooglePhotosEnabled() ? kNumFetches : 0);

  // Test fetching Google Photos count before fetching the enterprise setting.
  // No requests should be made.
  for (size_t i = 0; i < kNumFetches; ++i) {
    wallpaper_provider_remote()->get()->FetchGooglePhotosCount(
        base::BindLambdaForTesting([](int count) { EXPECT_EQ(count, -1); }));
  }
  wallpaper_provider_remote()->FlushForTesting();

  // Test fetching Google Photos count after fetching the enterprise setting.
  // Requests should be made if and only if the Google Photos wallpaper
  // integration is enabled.
  FetchGooglePhotosEnabled();
  for (size_t i = 0; i < kNumFetches; ++i) {
    wallpaper_provider_remote()->get()->FetchGooglePhotosCount(
        base::BindLambdaForTesting([this](int count) {
          EXPECT_EQ(count, GooglePhotosEnabled() ? 0 : -1);
        }));
  }
  wallpaper_provider_remote()->FlushForTesting();
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchEnabled) {
  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  FetchGooglePhotosEnabled(kNumFetches);
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchPhotos) {
  // Mock a fetcher for the photos query.
  auto* const google_photos_photos_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosPhotosFetcher>*>(
      delegate()->SetGooglePhotosPhotosFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosPhotosFetcher>>(profile())));

  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  const std::string item_id = "itemId";
  const std::string album_id = "albumId";
  EXPECT_CALL(*google_photos_photos_fetcher,
              AddRequestAndStartIfNecessary(
                  absl::make_optional(item_id), absl::make_optional(album_id),
                  absl::make_optional(kResumeToken), ::testing::_))
      .Times(GooglePhotosEnabled() ? kNumFetches : 0);

  // Test fetching Google Photos photos before fetching the enterprise setting.
  // No requests should be made.
  for (size_t i = 0; i < kNumFetches; ++i) {
    wallpaper_provider_remote()->get()->FetchGooglePhotosPhotos(
        item_id, album_id, kResumeToken,
        base::BindLambdaForTesting(
            [](ash::personalization_app::mojom::
                   FetchGooglePhotosPhotosResponsePtr response) {
              EXPECT_FALSE(response->photos.has_value());
            }));
  }
  wallpaper_provider_remote()->FlushForTesting();

  // Test fetching Google Photos photos after fetching the enterprise setting.
  // Requests should be made if and only if the Google Photos wallpaper
  // integration is enabled.
  FetchGooglePhotosEnabled();
  for (size_t i = 0; i < kNumFetches; ++i) {
    wallpaper_provider_remote()->get()->FetchGooglePhotosPhotos(
        item_id, album_id, kResumeToken,
        base::BindLambdaForTesting(
            [this](ash::personalization_app::mojom::
                       FetchGooglePhotosPhotosResponsePtr response) {
              EXPECT_EQ(response->photos.has_value(), GooglePhotosEnabled());
            }));
  }
  wallpaper_provider_remote()->FlushForTesting();
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       ParseAlbumsAbsentAlbum) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_albums_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosAlbumsFetcher>*>(
      delegate()->SetGooglePhotosAlbumsFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosAlbumsFetcher>>(profile())));

  // Parse an absent response (simulating a fetching error).
  auto result = FetchGooglePhotosAlbumsResponse::New();
  EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(nullptr), result);
  EXPECT_EQ(google_photos_albums_fetcher->GetResultCount(result),
            absl::nullopt);

  // Parse a response with no resume token or albums.
  base::Value::Dict empty_response;
  EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(&empty_response),
            result);

  // Parse a response with a resume token and no albums.
  auto response = JsonToDict(kGooglePhotosResumeTokenOnlyResponse);
  result = FetchGooglePhotosAlbumsResponse::New(absl::nullopt, kResumeToken);
  EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_albums_fetcher->GetResultCount(result),
            absl::nullopt);
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       ParseAlbumsInvalidAlbum) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_albums_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosAlbumsFetcher>*>(
      delegate()->SetGooglePhotosAlbumsFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosAlbumsFetcher>>(profile())));

  auto result = FetchGooglePhotosAlbumsResponse::New(
      std::vector<GooglePhotosAlbumPtr>(), kResumeToken);

  // Parse one-album responses where one of the album's fields is missing.
  for (const auto* const path :
       {"collectionId.mediaKey", "name", "numPhotos", "coverItemServingUrl"}) {
    auto response = JsonToDict(kGooglePhotosAlbumsFullResponse);
    auto* album = GetAlbumFromGooglePhotosAlbumsResponse(&response);
    album->RemoveByDottedPath(path);
    EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(&response), result);
    EXPECT_EQ(google_photos_albums_fetcher->GetResultCount(result),
              absl::make_optional<size_t>(0u));
  }

  // Parse one-album responses where one of the album's fields has an invalid
  // value.
  std::vector<std::pair<std::string, std::string>> invalid_field_test_cases = {
      {"numPhotos", ""},
      {"numPhotos", "NaN"},
      {"numPhotos", "-1"},
      {"numPhotos", "0"}};
  EXPECT_CALL(*google_photos_albums_fetcher, ParseResponse).Times(4);
  for (const auto& kv : invalid_field_test_cases) {
    auto response = JsonToDict(kGooglePhotosAlbumsFullResponse);
    auto* album = GetAlbumFromGooglePhotosAlbumsResponse(&response);
    album->SetByDottedPath(kv.first, kv.second);
    EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(&response), result);
    EXPECT_EQ(google_photos_albums_fetcher->GetResultCount(result),
              absl::make_optional<size_t>(0u));
  }
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       ParseAlbumsValidAlbum) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbum;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_albums_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosAlbumsFetcher>*>(
      delegate()->SetGooglePhotosAlbumsFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosAlbumsFetcher>>(profile())));

  // Parse a response with a valid album and a resume token.
  auto response = JsonToDict(kGooglePhotosAlbumsFullResponse);
  auto valid_albums_vector = std::vector<GooglePhotosAlbumPtr>();
  valid_albums_vector.push_back(GooglePhotosAlbum::New(
      "albumId", "title", 1, GURL("https://www.google.com/")));
  auto result = FetchGooglePhotosAlbumsResponse::New(
      mojo::Clone(valid_albums_vector), kResumeToken);
  EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_albums_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(valid_albums_vector.size()));

  // Parse a response with a valid album and no resume token.
  response.Remove("resumeToken");
  result = FetchGooglePhotosAlbumsResponse::New(
      mojo::Clone(valid_albums_vector), absl::nullopt);
  EXPECT_EQ(google_photos_albums_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_albums_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(valid_albums_vector.size()));
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest, ParseCount) {
  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_count_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosCountFetcher>*>(
      delegate()->SetGooglePhotosCountFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosCountFetcher>>(profile())));

  // Parse an absent response (simulating a fetching error).
  auto result = -1;
  EXPECT_EQ(google_photos_count_fetcher->ParseResponse(nullptr), result);
  EXPECT_EQ(google_photos_count_fetcher->GetResultCount(result), absl::nullopt);

  // Parse a response without a photo count.
  base::Value::Dict response;
  EXPECT_EQ(google_photos_count_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_count_fetcher->GetResultCount(result), absl::nullopt);

  // Parse a response with an empty photo count.
  response.SetByDottedPath("user.numPhotos", "");
  EXPECT_EQ(google_photos_count_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_count_fetcher->GetResultCount(result), absl::nullopt);

  // Parse a response with a non-integer photo count.
  response.SetByDottedPath("user.numPhotos", "NaN");
  EXPECT_EQ(google_photos_count_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_count_fetcher->GetResultCount(result), absl::nullopt);

  // Parse a response with a negative photo count.
  response.SetByDottedPath("user.numPhotos", "-2");
  EXPECT_EQ(google_photos_count_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_count_fetcher->GetResultCount(result), absl::nullopt);

  // Parse a valid response.
  response.SetByDottedPath("user.numPhotos", "2");
  result = 2;
  EXPECT_EQ(google_photos_count_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_count_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(1u));
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest, ParseEnabled) {
  using ash::personalization_app::mojom::GooglePhotosEnablementState;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_enabled_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosEnabledFetcher>*>(
      delegate()->SetGooglePhotosEnabledFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosEnabledFetcher>>(profile())));

  // Parse an absent response (simulating a fetching error).
  auto result = GooglePhotosEnablementState::kError;
  EXPECT_EQ(google_photos_enabled_fetcher->ParseResponse(nullptr), result);
  EXPECT_EQ(google_photos_enabled_fetcher->GetResultCount(result),
            absl::nullopt);

  // Parse a response without an enabled state.
  base::Value::Dict response;
  EXPECT_EQ(google_photos_enabled_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_enabled_fetcher->GetResultCount(result),
            absl::nullopt);

  // Parse a response with an unknown enabled state.
  response.SetByDottedPath("status.userState", "UNKNOWN_STATUS_STATE");
  EXPECT_EQ(google_photos_enabled_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_enabled_fetcher->GetResultCount(result),
            absl::nullopt);

  // Parse a response indicating that the user cannot access Google Photos data.
  response.SetByDottedPath("status.userState", "USER_DASHER_DISABLED");
  result = GooglePhotosEnablementState::kDisabled;
  EXPECT_EQ(google_photos_enabled_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_enabled_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(1u));

  // Parse a response indicating that the user can access Google Photos data.
  response.SetByDottedPath("status.userState", "USER_PERMITTED");
  result = GooglePhotosEnablementState::kEnabled;
  EXPECT_EQ(google_photos_enabled_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_enabled_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(1u));
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       ParsePhotosAbsentPhoto) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_photos_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosPhotosFetcher>*>(
      delegate()->SetGooglePhotosPhotosFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosPhotosFetcher>>(profile())));

  // Parse an absent response (simulating a fetching error).
  auto result = FetchGooglePhotosPhotosResponse::New();
  EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(nullptr), result);
  EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
            absl::nullopt);

  // Parse a response with no resume token or photos.
  base::Value::Dict empty_response;
  EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&empty_response),
            result);
  EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
            absl::nullopt);

  // Parse a response with a resume token and no photos.
  auto response = JsonToDict(kGooglePhotosResumeTokenOnlyResponse);
  result = FetchGooglePhotosPhotosResponse::New(absl::nullopt, kResumeToken);
  EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
            absl::optional<size_t>());
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       ParsePhotosInvalidPhoto) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_photos_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosPhotosFetcher>*>(
      delegate()->SetGooglePhotosPhotosFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosPhotosFetcher>>(profile())));

  auto result = FetchGooglePhotosPhotosResponse::New(
      std::vector<GooglePhotosPhotoPtr>(), kResumeToken);

  // Parse one-photo responses where one of the photo's fields is missing.
  for (const auto* const path : {"itemId.mediaKey", "filename",
                                 "creationTimestamp", "photo.servingUrl"}) {
    auto response = JsonToDict(kGooglePhotosPhotosFullResponse);
    auto* photo = GetPhotoFromGooglePhotosPhotosResponse(&response);
    photo->RemoveByDottedPath(path);
    EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&response), result);
    EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
              absl::make_optional<size_t>(0u));
  }

  // Parse one-photo responses where one of the photo's fields has an invalid
  // value.
  std::vector<std::pair<std::string, std::string>> invalid_field_test_cases = {
      {"creationTimestamp", ""},
      {"creationTimestamp", "Bad timestamp"},
      {"creationTimestamp", "2021T07:07:07.000Z"},
      {"creationTimestamp", "31T07:07:07.000Z"},
      {"creationTimestamp", "12T07:07:07.000Z"},
      {"creationTimestamp", "12-31T07:07:07.000Z"},
      {"creationTimestamp", "2021-31T07:07:07.000Z"},
      {"creationTimestamp", "2021-12T07:07:07.000Z"},
      {"creationTimestamp", "-2021-12-31T07:07:07.000Z"}};
  EXPECT_CALL(*google_photos_photos_fetcher, ParseResponse).Times(9);
  for (const auto& kv : invalid_field_test_cases) {
    auto response = JsonToDict(kGooglePhotosPhotosFullResponse);
    auto* photo = GetPhotoFromGooglePhotosPhotosResponse(&response);
    photo->SetByDottedPath(kv.first, kv.second);
    EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&response), result);
    EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
              absl::make_optional<size_t>(0u));
  }
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       ParsePhotosValidPhoto) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhoto;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  // Mock a fetcher to parse constructed responses.
  auto* const google_photos_photos_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosPhotosFetcher>*>(
      delegate()->SetGooglePhotosPhotosFetcherForTest(
          std::make_unique<::testing::NiceMock<
              wallpaper_handlers::MockGooglePhotosPhotosFetcher>>(profile())));

  // Ensure that photo timestamps resolve to the same date on all testing
  // platforms.
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("UTC"));

  // Parse a response with a valid photo and a resume token.
  auto valid_photos_vector = std::vector<GooglePhotosPhotoPtr>();
  valid_photos_vector.push_back(GooglePhotosPhoto::New(
      "photoId", "photoName", u"Friday, December 31, 2021",
      GURL("https://www.google.com/"), "home"));
  auto response = JsonToDict(kGooglePhotosPhotosFullResponse);
  auto result = FetchGooglePhotosPhotosResponse::New(
      mojo::Clone(valid_photos_vector), kResumeToken);
  EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(valid_photos_vector.size()));

  // Parse a response with a valid photo and no resume token.
  response.Remove("resumeToken");
  result = FetchGooglePhotosPhotosResponse::New(
      mojo::Clone(valid_photos_vector), absl::nullopt);
  EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(valid_photos_vector.size()));

  // Parse a response with a single valid photo not in a list.
  response = JsonToDict(kGooglePhotosPhotosSingleItemResponse);
  EXPECT_EQ(google_photos_photos_fetcher->ParseResponse(&response), result);
  EXPECT_EQ(google_photos_photos_fetcher->GetResultCount(result),
            absl::make_optional<size_t>(valid_photos_vector.size()));

  // Parse a response with a valid photo and no location.
  auto valid_photos_vector_without_location =
      std::vector<GooglePhotosPhotoPtr>();
  valid_photos_vector_without_location.push_back(GooglePhotosPhoto::New(
      "photoId", "photoName", u"Friday, December 31, 2021",
      GURL("https://www.google.com/"), absl::nullopt));
  auto* name_list = response.FindListByDottedPath("item.locationFeature.name");
  EXPECT_FALSE(name_list->empty());
  name_list->clear();
  EXPECT_EQ(FetchGooglePhotosPhotosResponse::New(
                std::move(valid_photos_vector_without_location), absl::nullopt),
            google_photos_photos_fetcher->ParseResponse(&response));
}

TEST_P(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosPhoto) {
  test_wallpaper_controller()->ClearCounts();
  const std::string photo_id = "OmnisVirLupus";
  bool feature_enabled = GooglePhotosEnabled();

  // Test selecting a wallpaper before fetching the enterprise setting.
  wallpaper_provider_remote()->get()->SelectGooglePhotosPhoto(
      photo_id, ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false,
      base::BindLambdaForTesting([](bool success) { EXPECT_FALSE(success); }));
  wallpaper_provider_remote()->FlushForTesting();

  EXPECT_EQ(0,
            test_wallpaper_controller()->set_google_photos_wallpaper_count());
  EXPECT_NE(
      ash::WallpaperInfo(
          {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
           photo_id, ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
           /*preview_mode=*/false}),
      test_wallpaper_controller()->wallpaper_info().value_or(
          ash::WallpaperInfo()));

  // Test selecting a wallpaper after fetching the enterprise setting.
  FetchGooglePhotosEnabled();
  wallpaper_provider_remote()->get()->SelectGooglePhotosPhoto(
      photo_id, ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false,
      base::BindLambdaForTesting([feature_enabled](bool success) {
        EXPECT_EQ(success, feature_enabled);
      }));
  wallpaper_provider_remote()->FlushForTesting();

  EXPECT_EQ(feature_enabled ? 1 : 0,
            test_wallpaper_controller()->set_google_photos_wallpaper_count());
  EXPECT_EQ(
      feature_enabled,
      ash::WallpaperInfo(
          {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
           photo_id, ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
           /*preview_mode=*/false}) ==
          test_wallpaper_controller()->wallpaper_info().value_or(
              ash::WallpaperInfo()));
}

}  // namespace personalization_app
}  // namespace ash
