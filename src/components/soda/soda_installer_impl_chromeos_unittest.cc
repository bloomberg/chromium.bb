// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const speech::LanguageCode kEnglishLocale = speech::LanguageCode::kEnUs;
const base::TimeDelta kSodaUninstallTime = base::Days(30);
}  // namespace

namespace speech {

class SodaInstallerImplChromeOSTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOnDeviceSpeechRecognition);
    soda_installer_impl_ = std::make_unique<SodaInstallerImplChromeOS>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    soda_installer_impl_->RegisterLocalStatePrefs(pref_service_->registry());
    // Set Dictation pref to true so that SODA will download when calling
    // Init().
    pref_service_->registry()->RegisterBooleanPref(
        ash::prefs::kAccessibilityDictationEnabled, true);
    pref_service_->registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                   true);
    pref_service_->registry()->RegisterBooleanPref(
        ash::prefs::kProjectorCreationFlowEnabled, true);
    pref_service_->registry()->RegisterStringPref(
        ash::prefs::kProjectorCreationFlowLanguage, kUsEnglishLocale);
    pref_service_->registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, kUsEnglishLocale);

    chromeos::DBusThreadManager::Initialize();
    chromeos::DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    pref_service_.reset();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::DlcserviceClient::Shutdown();
  }

  SodaInstallerImplChromeOS* GetInstance() {
    return soda_installer_impl_.get();
  }

  bool IsSodaInstalled() {
    return soda_installer_impl_->IsSodaInstalled(kEnglishLocale);
  }

  bool IsLanguageInstalled(LanguageCode language) {
    return soda_installer_impl_->IsLanguageInstalled(language);
  }

  bool IsAnyLanguagePackInstalled() {
    return soda_installer_impl_->IsAnyLanguagePackInstalled();
  }

  bool IsSodaDownloading() {
    return soda_installer_impl_->IsSodaDownloading(kEnglishLocale);
  }

  void Init() {
    soda_installer_impl_->Init(pref_service_.get(), pref_service_.get());
  }

  void InstallLanguage(const std::string& language) {
    soda_installer_impl_->InstallLanguage(language, pref_service_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SetInstallError() {
    fake_dlcservice_client_->set_install_error(dlcservice::kErrorNeedReboot);
  }

  void SetUninstallTimer() {
    soda_installer_impl_->SetUninstallTimer(pref_service_.get(),
                                            pref_service_.get());
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetDictationEnabled(bool enabled) {
    pref_service_->SetManagedPref(ash::prefs::kAccessibilityDictationEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetLiveCaptionEnabled(bool enabled) {
    pref_service_->SetManagedPref(prefs::kLiveCaptionEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetProjectorCreationFlowEnabled(bool enabled) {
    pref_service_->SetManagedPref(ash::prefs::kProjectorCreationFlowEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetSodaInstallerInitialized(bool initialized) {
    soda_installer_impl_->soda_installer_initialized_ = initialized;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<SodaInstallerImplChromeOS> soda_installer_impl_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  chromeos::FakeDlcserviceClient* fake_dlcservice_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SodaInstallerImplChromeOSTest, IsSodaInstalled) {
  ASSERT_FALSE(IsSodaInstalled());
  Init();
  ASSERT_FALSE(IsSodaInstalled());
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
}

TEST_F(SodaInstallerImplChromeOSTest, IsDownloading) {
  ASSERT_FALSE(IsSodaDownloading());
  Init();
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, IsAnyLanguagePackInstalled) {
  ASSERT_FALSE(IsAnyLanguagePackInstalled());
  Init();
  ASSERT_FALSE(IsAnyLanguagePackInstalled());
  RunUntilIdle();
  ASSERT_TRUE(IsAnyLanguagePackInstalled());
}

TEST_F(SodaInstallerImplChromeOSTest, SodaInstallError) {
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  SetInstallError();
  Init();
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, LanguagePackError) {
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
  SetInstallError();
  Init();
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, InstallSodaForTesting) {
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
  GetInstance()->NotifySodaInstalledForTesting();
  ASSERT_TRUE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  ASSERT_TRUE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, UninstallSodaForTesting) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  ASSERT_TRUE(IsLanguageInstalled(kEnglishLocale));
  GetInstance()->UninstallSodaForTesting();
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
}

TEST_F(SodaInstallerImplChromeOSTest, SodaProgressForTesting) {
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  Init();
  GetInstance()->NotifySodaDownloadProgressForTesting(50);
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsAnyLanguagePackInstalled());
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
}

TEST_F(SodaInstallerImplChromeOSTest, LanguagePackForTesting) {
  LanguageCode fr_fr = LanguageCode::kFrFr;
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifyOnSodaLanguagePackProgressForTesting(50, fr_fr);
  ASSERT_TRUE(GetInstance()->IsSodaDownloading(fr_fr));
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifyOnSodaLanguagePackInstalledForTesting(fr_fr);
  ASSERT_TRUE(IsLanguageInstalled(fr_fr));
}

TEST_F(SodaInstallerImplChromeOSTest, LanguagePackErrorForTesting) {
  LanguageCode fr_fr = LanguageCode::kFrFr;
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifyOnSodaLanguagePackProgressForTesting(50, fr_fr);
  ASSERT_TRUE(GetInstance()->IsSodaDownloading(fr_fr));
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifyOnSodaLanguagePackErrorForTesting(fr_fr);
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  ASSERT_FALSE(GetInstance()->IsSodaDownloading(fr_fr));
}

TEST_F(SodaInstallerImplChromeOSTest, UninstallSodaAfterThirtyDays) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  // Turn off features that use SODA so that the uninstall timer can be set.
  SetDictationEnabled(false);
  SetLiveCaptionEnabled(false);
  SetProjectorCreationFlowEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());
  // If 30 days pass without the uninstall time being pushed, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());
  // The uninstallation process doesn't start until Init() is called again.
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsSodaInstalled());
}

TEST_F(SodaInstallerImplChromeOSTest, ReinstallSoda) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  // Turn off features that use SODA so that the uninstall timer can be set.
  SetDictationEnabled(false);
  SetLiveCaptionEnabled(false);
  SetProjectorCreationFlowEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());
  // If 30 days pass without the uninstall time being pushed, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());
  // The uninstallation process doesn't start until Init() is called again.
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsSodaInstalled());
  // Enable live caption and reinstall SODA.
  SetLiveCaptionEnabled(true);
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
}

// Tests that SODA stays installed if thirty days pass and a feature using SODA
// is enabled.
TEST_F(SodaInstallerImplChromeOSTest,
       SodaStaysInstalledAfterThirtyDaysIfFeatureEnabled) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  // Turn off Dictation, but keep live caption enabled. This should prevent
  // SODA from automatically uninstalling.
  SetDictationEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
}

}  // namespace speech
