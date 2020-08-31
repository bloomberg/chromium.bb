// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class PrefRegistrySimple;

namespace ash {

class CollapseButton;
class SignOutButton;
class TopShortcutButton;
class TopShortcutsViewTest;
class UnifiedSystemTrayController;

// Container for the top shortcut buttons. The view may narrow gaps between
// buttons when there's not enough space. When those doesn't fit in the view
// even after that, the sign-out button will be resized.
class TopShortcutButtonContainer : public views::View {
 public:
  TopShortcutButtonContainer();
  ~TopShortcutButtonContainer() override;

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

  void AddUserAvatarButton(views::View* user_avatar_button);
  // Add the sign-out button, which can be resized upon layout.
  void AddSignOutButton(views::View* sign_out_button);

 private:
  views::View* user_avatar_button_ = nullptr;
  views::View* sign_out_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TopShortcutButtonContainer);
};

// Top shortcuts view shown on the top of UnifiedSystemTrayView.
class ASH_EXPORT TopShortcutsView : public views::View,
                                    public views::ButtonListener {
 public:
  explicit TopShortcutsView(UnifiedSystemTrayController* controller);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Change the expanded state. CollapseButton icon will rotate.
  void SetExpandedAmount(double expanded_amount);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View
  const char* GetClassName() const override;

 private:
  friend class TopShortcutsViewTest;

  // Disables/Enables the |settings_button_| based on kSettingsIconEnabled pref.
  void UpdateSettingsButtonState();

  UnifiedSystemTrayController* controller_;

  // Owned by views hierarchy.
  views::Button* user_avatar_button_ = nullptr;
  SignOutButton* sign_out_button_ = nullptr;
  TopShortcutButtonContainer* container_ = nullptr;
  TopShortcutButton* lock_button_ = nullptr;
  TopShortcutButton* settings_button_ = nullptr;
  TopShortcutButton* power_button_ = nullptr;
  CollapseButton* collapse_button_ = nullptr;

  PrefChangeRegistrar local_state_pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TopShortcutsView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
