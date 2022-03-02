// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_UPDATE_REQUIRED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_UPDATE_REQUIRED_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"

namespace chromeos {

class FakeUpdateRequiredScreenHandler : public UpdateRequiredView {
 public:
  FakeUpdateRequiredScreenHandler() = default;

  FakeUpdateRequiredScreenHandler(const FakeUpdateRequiredScreenHandler&) =
      delete;
  FakeUpdateRequiredScreenHandler& operator=(
      const FakeUpdateRequiredScreenHandler&) = delete;

  ~FakeUpdateRequiredScreenHandler() override {}

  UpdateRequiredView::UIState ui_state() { return ui_state_; }

 private:
  void Show() override {}
  void Hide() override {}
  void Bind(ash::UpdateRequiredScreen* screen) override {}
  void Unbind() override {}

  void SetIsConnected(bool connected) override {}
  void SetUpdateProgressUnavailable(bool unavailable) override {}
  void SetUpdateProgressValue(int progress) override {}
  void SetUpdateProgressMessage(const std::u16string& message) override {}
  void SetEstimatedTimeLeftVisible(bool visible) override {}
  void SetEstimatedTimeLeft(int seconds_left) override {}
  void SetUIState(UpdateRequiredView::UIState ui_state) override;
  void SetEnterpriseAndDeviceName(const std::string& enterpriseDomain,
                                  const std::u16string& deviceName) override {}
  void SetEolMessage(const std::string& eolMessage) override {}
  void SetIsUserDataPresent(bool data_present) override {}

  UpdateRequiredView::UIState ui_state_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_UPDATE_REQUIRED_SCREEN_HANDLER_H_
