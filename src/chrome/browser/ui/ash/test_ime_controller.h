// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_IME_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_TEST_IME_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ime_controller.h"
#include "ash/public/cpp/ime_info.h"

// Class that resets the ImeController instance to nullptr and then restores it
// when it is destroyed.
class ImeControllerResetterForTest {
 public:
  ImeControllerResetterForTest();
  ~ImeControllerResetterForTest();

 private:
  ash::ImeController* const instance_;
};

class TestImeController : private ImeControllerResetterForTest,
                          public ash::ImeController {
 public:
  TestImeController();
  ~TestImeController() override;

  // ash::ImeController:
  void SetClient(ash::ImeControllerClient* client) override;
  void RefreshIme(const std::string& current_ime_id,
                  std::vector<ash::ImeInfo> available_imes,
                  std::vector<ash::ImeMenuItem> menu_items) override;
  void SetImesManagedByPolicy(bool managed) override;
  void ShowImeMenuOnShelf(bool show) override;
  void UpdateCapsLockState(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;
  void SetExtraInputOptionsEnabledState(bool is_extra_input_options_enabled,
                                        bool is_emoji_enabled,
                                        bool is_handwriting_enabled,
                                        bool is_voice_enabled) override;
  void ShowModeIndicator(const gfx::Rect& anchor_bounds,
                         const base::string16& text) override;

  // The most recent values received via mojo.
  std::string current_ime_id_;
  std::vector<ash::ImeInfo> available_imes_;
  std::vector<ash::ImeMenuItem> menu_items_;
  bool managed_by_policy_ = false;
  bool show_ime_menu_on_shelf_ = false;
  bool show_mode_indicator_ = false;
  bool is_caps_lock_enabled_ = false;
  std::string keyboard_layout_name_;
  bool is_extra_input_options_enabled_ = false;
  bool is_emoji_enabled_ = false;
  bool is_handwriting_enabled_ = false;
  bool is_voice_enabled_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestImeController);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_IME_CONTROLLER_H_
