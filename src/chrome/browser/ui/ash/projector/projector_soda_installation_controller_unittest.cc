// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "projector_soda_installation_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

inline const void SetLocale(const std::string& locale) {
  g_browser_process->SetApplicationLocale(locale);
}

// A mocked version instance of SodaInstaller for testing purposes.
class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller() = default;
  MockSodaInstaller(const MockSodaInstaller&) = delete;
  MockSodaInstaller& operator=(const MockSodaInstaller&) = delete;
  ~MockSodaInstaller() override = default;

  MOCK_CONST_METHOD0(GetSodaBinaryPath, base::FilePath());
  MOCK_CONST_METHOD1(GetLanguagePath, base::FilePath(const std::string&));
  MOCK_METHOD2(InstallLanguage, void(const std::string&, PrefService*));
  MOCK_CONST_METHOD0(GetAvailableLanguages, std::vector<std::string>());
  MOCK_METHOD1(InstallSoda, void(PrefService*));
  MOCK_METHOD1(UninstallSoda, void(PrefService*));
};

const char kEnglishLocale[] = "en-US";

}  // namespace

namespace ash {

class ProjectorSodaInstallationControllerTest : public testing::Test {
 public:
  ProjectorSodaInstallationControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kOnDeviceSpeechRecognition,
            features::kProjector,
        },
        {});
  }
  ProjectorSodaInstallationControllerTest(
      const ProjectorSodaInstallationControllerTest&) = delete;
  ProjectorSodaInstallationControllerTest& operator=(
      const ProjectorSodaInstallationControllerTest&) = delete;
  ~ProjectorSodaInstallationControllerTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ = ProfileManager::GetPrimaryUserProfile();

    soda_installer_ = std::make_unique<MockSodaInstaller>();

    mock_app_client_ = std::make_unique<MockAppClient>();

    mock_projector_controller_ = std::make_unique<MockProjectorController>();

    soda_installation_controller_ =
        std::make_unique<ProjectorSodaInstallationController>(
            mock_app_client_.get(), mock_projector_controller_.get());
  }

  void TearDown() override {
    soda_installation_controller_.reset();
    mock_projector_controller_.reset();
    mock_app_client_.reset();
    soda_installer_.reset();
  }

  MockAppClient& app_client() { return *mock_app_client_; }
  MockProjectorController& projector_controller() {
    return *mock_projector_controller_;
  }

  ProjectorSodaInstallationController* soda_installation_controller() {
    return soda_installation_controller_.get();
  }

  MockSodaInstaller* soda_installer() { return soda_installer_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  Profile* testing_profile_ = nullptr;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  std::unique_ptr<MockSodaInstaller> soda_installer_;
  std::unique_ptr<MockAppClient> mock_app_client_;
  std::unique_ptr<MockProjectorController> mock_projector_controller_;

  std::unique_ptr<ProjectorSodaInstallationController>
      soda_installation_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorSodaInstallationControllerTest, ShouldDownloadSoda) {
  ON_CALL(*soda_installer(), GetAvailableLanguages)
      .WillByDefault(
          testing::Return(std::vector<std::string>({kEnglishLocale})));

  EXPECT_TRUE(soda_installation_controller()->ShouldDownloadSoda(
      speech::LanguageCode::kEnUs));

  // Other languages other than English are not currently supported.
  EXPECT_FALSE(soda_installation_controller()->ShouldDownloadSoda(
      speech::LanguageCode::kFrFr));
}

TEST_F(ProjectorSodaInstallationControllerTest, IsSpeechRecognitionAvailable) {
  SetLocale(kEnglishLocale);
  EXPECT_FALSE(soda_installation_controller()->IsSodaAvailable(
      speech::LanguageCode::kEnUs));

  EXPECT_CALL(app_client(), OnSodaInstalled()).Times(1);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_TRUE(soda_installation_controller()->IsSodaAvailable(
      speech::LanguageCode::kEnUs));
  EXPECT_FALSE(soda_installation_controller()->IsSodaAvailable(
      speech::LanguageCode::kFrFr));
}

TEST_F(ProjectorSodaInstallationControllerTest, InstallSoda) {
  SetLocale(kEnglishLocale);
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      prefs::kProjectorCreationFlowEnabled, true);

  // Test case where SODA is already installed.
  soda_installation_controller()->InstallSoda(kEnglishLocale);

  EXPECT_CALL(app_client(), OnSodaInstalled()).Times(1);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
}

TEST_F(ProjectorSodaInstallationControllerTest, OnSodaInstallProgress) {
  SetLocale(kEnglishLocale);
  EXPECT_CALL(app_client(), OnSodaInstallProgress(50)).Times(1);
  speech::SodaInstaller::GetInstance()->NotifySodaDownloadProgressForTesting(
      50);
}

TEST_F(ProjectorSodaInstallationControllerTest, OnSodaInstallError) {
  SetLocale(kEnglishLocale);
  EXPECT_CALL(app_client(), OnSodaInstallError()).Times(1);
  EXPECT_CALL(projector_controller(),
              OnSpeechRecognitionAvailabilityChanged(
                  ash::SpeechRecognitionAvailability::kSodaInstallationError))
      .Times(1);
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting();
}

}  // namespace ash
