// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

constexpr StaticOobeScreenId HIDDetectionView::kScreenId;

HIDDetectionScreenHandler::HIDDetectionScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.HIDDetectionScreen.userActed");
}

HIDDetectionScreenHandler::~HIDDetectionScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void HIDDetectionScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  int num_of_times_dialog_was_shown = local_state->GetInteger(
      prefs::kTimesHIDDialogShown);
  local_state->SetInteger(prefs::kTimesHIDDialogShown,
                          num_of_times_dialog_was_shown + 1);

  ShowScreen(kScreenId);
}

void HIDDetectionScreenHandler::Hide() {
}

void HIDDetectionScreenHandler::Bind(HIDDetectionScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
  if (page_is_ready())
    Initialize();
}

void HIDDetectionScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void HIDDetectionScreenHandler::CheckIsScreenRequired(
      const base::Callback<void(bool)>& on_check_done) {
  screen_->CheckIsScreenRequired(on_check_done);
}

void HIDDetectionScreenHandler::SetKeyboardState(const std::string& value) {
  keyboard_state_ = value;
  CallJS("login.HIDDetectionScreen.setKeyboardState", value);
}

void HIDDetectionScreenHandler::SetMouseState(const std::string& value) {
  mouse_state_ = value;
  CallJS("login.HIDDetectionScreen.setMouseState", value);
}

void HIDDetectionScreenHandler::SetKeyboardPinCode(const std::string& value) {
  keyboard_pin_code_ = value;
  CallJS("login.HIDDetectionScreen.setKeyboardPinCode", value);
}

void HIDDetectionScreenHandler::SetPinDialogVisible(bool value) {
  num_keys_entered_expected_ = value;
  CallJS("login.HIDDetectionScreen.setPinDialogVisible", value);
}

void HIDDetectionScreenHandler::SetNumKeysEnteredPinCode(int value) {
  num_keys_entered_pin_code_ = value;
  CallJS("login.HIDDetectionScreen.setNumKeysEnteredPinCode", value);
}

void HIDDetectionScreenHandler::SetPointingDeviceName(
    const std::string& value) {
  mouse_device_name_ = value;
  CallJS("login.HIDDetectionScreen.setPointingDeviceName", value);
}

void HIDDetectionScreenHandler::SetKeyboardDeviceName(
    const std::string& value) {
  keyboard_device_name_ = value;
  CallJS("login.HIDDetectionScreen.setKeyboardDeviceName", value);
}

void HIDDetectionScreenHandler::SetContinueButtonEnabled(bool value) {
  continue_button_enabled_ = value;
  CallJS("login.HIDDetectionScreen.setContinueButtonEnabled", value);
}

void HIDDetectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("hidDetectionContinue", IDS_HID_DETECTION_CONTINUE_BUTTON);
  builder->Add("hidDetectionInvitation", IDS_HID_DETECTION_INVITATION_TEXT);
  builder->Add("hidDetectionPrerequisites",
      IDS_HID_DETECTION_PRECONDITION_TEXT);
  builder->Add("hidDetectionMouseSearching", IDS_HID_DETECTION_SEARCHING_MOUSE);
  builder->Add("hidDetectionKeyboardSearching",
      IDS_HID_DETECTION_SEARCHING_KEYBOARD);
  builder->Add("hidDetectionUSBMouseConnected",
      IDS_HID_DETECTION_CONNECTED_USB_MOUSE);
  builder->Add("hidDetectionPointingDeviceConnected",
      IDS_HID_DETECTION_CONNECTED_POINTING_DEVICE);
  builder->Add("hidDetectionKeyboardPairing",
               IDS_HID_DETECTION_PAIRING_BLUETOOTH_KEYBOARD);
  builder->Add("hidDetectionUSBKeyboardConnected",
      IDS_HID_DETECTION_CONNECTED_USB_KEYBOARD);
  builder->Add("hidDetectionBTMousePaired",
      IDS_HID_DETECTION_PAIRED_BLUETOOTH_MOUSE);
  builder->Add("hidDetectionBTEnterKey", IDS_HID_DETECTION_BLUETOOTH_ENTER_KEY);
  builder->Add("hidDetectionPinDialogTitle",
               IDS_HID_DETECTION_PAIRING_BLUETOOTH_KEYBOARD_PIN_DIALOG_TITLE);
  builder->Add("hidDetectionBluetoothPairingCode",
               IDS_HID_DETECTION_BLUETOOTH_PAIRING_CODE);
  builder->Add("hidDetectionBluetoothPairingCodeExplanation",
               IDS_HID_DETECTION_BLUETOOTH_PAIRING_CODE_EXPLANATION);
  builder->Add("hidDetectionBluetoothKeyboardPaired",
               IDS_HID_DETECTION_PAIRED_BLUETOOTH_KEYBOARD);
  builder->Add("oobeModalDialogClose", IDS_CHROMEOS_OOBE_CLOSE_DIALOG);
}

void HIDDetectionScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

// static
void HIDDetectionScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kTimesHIDDialogShown, 0);
}

}  // namespace chromeos
