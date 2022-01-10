// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_

#include <memory>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class HIDDetectionScreen;
}

namespace chromeos {

// Interface between HID detection screen and its representation, either WebUI
// or Views one. Note, do not forget to call OnViewDestroyed in the
// dtor.
class HIDDetectionView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"hid-detection"};

  virtual ~HIDDetectionView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Bind(ash::HIDDetectionScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void SetKeyboardState(const std::string& value) = 0;
  virtual void SetMouseState(const std::string& value) = 0;
  virtual void SetTouchscreenDetectedState(bool value) = 0;
  virtual void SetKeyboardPinCode(const std::string& value) = 0;
  virtual void SetPinDialogVisible(bool value) = 0;
  virtual void SetNumKeysEnteredPinCode(int value) = 0;
  virtual void SetPointingDeviceName(const std::string& value) = 0;
  virtual void SetKeyboardDeviceName(const std::string& value) = 0;
  virtual void SetContinueButtonEnabled(bool value) = 0;
};

// WebUI implementation of HIDDetectionScreenView.
class HIDDetectionScreenHandler
    : public HIDDetectionView,
      public BaseScreenHandler {
 public:
  using TView = HIDDetectionView;

  explicit HIDDetectionScreenHandler(JSCallsContainer* js_calls_container);

  HIDDetectionScreenHandler(const HIDDetectionScreenHandler&) = delete;
  HIDDetectionScreenHandler& operator=(const HIDDetectionScreenHandler&) =
      delete;

  ~HIDDetectionScreenHandler() override;

  // HIDDetectionView implementation:
  void Show() override;
  void Hide() override;
  void Bind(ash::HIDDetectionScreen* screen) override;
  void Unbind() override;
  void SetKeyboardState(const std::string& value) override;
  void SetMouseState(const std::string& value) override;
  void SetTouchscreenDetectedState(bool value) override;
  void SetKeyboardPinCode(const std::string& value) override;
  void SetPinDialogVisible(bool value) override;
  void SetNumKeysEnteredPinCode(int value) override;
  void SetPointingDeviceName(const std::string& value) override;
  void SetKeyboardDeviceName(const std::string& value) override;
  void SetContinueButtonEnabled(bool value) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void Initialize() override;

  // Emulate that a USB Mouse and a USB Keyboard are connected for testing.
  void HandleEmulateDevicesConnectedForTesting();

  // State that has been exported to JS. Used by tests.
  std::string keyboard_state_for_test() const { return keyboard_state_; }
  std::string mouse_state_for_test() const { return mouse_state_; }
  std::string keyboard_pin_code_for_test() const { return keyboard_pin_code_; }
  bool num_keys_entered_expected_for_test() const {
    return num_keys_entered_expected_;
  }
  int num_keys_entered_pin_code_for_test() const {
    return num_keys_entered_pin_code_;
  }
  std::string mouse_device_name_for_test() const { return mouse_device_name_; }
  std::string keyboard_device_name_for_test() const {
    return keyboard_device_name_;
  }
  std::string keyboard_device_label_for_test() const {
    return keyboard_device_label_;
  }
  bool continue_button_enabled_for_test() const {
    return continue_button_enabled_;
  }

 private:
  // Cached values that have been sent to JS. Used by tests.
  std::string keyboard_state_;
  std::string mouse_state_;
  std::string keyboard_pin_code_;
  bool num_keys_entered_expected_ = false;
  int num_keys_entered_pin_code_ = 0;
  std::string mouse_device_name_;
  std::string keyboard_device_name_;
  std::string keyboard_device_label_;
  bool continue_button_enabled_ = false;

  ash::HIDDetectionScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::HIDDetectionScreenHandler;
using ::chromeos::HIDDetectionView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_
