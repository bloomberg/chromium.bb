// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/view.h"

namespace views {
class EditableCombobox;
class ToggleImageButton;

#if defined(PASSWORD_STORE_SELECT_ENABLED)
class Checkbox;
#endif  // defined(PASSWORD_STORE_SELECT_ENABLED)
}  // namespace views

class PasswordSignInPromoView;

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|). Contains a username and password field, along with a
// "Save"/"Update" button and a "Never"/"Nope" button.
class PasswordSaveUpdateView : public PasswordBubbleViewBase,
                               public views::ButtonListener,
                               public views::EditableComboboxListener {
 public:
  PasswordSaveUpdateView(content::WebContents* web_contents,
                         views::View* anchor_view,
                         DisplayReason reason);

  views::View* GetUsernameTextfieldForTest() const;

 private:
  ~PasswordSaveUpdateView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::EditableComboboxListener:
  // Used for both the username and password editable comboboxes.
  void OnContentChanged(views::EditableCombobox* editable_combobox) override;

  // PasswordBubbleViewBase:
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetInitiallyFocusedView() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;

  // View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  void TogglePasswordVisibility();
  void UpdateUsernameAndPasswordInModel();
  void ReplaceWithPromo();
  void UpdateDialogButtons();
  std::unique_ptr<views::View> CreateFooterView();
  void OnDialogCancelled();

  SaveUpdateBubbleController controller_;

  // True iff it is an update password bubble on creation. False iff it is a
  // save bubble.
  const bool is_update_bubble_;

  // Different promo dialogs that helps the user get access to credentials
  // across devices. One of these are non-null when the promotion dialog is
  // active.
  PasswordSignInPromoView* sign_in_promo_;

  views::EditableCombobox* username_dropdown_;
  views::ToggleImageButton* password_view_button_;

  // The view for the password value.
  views::EditableCombobox* password_dropdown_;

#if defined(PASSWORD_STORE_SELECT_ENABLED)
  views::Checkbox* account_store_checkbox_ = nullptr;
#endif  // defined(PASSWORD_STORE_SELECT_ENABLED)

  bool are_passwords_revealed_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_VIEW_H_
