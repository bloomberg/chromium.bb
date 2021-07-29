// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/resolution_notification_controller.h"

#include "ash/display/display_change_dialog.h"
#include "ash/display/display_util.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/display/display_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"

namespace ash {

class ResolutionNotificationControllerTest
    : public AshTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ResolutionNotificationControllerTest() : accept_count_(0) {}

  ~ResolutionNotificationControllerTest() override = default;

  std::u16string ExpectedNotificationMessage(int64_t display_id,
                                             const gfx::Size& new_resolution,
                                             float new_refresh_rate) {
    const std::u16string display_name =
        base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(display_id));
    const std::u16string countdown = ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
        base::TimeDelta::FromSeconds(15));
    if (::display::features::IsListAllDisplayModesEnabled()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_RESOLUTION_REFRESH_CHANGE_DIALOG_CHANGED, display_name,
          base::UTF8ToUTF16(new_resolution.ToString()),
          ConvertRefreshRateToString16(new_refresh_rate), countdown);
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_RESOLUTION_CHANGE_DIALOG_CHANGED, display_name,
        base::UTF8ToUTF16(new_resolution.ToString()), countdown);
  }

  std::u16string ExpectedFallbackNotificationMessage(
      int64_t display_id,
      const gfx::Size& specified_resolution,
      float specified_refresh_rate,
      const gfx::Size& fallback_resolution,
      float fallback_refresh_rate) {
    const std::u16string display_name =
        base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(display_id));
    const std::u16string countdown = ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
        base::TimeDelta::FromSeconds(15));
    if (::display::features::IsListAllDisplayModesEnabled()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_RESOLUTION_REFRESH_CHANGE_DIALOG_FALLBACK,
          {display_name, base::UTF8ToUTF16(specified_resolution.ToString()),
           ConvertRefreshRateToString16(specified_refresh_rate),
           base::UTF8ToUTF16(fallback_resolution.ToString()),
           ConvertRefreshRateToString16(fallback_refresh_rate), countdown},
          /*offsets=*/nullptr);
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_RESOLUTION_CHANGE_DIALOG_FALLBACK, display_name,
        base::UTF8ToUTF16(specified_resolution.ToString()),
        base::UTF8ToUTF16(fallback_resolution.ToString()), countdown);
  }

 protected:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          display::features::kListAllDisplayModes);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          display::features::kListAllDisplayModes);
    }
    AshTestBase::SetUp();
  }

  void SetDisplayResolutionAndNotifyWithResolution(
      const display::Display& display,
      const gfx::Size& new_resolution,
      const gfx::Size& actual_new_resolution,
      float new_refresh_rate,
      bool old_is_native,
      bool new_is_native,
      mojom::DisplayConfigSource source = mojom::DisplayConfigSource::kUser) {
    const display::ManagedDisplayInfo& info =
        display_manager()->GetDisplayInfo(display.id());
    display::ManagedDisplayMode old_mode(info.size_in_pixel(),
                                         info.refresh_rate(),
                                         false /* interlaced */, old_is_native);
    display::ManagedDisplayMode new_mode(
        new_resolution, new_refresh_rate, old_mode.is_interlaced(),
        new_is_native, old_mode.device_scale_factor());

    EXPECT_TRUE(controller()->PrepareNotificationAndSetDisplayMode(
        display.id(), old_mode, new_mode, source,
        base::BindOnce(&ResolutionNotificationControllerTest::OnAccepted,
                       base::Unretained(this))));

    // OnConfigurationChanged event won't be emitted in the test environment,
    // so invoke UpdateDisplay() to emit that event explicitly.
    std::vector<display::ManagedDisplayInfo> info_list;
    for (size_t i = 0; i < display_manager()->GetNumDisplays(); ++i) {
      int64_t id = display_manager()->GetDisplayAt(i).id();
      display::ManagedDisplayInfo info = display_manager()->GetDisplayInfo(id);
      if (display.id() == id) {
        gfx::Rect bounds = info.bounds_in_native();
        bounds.set_size(actual_new_resolution);
        info.SetBounds(bounds);
        info.set_refresh_rate(new_refresh_rate);
        info.set_native(new_is_native);
      }
      info_list.push_back(info);
    }
    display_manager()->OnNativeDisplaysChanged(info_list);
    base::RunLoop().RunUntilIdle();
  }

  void SetDisplayResolutionAndNotify(
      const display::Display& display,
      const gfx::Size& new_resolution,
      float refresh_rate,
      bool old_is_native,
      bool new_is_native,
      mojom::DisplayConfigSource source = mojom::DisplayConfigSource::kUser) {
    SetDisplayResolutionAndNotifyWithResolution(
        display, new_resolution, new_resolution, refresh_rate, old_is_native,
        new_is_native, source);
  }

  static std::u16string GetNotificationMessage() {
    return controller()->dialog_for_testing()->label_->GetText();
  }

  static void ClickOnNotification() {
    controller()->dialog_for_testing()->AcceptDialog();
  }


  static void CloseNotification() {
    controller()->dialog_for_testing()->AcceptDialog();
  }

  static bool IsNotificationVisible() {
    return controller()->dialog_for_testing() != nullptr;
  }

  static void TickTimer() { controller()->dialog_for_testing()->OnTimerTick(); }

  static ResolutionNotificationController* controller() {
    return Shell::Get()->resolution_notification_controller();
  }

  static void CancelNotification() {
    controller()->dialog_for_testing()->CancelDialog();
  }

  int accept_count() const { return accept_count_; }

 private:
  void OnAccepted() {
    accept_count_++;
  }

  int accept_count_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionNotificationControllerTest);
};

// Basic behaviors and verifies it doesn't cause crashes.
TEST_P(ResolutionNotificationControllerTest, Basic) {
  UpdateDisplay("300x300#300x300%57|200x200%58,250x250#250x250%60|200x200%59");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(display_manager_test.GetSecondaryDisplay(),
                                gfx::Size(200, 200), 59, /*old_is_native=*/true,
                                /*new_is_native=*/false);
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_EQ(ExpectedNotificationMessage(id2, gfx::Size(200, 200), 59),
            GetNotificationMessage());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(59.0, mode.refresh_rate());

  // Click the revert button, which reverts to the best resolution.
  CancelNotification();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size().ToString());
  EXPECT_EQ(60.0, mode.refresh_rate());
}

// Check that notification is not shown when changes are forced by policy.
TEST_P(ResolutionNotificationControllerTest, ForcedByPolicy) {
  UpdateDisplay("300x300#300x300%57|200x200%58,250x250#250x250%59|200x200%60");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(display_manager_test.GetSecondaryDisplay(),
                                gfx::Size(200, 200), 60, /*old_is_native=*/true,
                                /*new_is_native=*/false,
                                mojom::DisplayConfigSource::kPolicy);
  EXPECT_FALSE(IsNotificationVisible());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0, mode.refresh_rate());
}

TEST_P(ResolutionNotificationControllerTest, ClickMeansAccept) {
  UpdateDisplay("300x300#300x300%57|200x200%58,250x250#250x250%59|200x200%60");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(display_manager_test.GetSecondaryDisplay(),
                                gfx::Size(200, 200), 60, /*old_is_native=*/true,
                                /*new_is_native=*/false);
  EXPECT_TRUE(IsNotificationVisible());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0, mode.refresh_rate());

  ClickOnNotification();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0, mode.refresh_rate());
}

TEST_P(ResolutionNotificationControllerTest, AcceptButton) {
  UpdateDisplay("300x300#300x300%59|200x200%60");
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200), 60,
                                /*old_is_native=*/true,
                                /*new_is_native=*/false);
  EXPECT_TRUE(IsNotificationVisible());

  controller()->dialog_for_testing()->AcceptDialog();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());

  display::ManagedDisplayMode mode;
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display.id(), &mode));

  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());

  // In that case the second button is revert.
  UpdateDisplay("300x300#300x300%60|200x200%59");
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200), 59,
                                /*old_is_native=*/true,
                                /*new_is_native=*/false);
  EXPECT_TRUE(IsNotificationVisible());

  controller()->dialog_for_testing()->CancelDialog();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display.id(), &mode));

  EXPECT_EQ("300x300", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());
}

TEST_P(ResolutionNotificationControllerTest, Close) {
  UpdateDisplay("100x100,150x150#150x150%59|200x200%60");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      display_manager_test.GetSecondaryDisplay(), gfx::Size(200, 200), 60,
      /*old_is_native=*/false, /*new_is_native=*/true);
  EXPECT_TRUE(IsNotificationVisible());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());

  // Close the notification (imitates clicking [x] button). Also verifies if
  // this does not cause a crash.  See crbug.com/271784
  CloseNotification();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(1, accept_count());
}

TEST_P(ResolutionNotificationControllerTest, Timeout) {
  UpdateDisplay("300x300#300x300%60|200x200%60");
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200), 60,
                                /*old_is_native=*/true,
                                /*new_is_native=*/false);

  for (int i = 0; i < DisplayChangeDialog::kDefaultTimeoutInSeconds; ++i) {
    EXPECT_TRUE(IsNotificationVisible())
        << "notification is closed after " << i << "-th timer tick";
    TickTimer();
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display.id(), &mode));
  EXPECT_EQ("300x300", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());
}

TEST_P(ResolutionNotificationControllerTest, DisplayDisconnected) {
  UpdateDisplay(
      "300x300#300x300%56|200x200%57,"
      "200x200#250x250%58|200x200%60|100x100%60");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  SetDisplayResolutionAndNotify(
      display_manager_test.GetSecondaryDisplay(), gfx::Size(100, 100), 60,
      /*old_is_native=*/false, /*new_is_native=*/false);
  ASSERT_TRUE(IsNotificationVisible());

  // Disconnects the secondary display and verifies it doesn't cause crashes.
  UpdateDisplay("300x300#300x300%60|200x200%60");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());
}

// See http://crbug.com/869401 for details.
TEST_P(ResolutionNotificationControllerTest, MultipleResolutionChange) {
  UpdateDisplay(
      "300x300#300x300%56|200x200%57,"
      "250x250#250x250%58|200x200%59");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();

  SetDisplayResolutionAndNotify(display_manager_test.GetSecondaryDisplay(),
                                gfx::Size(200, 200), 59, /*old_is_native=*/true,
                                /*new_is_native=*/false);
  EXPECT_TRUE(IsNotificationVisible());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(59.0f, mode.refresh_rate());

  // Invokes SetDisplayResolutionAndNotify during the previous notification is
  // visible.
  SetDisplayResolutionAndNotify(
      display_manager_test.GetSecondaryDisplay(), gfx::Size(250, 250), 58,
      /*old_is_native=*/false, /*new_is_native=*/true);
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size().ToString());
  EXPECT_EQ(58.0f, mode.refresh_rate());

  // Then, click the revert button. Although |old_resolution| for the second
  // SetDisplayResolutionAndNotify is 200x200, it should revert to the original
  // size 250x250.
  CancelNotification();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size().ToString());
  EXPECT_EQ(58.0f, mode.refresh_rate());
}

TEST_P(ResolutionNotificationControllerTest, Fallback) {
  UpdateDisplay(
      "300x300#300x300%56|200x200%57,"
      "250x250#250x250%60|220x220%60|200x200%60");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotifyWithResolution(
      display_manager_test.GetSecondaryDisplay(), gfx::Size(220, 220),
      gfx::Size(200, 200), 60, /*old_is_native=*/true, /*new_is_native=*/false);
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_EQ(ExpectedFallbackNotificationMessage(id2, gfx::Size(220, 220), 60,
                                                gfx::Size(200, 200), 60),
            GetNotificationMessage());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());

  // Click the revert button, which reverts to the best resolution.
  CancelNotification();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_EQ(0, accept_count());

  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("250x250", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());
}

TEST_P(ResolutionNotificationControllerTest, NoTimeoutInKioskMode) {
  // Login in as kiosk app.
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_KIOSK_APP;
  session.user_info.account_id = AccountId::FromUserEmail("user1@test.com");
  session.user_info.display_name = "User 1";
  session.user_info.display_email = "user1@test.com";
  Shell::Get()->session_controller()->UpdateUserSession(std::move(session));
  EXPECT_EQ(LoginStatus::KIOSK_APP,
            Shell::Get()->session_controller()->login_status());

  UpdateDisplay("300x300#300x300%59|200x200%60");
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  SetDisplayResolutionAndNotify(display, gfx::Size(200, 200), 60,
                                /*old_is_native=*/true,
                                /*new_is_native=*/false);
}

TEST_P(ResolutionNotificationControllerTest, NoDialogInKioskMode) {
  // Login in as kiosk app.
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::USER_TYPE_KIOSK_APP;
  session.user_info.account_id = AccountId::FromUserEmail("user1@test.com");
  session.user_info.display_name = "User 1";
  session.user_info.display_email = "user1@test.com";
  Shell::Get()->session_controller()->UpdateUserSession(std::move(session));
  EXPECT_EQ(LoginStatus::KIOSK_APP,
            Shell::Get()->session_controller()->login_status());

  UpdateDisplay("100x100,150x150#150x150%59|200x200%60");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  ASSERT_EQ(0, accept_count());
  EXPECT_FALSE(IsNotificationVisible());

  // Changes the resolution and apply the result.
  SetDisplayResolutionAndNotify(
      display_manager_test.GetSecondaryDisplay(), gfx::Size(200, 200), 60,
      /*old_is_native=*/false, /*new_is_native=*/true);
  EXPECT_FALSE(IsNotificationVisible());
  display::ManagedDisplayMode mode;
  EXPECT_TRUE(display_manager()->GetSelectedModeForDisplayId(id2, &mode));
  EXPECT_EQ("200x200", mode.size().ToString());
  EXPECT_EQ(60.0f, mode.refresh_rate());
}

// Parametrizes all tests to run with display::features::kListAllDisplayModes
// enabled and disabled.
INSTANTIATE_TEST_SUITE_P(All,
                         ResolutionNotificationControllerTest,
                         ::testing::Bool());

}  // namespace ash
