// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/login/ui/login_palette.h"
#include "ash/public/cpp/session/user_info.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Textfield;
class ToggleImageButton;
}  // namespace views

namespace ash {
class ArrowButtonView;
enum class EasyUnlockIconId;

// Contains a textfield and a submit button. When the display password button
// is visible, the textfield contains a button in the form of an eye icon that
// the user can click on to reveal the password. Submitting a password will
// make it read only and prevent further submissions until the controller sets
// ReadOnly to false again.
//
// This view is always rendered via layers.
//
//
// When the display password button is hidden, the password view looks
// like this:
//
// * * * * * *         (=>)
// ------------------
//
// When the display password button is visible, the password view looks
// like this by default:
//
//  * * * * * *    (\)  (=>)
//  ------------------
//
//  or this, in display mode:
//
//  1 2 3 4 5 6    (o)  (=>)
//  ------------------
class ASH_EXPORT LoginPasswordView : public views::View,
                                     public views::TextfieldController,
                                     public ImeControllerImpl::Observer {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPasswordView* view);
    ~TestApi();

    void SubmitPassword(const std::string& password);

    views::Textfield* textfield() const;
    views::View* submit_button() const;
    views::ToggleImageButton* display_password_button() const;
    views::View* easy_unlock_icon() const;
    views::View* capslock_icon() const;
    void set_immediately_hover_easy_unlock_icon();

    bool is_capslock_highlight_for_testing() {
      return view_->is_capslock_higlight_;
    }

   private:
    LoginPasswordView* view_;
  };

  using OnPasswordSubmit =
      base::RepeatingCallback<void(const std::u16string& password)>;
  using OnPasswordTextChanged = base::RepeatingCallback<void(bool is_empty)>;
  using OnEasyUnlockIconHovered = base::RepeatingClosure;

  // Must call |Init| after construction.
  explicit LoginPasswordView(const LoginPalette& palette);
  ~LoginPasswordView() override;

  // |on_submit| is called when the user hits enter or has pressed the submit
  // arrow.
  // |on_password_text_changed| is called when the text in the password field
  // changes.
  void Init(const OnPasswordSubmit& on_submit,
            const OnPasswordTextChanged& on_password_text_changed,
            const OnEasyUnlockIconHovered& on_easy_unlock_icon_hovered,
            views::Button::PressedCallback on_easy_unlock_icon_tapped);

  // Is the password field enabled when there is no text?
  void SetEnabledOnEmptyPassword(bool enabled);

  // Change the active icon for easy unlock.
  void SetEasyUnlockIcon(EasyUnlockIconId id,
                         const std::u16string& accessibility_label);

  // Set the textfield name used for accessibility.
  void SetAccessibleName(const std::u16string& name);

  // Enable or disable focus on the child elements (i.e.: password field and
  // submit button, or display password button if it is shown).
  void SetFocusEnabledForTextfield(bool enable);

  // Sets whether the display password button is visible.
  void SetDisplayPasswordButtonVisible(bool visible);

  // Clear the text and put the password into hide mode.
  void Reset();

  // Clear all currently entered text.
  void Clear();

  // Inserts the given numeric value to the textfield at the current cursor
  // position (most likely the end).
  void InsertNumber(int value);

  // Erase the last entered value.
  void Backspace();

  // Set password field placeholder. The password view cannot set the text by
  // itself because it doesn't know which auth methods are enabled.
  void SetPlaceholderText(const std::u16string& placeholder_text);

  // Makes the textfield read-only and enables/disables submitting.
  void SetReadOnly(bool read_only);

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Invert the textfield type and toggle the display password button.
  void InvertPasswordDisplayingState();

  // Hides the password. When |chromevox_exception| is true, the password is not
  // hidden if ChromeVox is enabled.
  void HidePassword(bool chromevox_exception);

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // ImeControllerImpl::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

  void HandleLeftIconsVisibilities(bool handling_capslock);

  // Submits the current password field text to mojo call and resets the text
  // field.
  void SubmitPassword();

  // When theme changes, palette should be updated and some subviews
  // recalculated.
  void UpdatePalette(const LoginPalette& palette);

 private:
  class EasyUnlockIcon;
  class DisplayPasswordButton;
  class LoginPasswordRow;
  class LoginTextfield;
  class AlternateIconsView;
  friend class TestApi;

  // Increases/decreases the contrast of the capslock icon.
  void SetCapsLockHighlighted(bool highlight);

  // Highlight or remove highlight from password row.
  void SetPasswordRowHighlighted(bool highlight);

  // Remove highlight from caps lock and password row, when textfield looses
  // focus.
  void RemoveHighlightFromCapsLockAndRow();

  // Needs to be true in order for SubmitPassword to be ran. Returns true if the
  // textfield is not empty or if |enabled_on_empty_password| is true.
  bool IsPasswordSubmittable();

  // UpdateUiState enables/disables the submit button, and the display password
  // button when it is visible.
  void UpdateUiState();

  OnPasswordSubmit on_submit_;
  OnPasswordTextChanged on_password_text_changed_;

  // Is the password field enabled when there is no text?
  bool enabled_on_empty_password_ = false;

  // Clears the password field after a time without action if the display
  // password button is visible.
  std::unique_ptr<base::RetainingOneShotTimer> clear_password_timer_;

  // Hides the password after a short delay if the password is shown, except if
  // ChromeVox is enabled (otherwise, the user would not have time to navigate
  // through the password and make the characters read out loud one by one).
  std::unique_ptr<base::RetainingOneShotTimer> hide_password_timer_;

  LoginPalette palette_;

  LoginPasswordRow* password_row_ = nullptr;
  LoginTextfield* textfield_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;
  DisplayPasswordButton* display_password_button_ = nullptr;
  // Could show either the caps lock icon or the easy unlock icon.
  AlternateIconsView* left_icon_ = nullptr;
  views::ImageView* capslock_icon_ = nullptr;
  bool should_show_capslock_ = false;
  EasyUnlockIcon* easy_unlock_icon_ = nullptr;
  bool should_show_easy_unlock_ = false;

  bool is_capslock_higlight_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginPasswordView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
