// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_

#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

class Profile;

namespace chromeos {
namespace settings {

class AccessibilityHandler : public ::settings::SettingsPageUIHandler,
                             public ash::TabletModeObserver {
 public:
  explicit AccessibilityHandler(Profile* profile);
  ~AccessibilityHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callback which updates if startup sound is enabled and if tablet
  // mode is supported. Visible for testing.
  void HandleManageA11yPageReady(const base::ListValue* args);

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  // Callback for the messages to show settings for ChromeVox or
  // Select To Speak.
  void HandleShowChromeVoxSettings(const base::ListValue* args);
  void HandleShowSelectToSpeakSettings(const base::ListValue* args);
  void HandleSetStartupSoundEnabled(const base::ListValue* args);
  void HandleRecordSelectedShowShelfNavigationButtonsValue(
      const base::ListValue* args);

  void OpenExtensionOptionsPage(const char extension_id[]);

  ScopedObserver<ash::TabletMode, ash::TabletModeObserver>
      tablet_mode_observer_{this};

  Profile* profile_;  // Weak pointer.

  // Timer to record user changed value for the accessibility setting to turn
  // shelf navigation buttons on in tablet mode. The metric is recorded with 10
  // second delay to avoid overreporting when the user keeps toggling the
  // setting value in the screen UI.
  base::OneShotTimer a11y_nav_buttons_toggle_metrics_reporter_timer_;

  base::WeakPtrFactory<AccessibilityHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_
