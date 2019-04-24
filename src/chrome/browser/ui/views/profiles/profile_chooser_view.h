// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace views {
class LabelButton;
}

class Browser;
class DiceSigninButtonView;
class HoverButton;

// TODO(https://crbug.com/934689): Separation of providing content for different
// menus and the UI effort to view it between this class and
// |ProfileMenuViewBase| is in progress.

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public ProfileMenuViewBase,
                           public AvatarMenuObserver,
                           public views::ButtonListener,
                           public views::LinkListener,
                           public identity::IdentityManager::Observer {
 public:
  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  // There are 2 ways to position the Bubble, if |anchor_button| is set, then
  // |parent_window| and |anchor_rect| are ignored. Otherwise, |parent_window|
  // and |anchor_rect| have to be set.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      views::Button* anchor_button,
      gfx::NativeView parent_window,
      const gfx::Rect& anchor_rect,
      Browser* browser,
      bool is_source_keyboard);
  static bool IsShowing();
  static void Hide();

 private:
  friend class ProfileChooserViewExtensionsTest;

  typedef std::vector<size_t> Indexes;
  typedef std::map<views::Button*, int> ButtonIndexes;

  ProfileChooserView(views::Button* anchor_button,
                     const gfx::Rect& anchor_rect,
                     gfx::NativeView parent_window,
                     Browser* browser,
                     profiles::BubbleViewMode view_mode,
                     signin::GAIAServiceType service_type,
                     signin_metrics::AccessPoint access_point,
                     bool is_source_keyboard);
  ~ProfileChooserView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;
  void OnWidgetClosing(views::Widget* widget) override;
  views::View* GetInitiallyFocusedView() override;
  base::string16 GetAccessibleWindowTitle() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* sender, int event_flags) override;

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override;

  // identity::IdentityManager::Observer overrides.
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  static ProfileChooserView* profile_bubble_;

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  void ResetView();

  // Shows the bubble with the |view_to_display|.
  void ShowView(profiles::BubbleViewMode view_to_display,
                AvatarMenu* avatar_menu);
  void ShowViewFromMode(profiles::BubbleViewMode mode);

  // Focuses the first profile button in the menu list.
  void FocusFirstProfileButton();

  // Creates the profile chooser view.
  views::View* CreateProfileChooserView(AvatarMenu* avatar_menu);

  // Creates the incognito window count view.
  views::View* CreateIncognitoWindowCountView();

  // Creates the main profile card for the profile |avatar_item|. |is_guest|
  // is used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  views::View* CreateCurrentProfileView(
      const AvatarMenu::Item& avatar_item,
      bool is_guest);
  views::View* CreateGuestProfileView();
  views::View* CreateOptionsView(bool display_lock, AvatarMenu* avatar_menu);
  views::View* CreateSupervisedUserDisclaimerView();
  views::View* CreateAutofillHomeView();

  // Creates the DICE UI view to sign in and turn on sync. It includes an
  // illustration, a promo and a button.
  views::View* CreateDiceSigninView();

  // Creates a header for signin and sync error surfacing for the user menu.
  views::View* CreateSyncErrorViewIfNeeded(const AvatarMenu::Item& avatar_item);

  // Creates a view showing the profile associated with |avatar_item| and an
  // error button below.
  views::View* CreateDiceSyncErrorView(const AvatarMenu::Item& avatar_item,
                                       sync_ui_util::AvatarSyncErrorType error,
                                       int button_string_id);

  bool ShouldShowGoIncognito() const;

  // Clean-up done after an action was performed in the ProfileChooser.
  void PostActionPerformed(ProfileMetrics::ProfileDesktopMenu action_performed);

  // Callbacks for DiceAccountsMenu.
  void EnableSync(const base::Optional<AccountInfo>& account);
  void SignOutAllWebAccounts();

  // Methods to keep track of the number of times the Dice sign-in promo has
  // been shown.
  int GetDiceSigninPromoShowCount() const;
  void IncrementDiceSigninPromoShowCount();

  std::unique_ptr<AvatarMenu> avatar_menu_;

  // Other profiles used in the "fast profile switcher" view.
  ButtonIndexes open_other_profile_indexes_map_;

  // Button in the signin/sync error header on top of the desktop user menu.
  views::LabelButton* sync_error_button_;

  // Links and buttons displayed in the active profile card.
  views::Link* manage_accounts_link_;
  views::LabelButton* manage_accounts_button_;
  views::LabelButton* signin_current_profile_button_;
  HoverButton* sync_to_another_account_button_;
  views::LabelButton* signin_with_gaia_account_button_;

  // For material design user menu, the active profile card owns the profile
  // name and photo.
  views::LabelButton* current_profile_card_;

  // Action buttons.
  views::LabelButton* first_profile_button_;
  views::LabelButton* guest_profile_button_;
  views::LabelButton* users_button_;
  views::LabelButton* go_incognito_button_;
  views::LabelButton* lock_button_;
  views::LabelButton* close_all_windows_button_;
  views::Link* add_account_link_;
  views::LabelButton* passwords_button_;
  views::LabelButton* credit_cards_button_;
  views::LabelButton* addresses_button_;
  views::LabelButton* signout_button_;

  // View for the signin/turn-on-sync button in the dice promo.
  DiceSigninButtonView* dice_signin_button_view_;

  // Active view mode.
  profiles::BubbleViewMode view_mode_;

  // The GAIA service type provided in the response header.
  signin::GAIAServiceType gaia_service_type_;

  // The current access point of sign in.
  const signin_metrics::AccessPoint access_point_;

  // Accounts that are presented in the enable sync promo.
  std::vector<AccountInfo> dice_sync_promo_accounts_;

  // Accounts submenu that is shown when |sync_to_another_account_button_| is
  // pressed.
  std::unique_ptr<DiceAccountsMenu> dice_accounts_menu_;

  const bool dice_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
