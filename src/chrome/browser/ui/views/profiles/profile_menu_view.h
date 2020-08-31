// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "ui/views/controls/styled_label.h"

namespace views {
class Button;
}

struct AccountInfo;
class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileMenuView : public ProfileMenuViewBase {
 public:
  ProfileMenuView(views::Button* anchor_button, Browser* browser);
  ~ProfileMenuView() override;

  // ProfileMenuViewBase:
  void BuildMenu() override;
  gfx::ImageSkia GetSyncIcon() const override;

 private:
  friend class ProfileMenuViewExtensionsTest;
  friend class ProfileMenuViewSignoutTest;

  // views::BubbleDialogDelegateView:
  base::string16 GetAccessibleWindowTitle() const override;

  // Button/link actions.
  void OnManageGoogleAccountButtonClicked();
  void OnPasswordsButtonClicked();
  void OnCreditCardsButtonClicked();
  void OnAddressesButtonClicked();
  void OnGuestProfileButtonClicked();
  void OnExitProfileButtonClicked();
  void OnSyncSettingsButtonClicked();
  void OnSyncErrorButtonClicked(sync_ui_util::AvatarSyncErrorType error);
  void OnSigninAccountButtonClicked(AccountInfo account);
  void OnCookiesClearedOnExitLinkClicked();
#if !defined(OS_CHROMEOS)
  void OnSignoutButtonClicked();
  void OnSigninButtonClicked();
  void OnOtherProfileSelected(const base::FilePath& profile_path);
  void OnAddNewProfileButtonClicked();
  void OnManageProfilesButtonClicked();
  void OnEditProfileButtonClicked();
#endif

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  // Helper methods for building the menu.
  void BuildIdentity();
  void BuildGuestIdentity();
  void BuildAutofillButtons();
  void BuildSyncInfo();
  void BuildFeatureButtons();
#if !defined(OS_CHROMEOS)
  void BuildSelectableProfiles();
  void BuildProfileManagementHeading();
  void BuildProfileManagementFeatureButtons();
#endif

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_
