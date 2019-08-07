// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/reset_view.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

// WebUI implementation of ResetScreenActor.
class ResetScreenHandler : public ResetView,
                           public BaseScreenHandler {
 public:
  explicit ResetScreenHandler(JSCallsContainer* js_calls_container);
  ~ResetScreenHandler() override;

  // ResetView implementation:
  void Bind(ResetScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void Initialize() override;
  void SetIsRollbackAvailable(bool value) override;
  void SetIsRollbackChecked(bool value) override;
  void SetIsTpmFirmwareUpdateAvailable(bool value) override;
  void SetIsTpmFirmwareUpdateChecked(bool value) override;
  void SetIsTpmFirmwareUpdateEditable(bool value) override;
  void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) override;
  void SetIsConfirmational(bool value) override;
  void SetIsOfficialBuild(bool value) override;
  void SetScreenState(State value) override;
  State GetScreenState() override;
  tpm_firmware_update::Mode GetTpmFirmwareUpdateMode() override;
  bool GetIsRollbackAvailable() override;
  bool GetIsRollbackChecked() override;
  bool GetIsTpmFirmwareUpdateChecked() override;

 private:
  void HandleSetTpmFirmwareUpdateChecked(bool value);

  ResetScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;

  ResetView::State state_ = ResetView::State::kRestartRequired;
  tpm_firmware_update::Mode mode_ = tpm_firmware_update::Mode::kNone;
  bool is_rollback_available_ = false;
  bool is_rollback_checked_ = false;
  bool is_tpm_firmware_update_checked_ = false;

  DISALLOW_COPY_AND_ASSIGN(ResetScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
