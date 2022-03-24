// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/views/view.h"

class PrefRegistrySimple;

namespace views {
class Button;
}

namespace ash {

class CollapseButton;
class IconButton;
class TopShortcutsViewTest;
class UnifiedSystemTrayController;

// Container for the top shortcut buttons. The view may narrow gaps between
// buttons when there's not enough space. When those doesn't fit in the view
// even after that, the sign-out button will be resized.
class TopShortcutButtonContainer : public views::View {
 public:
  TopShortcutButtonContainer();

  TopShortcutButtonContainer(const TopShortcutButtonContainer&) = delete;
  TopShortcutButtonContainer& operator=(const TopShortcutButtonContainer&) =
      delete;

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
};

// Top shortcuts view shown on the top of UnifiedSystemTrayView.
class ASH_EXPORT TopShortcutsView : public views::View {
 public:
  explicit TopShortcutsView(UnifiedSystemTrayController* controller);

  TopShortcutsView(const TopShortcutsView&) = delete;
  TopShortcutsView& operator=(const TopShortcutsView&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Change the expanded state. CollapseButton icon will rotate.
  void SetExpandedAmount(double expanded_amount);

  // views::View
  const char* GetClassName() const override;

 private:
  friend class TopShortcutsViewTest;

  // Disables/Enables the |settings_button_| based on kSettingsIconEnabled pref.
  void UpdateSettingsButtonState();

  // Owned by views hierarchy.
  views::Button* user_avatar_button_ = nullptr;
  views::Button* sign_out_button_ = nullptr;
  TopShortcutButtonContainer* container_ = nullptr;
  IconButton* lock_button_ = nullptr;
  IconButton* settings_button_ = nullptr;
  IconButton* power_button_ = nullptr;
  CollapseButton* collapse_button_ = nullptr;

  PrefChangeRegistrar local_state_pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
