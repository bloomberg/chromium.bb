// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_

#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/soda/soda_installer.h"

namespace base {
class ListValue;
}

class Profile;

namespace chromeos {
namespace settings {

class AccessibilityHandler : public ::settings::SettingsPageUIHandler,
                             public speech::SodaInstaller::Observer {
 public:
  explicit AccessibilityHandler(Profile* profile);

  AccessibilityHandler(const AccessibilityHandler&) = delete;
  AccessibilityHandler& operator=(const AccessibilityHandler&) = delete;

  ~AccessibilityHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callback which updates if startup sound is enabled. Visible for testing.
  void HandleManageA11yPageReady(const base::ListValue* args);

 private:
  friend class AccessibilityHandlerTest;

  // Callback for the messages to show settings for ChromeVox or
  // Select To Speak.
  void HandleShowChromeVoxSettings(const base::ListValue* args);
  void HandleShowSelectToSpeakSettings(const base::ListValue* args);
  void HandleSetStartupSoundEnabled(const base::ListValue* args);
  void HandleRecordSelectedShowShelfNavigationButtonsValue(
      const base::ListValue* args);
  void HandleShowChromeVoxTutorial(const base::ListValue* args);

  void OpenExtensionOptionsPage(const char extension_id[]);

  void MaybeAddSodaInstallerObserver();
  void OnSodaInstallSucceeded();
  void OnSodaInstallProgress(int progress, speech::LanguageCode language_code);
  void OnSodaInstallFailed(speech::LanguageCode language_code);

  // SodaInstaller::Observer:
  void OnSodaInstalled() override;
  void OnSodaLanguagePackInstalled(speech::LanguageCode language_code) override;
  void OnSodaProgress(int progress) override {}
  void OnSodaLanguagePackProgress(int language_progress,
                                  speech::LanguageCode language_code) override;
  void OnSodaError() override;
  void OnSodaLanguagePackError(speech::LanguageCode language_code) override;

  void MaybeAddDictationLocales();
  speech::LanguageCode GetDictationLocale();
  std::u16string GetDictationLocaleDisplayName();

  Profile* profile_;  // Weak pointer.

  // Timer to record user changed value for the accessibility setting to turn
  // shelf navigation buttons on in tablet mode. The metric is recorded with 10
  // second delay to avoid overreporting when the user keeps toggling the
  // setting value in the screen UI.
  base::OneShotTimer a11y_nav_buttons_toggle_metrics_reporter_timer_;

  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      soda_observation_{this};

  base::WeakPtrFactory<AccessibilityHandler> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_
