// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";

class TestThemeObserver
    : public ash::personalization_app::mojom::ThemeObserver {
 public:
  void OnColorModeChanged(bool dark_mode_enabled) override {
    dark_mode_enabled_ = dark_mode_enabled;
  }

  mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
  pending_remote() {
    DCHECK(!theme_observer_receiver_.is_bound());
    return theme_observer_receiver_.BindNewPipeAndPassRemote();
  }

  absl::optional<bool> is_dark_mode_enabled() {
    if (!theme_observer_receiver_.is_bound())
      return absl::nullopt;

    theme_observer_receiver_.FlushForTesting();
    return dark_mode_enabled_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::ThemeObserver>
      theme_observer_receiver_{this};

  bool dark_mode_enabled_ = false;
};

}  // namespace

class PersonalizationAppThemeProviderImplTest : public ChromeAshTestBase {
 public:
  PersonalizationAppThemeProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures({ash::features::kPersonalizationHub,
                                           chromeos::features::kDarkLightMode},
                                          {});
  }
  PersonalizationAppThemeProviderImplTest(
      const PersonalizationAppThemeProviderImplTest&) = delete;
  PersonalizationAppThemeProviderImplTest& operator=(
      const PersonalizationAppThemeProviderImplTest&) = delete;
  ~PersonalizationAppThemeProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    ash::AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
        ash::Shell::Get()->session_controller()->GetActivePrefService());

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    theme_provider_ =
        std::make_unique<PersonalizationAppThemeProviderImpl>(&web_ui_);
    theme_provider_->BindInterface(
        theme_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    theme_provider_.reset();
    ChromeAshTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::ThemeProvider>*
  theme_provider_remote() {
    return &theme_provider_remote_;
  }

  PersonalizationAppThemeProviderImpl* theme_provider() {
    return theme_provider_.get();
  }

  void SetThemeObserver() {
    theme_provider_remote_->SetThemeObserver(
        test_theme_observer_.pending_remote());
  }

  absl::optional<bool> is_dark_mode_enabled() {
    theme_provider_remote_.FlushForTesting();
    return test_theme_observer_.is_dark_mode_enabled();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::ThemeProvider>
      theme_provider_remote_;
  TestThemeObserver test_theme_observer_;
  std::unique_ptr<PersonalizationAppThemeProviderImpl> theme_provider_;
};

TEST_F(PersonalizationAppThemeProviderImplTest, SetColorModePref) {
  SetThemeObserver();
  theme_provider()->SetColorModePref(/*dark_mode_enabled=*/false);
  EXPECT_FALSE(is_dark_mode_enabled().value());

  theme_provider()->SetColorModePref(/*dark_mode_enabled=*/true);
  EXPECT_TRUE(is_dark_mode_enabled().value());
}

TEST_F(PersonalizationAppThemeProviderImplTest, OnColorModeChanged) {
  SetThemeObserver();

  bool dark_mode_enabled = ash::AshColorProvider::Get()->IsDarkModeEnabled();
  ash::AshColorProvider::Get()->ToggleColorMode();
  EXPECT_NE(is_dark_mode_enabled().value(), dark_mode_enabled);

  ash::AshColorProvider::Get()->ToggleColorMode();
  EXPECT_EQ(is_dark_mode_enabled().value(), dark_mode_enabled);
}
