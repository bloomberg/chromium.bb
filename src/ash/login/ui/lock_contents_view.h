// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/login/ui/bottom_status_indicator.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_error_bubble.h"
#include "ash/login/ui/login_tooltip_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/system_tray_observer.h"
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"

namespace keyboard {
class KeyboardUIController;
}  // namespace keyboard

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class LockScreenMediaControlsView;
class LoginAuthUserView;
class LoginBigUserView;
class LoginCameraTimeoutView;
class LoginDetachableBaseModel;
class LoginExpandedPublicAccountView;
class LoginUserView;
class NoteActionLaunchButton;
class ScrollableUsersListView;
enum class SmartLockState;

namespace mojom {
enum class TrayActionState;
}

// LockContentsView hosts the root view for the lock screen. All other lock
// screen views are embedded within this one. LockContentsView is per-display,
// but it is always shown on the primary display. There is only one instance
// at a time.
class ASH_EXPORT LockContentsView
    : public NonAccessibleView,
      public LoginDataDispatcher::Observer,
      public SystemTrayObserver,
      public display::DisplayObserver,
      public KeyboardControllerObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  METADATA_HEADER(LockContentsView);
  class AuthErrorBubble;
  class ManagementBubble;
  class UserState;

  enum class BottomIndicatorState {
    kNone,
    kManagedDevice,
    kAdbSideLoadingEnabled,
  };

  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LockContentsView* view);
    ~TestApi();

    LoginBigUserView* primary_big_view() const;
    LoginBigUserView* opt_secondary_big_view() const;
    AccountId focused_user() const;
    ScrollableUsersListView* users_list() const;
    LockScreenMediaControlsView* media_controls_view() const;
    views::View* note_action() const;
    views::View* tooltip_bubble() const;
    views::View* management_bubble() const;
    LoginErrorBubble* auth_error_bubble() const;
    LoginErrorBubble* detachable_base_error_bubble() const;
    LoginErrorBubble* warning_banner_bubble() const;
    views::View* user_adding_screen_indicator() const;
    views::View* system_info() const;
    views::View* bottom_status_indicator() const;
    BottomIndicatorState bottom_status_indicator_status() const;
    LoginExpandedPublicAccountView* expanded_view() const;
    views::View* main_view() const;
    const std::vector<LockContentsView::UserState>& users() const;

    // Finds and focuses (if needed) Big User View view specified by
    // |account_id|. Returns nullptr if the user not found.
    LoginBigUserView* FindBigUser(const AccountId& account_id);
    LoginUserView* FindUserView(const AccountId& account_id);
    bool RemoveUser(const AccountId& account_id);
    bool IsOobeDialogVisible() const;
    FingerprintState GetFingerPrintState(const AccountId& account_id) const;

   private:
    LockContentsView* const view_;
  };

  enum class DisplayStyle {
    // Display all the user views, top header view in LockContentsView.
    kAll,
    // Display only the public account expanded view, other views in
    // LockContentsView are hidden.
    kExclusivePublicAccountExpandedView,
  };

  // Number of login attempts before a login dialog is shown. For example, if
  // this value is 4 then the user can submit their password 4 times, and on the
  // 4th bad attempt the login dialog is shown. This only applies to the login
  // screen.
  static const int kLoginAttemptsBeforeGaiaDialog;

  LockContentsView(
      mojom::TrayActionState initial_note_action_state,
      LockScreen::ScreenType screen_type,
      LoginDataDispatcher* data_dispatcher,
      std::unique_ptr<LoginDetachableBaseModel> detachable_base_model);

  LockContentsView(const LockContentsView&) = delete;
  LockContentsView& operator=(const LockContentsView&) = delete;

  ~LockContentsView() override;

  void FocusNextUser();
  void FocusPreviousUser();
  void ShowEnterpriseDomainManager(
      const std::string& entreprise_domain_manager);
  void ShowAdbEnabled();
  void ToggleSystemInfo();
  void ShowParentAccessDialog();

  // views::View:
  void Layout() override;
  void AddedToWidget() override;
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // NonAccessibleView:
  void OnThemeChanged() override;

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(const std::vector<LoginUserInfo>& users) override;
  void OnUserAvatarChanged(const AccountId& account_id,
                           const UserAvatar& avatar) override;
  void OnPinEnabledForUserChanged(const AccountId& user, bool enabled) override;
  void OnChallengeResponseAuthEnabledForUserChanged(const AccountId& user,
                                                    bool enabled) override;
  void OnFingerprintStateChanged(const AccountId& account_id,
                                 FingerprintState state) override;
  void OnFingerprintAuthResult(const AccountId& account_id,
                               bool success) override;
  void OnSmartLockStateChanged(const AccountId& account_id,
                               SmartLockState state) override;
  void OnSmartLockAuthResult(const AccountId& account_id,
                             bool success) override;
  void OnAuthEnabledForUser(const AccountId& user) override;
  void OnAuthDisabledForUser(
      const AccountId& user,
      const AuthDisabledData& auth_disabled_data) override;
  void OnSetTpmLockedState(const AccountId& user,
                           bool is_locked,
                           base::TimeDelta time_left) override;
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;
  void OnTapToUnlockEnabledForUserChanged(const AccountId& user,
                                          bool enabled) override;
  void OnForceOnlineSignInForUser(const AccountId& user) override;
  // TODO(https://crbug.com/1233614): Delete this method in favor of
  // OnSmartLockStateChanged once Smart Lock UI revamp is enabled.
  void OnShowEasyUnlockIcon(const AccountId& user,
                            const EasyUnlockIconInfo& icon_info) override;
  void OnWarningMessageUpdated(const std::u16string& message) override;
  void OnSystemInfoChanged(bool show,
                           bool enforced,
                           const std::string& os_version_label_text,
                           const std::string& enterprise_info_text,
                           const std::string& bluetooth_name,
                           bool adb_sideloading_enabled) override;
  void OnPublicSessionDisplayNameChanged(
      const AccountId& account_id,
      const std::string& display_name) override;
  void OnPublicSessionLocalesChanged(const AccountId& account_id,
                                     const std::vector<LocaleItem>& locales,
                                     const std::string& default_locale,
                                     bool show_advanced_view) override;
  void OnPublicSessionKeyboardLayoutsChanged(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<InputMethodItem>& keyboard_layouts) override;
  void OnPublicSessionShowFullManagementDisclosureChanged(
      bool show_full_management_disclosure) override;
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus pairing_status) override;
  void OnFocusLeavingLockScreenApps(bool reverse) override;
  void OnOobeDialogStateChanged(OobeDialogState state) override;

  void MaybeUpdateExpandedView(const AccountId& account_id,
                               const LoginUserInfo& user_info);

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

  void ShowAuthErrorMessageForDebug(int unlock_attempt);

  // Called for debugging to make |user| managed and display an icon along with
  // a note in the menu user view.
  void ToggleManagementForUserForDebug(const AccountId& user);

  // Called for debugging to make |user| having a multiprofile policy.
  void SetMultiprofilePolicyForUserForDebug(
      const AccountId& user,
      const MultiProfileUserBehavior& multiprofile_policy);

  // Called for debugging to toggle forced online sign-in form |user|.
  void ToggleForceOnlineSignInForUserForDebug(const AccountId& user);

  // Called for debugging to remove forced online sign-in form |user|.
  void UndoForceOnlineSignInForUserForDebug(const AccountId& user);

  // Called by LockScreenMediaControlsView.
  void CreateMediaControlsLayout();
  void HideMediaControlsLayout();
  bool AreMediaControlsEnabled() const;

  class UserState {
   public:
    explicit UserState(const LoginUserInfo& user_info);
    UserState(UserState&&);

    UserState(const UserState&) = delete;
    UserState& operator=(const UserState&) = delete;

    ~UserState();

    AccountId account_id;
    bool show_pin = false;
    bool show_challenge_response_auth = false;
    bool enable_tap_auth = false;
    bool force_online_sign_in = false;
    bool disable_auth = false;
    bool show_pin_pad_for_password = false;
    size_t autosubmit_pin_length = 0;
    absl::optional<EasyUnlockIconInfo> easy_unlock_icon_info = absl::nullopt;
    FingerprintState fingerprint_state;
    SmartLockState smart_lock_state;
    bool auth_factor_is_hiding_password = false;
    // When present, indicates that the TPM is locked.
    absl::optional<base::TimeDelta> time_until_tpm_unlock = absl::nullopt;
  };

 private:
  class AutoLoginUserActivityHandler;

  using DisplayLayoutAction = base::RepeatingCallback<void(bool landscape)>;

  // Focus the next/previous widget.
  void FocusNextWidget(bool reverse);

  // Set |spacing_middle| to the correct size for low density layouts. If there
  // is less spacing available than desired, use up to the available.
  void SetLowDensitySpacing(views::View* spacing_middle,
                            views::View* secondary_view,
                            int landscape_dist,
                            int portrait_dist,
                            bool landscape);

  // Set |spacing_middle| for media controls.
  void SetMediaControlsSpacing(bool landscape);

  // 1-2 users.
  void CreateLowDensityLayout(
      const std::vector<LoginUserInfo>& users,
      std::unique_ptr<LoginBigUserView> primary_big_view);
  // 3-6 users.
  void CreateMediumDensityLayout(
      const std::vector<LoginUserInfo>& users,
      std::unique_ptr<LoginBigUserView> primary_big_view);
  // 7+ users.
  void CreateHighDensityLayout(
      const std::vector<LoginUserInfo>& users,
      views::BoxLayout* main_layout,
      std::unique_ptr<LoginBigUserView> primary_big_view);

  // Lay out the entire view. This is called when the view is attached to a
  // widget and when the screen is rotated.
  void DoLayout();

  // Lay out the top header. This is called when the children of the top header
  // change contents or visibility.
  void LayoutTopHeader();

  // Lay out the bottom status indicator. This is called when system information
  // is shown if ADB is enabled and at the initialization of lock screen if the
  // device is enrolled.
  void LayoutBottomStatusIndicator();

  // Lay out the user adding screen indicator. This is called when a secondary
  // user is being added.
  void LayoutUserAddingScreenIndicator();

  // Lay out the expanded public session view.
  void LayoutPublicSessionView();

  // Adds |layout_action| to |layout_actions_| and immediately executes it with
  // the current rotation.
  void AddDisplayLayoutAction(const DisplayLayoutAction& layout_action);

  // Change the active |auth_user_|. If |is_primary| is true, the active auth
  // switches to |opt_secondary_big_view_|. If |is_primary| is false, the active
  // auth switches to |primary_big_view_|.
  void SwapActiveAuthBetweenPrimaryAndSecondary(bool is_primary);

  // Called when an authentication check is complete.
  void OnAuthenticate(bool auth_success, bool display_error_messages);

  // Tries to lookup the stored state for |user|. Returns an unowned pointer
  // that is invalidated whenver |users_| changes.
  UserState* FindStateForUser(const AccountId& user);

  // Updates the auth methods for |to_update| and |to_hide|, if passed.
  // For auth users:
  //   |to_hide| will be set to LoginAuthUserView::AUTH_NONE. At minimum,
  //   |to_update| will show a password prompt.
  // For pubic account users:
  //   |to_hide| will set to disable auth.
  //   |to_update| will show an arrow button.
  void LayoutAuth(LoginBigUserView* to_update,
                  LoginBigUserView* opt_to_hide,
                  bool animate);

  // Make the user at |user_index| the big user with auth enabled.
  // We pass in the index because the actual user may change.
  void SwapToBigUser(int user_index);

  // Warning to remove a user is shown.
  void OnRemoveUserWarningShown(bool is_primary);
  // Remove one of the auth users.
  void RemoveUser(bool is_primary);

  // Called after the big user change has taken place.
  void OnBigUserChanged();

  // Shows the correct (cached) easy unlock icon for the given auth user.
  void UpdateEasyUnlockIconForUser(const AccountId& user);

  // Get the current active big user view.
  LoginBigUserView* CurrentBigUserView();

  // Opens an error bubble to indicate authentication failure.
  void ShowAuthErrorMessage();

  // Called when the easy unlock icon is hovered.
  void OnEasyUnlockIconHovered();
  // Called when the easy unlock icon is tapped.
  void OnEasyUnlockIconTapped();

  // Called when LoginAuthFactorsView enters/exits a state where an auth
  // factor wants to hide the password and pin fields.
  void OnAuthFactorIsHidingPasswordChanged(const AccountId& account_id,
                                           bool auth_factor_is_hiding_password);

  // Called when parent access validation finished for the user with
  // |account_id|.
  void OnParentAccessValidationFinished(const AccountId& account_id,
                                        bool access_granted);

  // Returns keyboard controller for the view. Returns nullptr if keyboard is
  // not activated, view has not been added to the widget yet or keyboard is not
  // displayed in this window.
  keyboard::KeyboardUIController* GetKeyboardControllerForView() const;

  // Called when the public account is tapped.
  void OnPublicAccountTapped(bool is_primary);

  void LearnMoreButtonPressed();

  // Helper method to allocate a LoginBigUserView instance.
  std::unique_ptr<LoginBigUserView> AllocateLoginBigUserView(
      const LoginUserInfo& user,
      bool is_primary);

  // Returns the big view for |user| if |user| is one of the active
  // big views. If |require_auth_active| is true then the view must
  // have auth enabled.
  LoginBigUserView* TryToFindBigUser(const AccountId& user,
                                     bool require_auth_active);

  // Returns the user view for |user|.
  LoginUserView* TryToFindUserView(const AccountId& user);

  // Change the visibility of child views based on the |style|.
  void SetDisplayStyle(DisplayStyle style);

  // Register accelerators used in login screen.
  void RegisterAccelerators();

  // Performs the specified accelerator action.
  void PerformAction(LoginAcceleratorAction action);

  // Check whether the view should display the system information based on all
  // factors including policy settings, channel and Alt-V accelerator.
  bool GetSystemInfoVisibility() const;

  // Updates the colors of the system info view.
  void UpdateSystemInfoColors();

  // Updates the colors of the bottom status indicator.
  void UpdateBottomStatusIndicatorColors();

  // Toggles the visibility of the |bottom_status_indicator_| based on its
  // content type and whether the extension UI window is opened.
  void UpdateBottomStatusIndicatorVisibility();

  // Shows a pop-up including more details about device management. It is
  // triggered when the bottom status indicator is clicked while displaying a
  // "device is managed" type message.
  void OnBottomStatusIndicatorTapped();

  // Shows GAIA sign-in page.
  void OnBackToSigninButtonTapped();

  const LockScreen::ScreenType screen_type_;

  std::vector<UserState> users_;

  LoginDataDispatcher* const data_dispatcher_;  // Unowned.
  std::unique_ptr<LoginDetachableBaseModel> detachable_base_model_;

  LoginBigUserView* primary_big_view_ = nullptr;
  LoginBigUserView* opt_secondary_big_view_ = nullptr;
  ScrollableUsersListView* users_list_ = nullptr;

  // View for media controls that appear on the lock screen if user enabled.
  LockScreenMediaControlsView* media_controls_view_ = nullptr;
  views::View* middle_spacing_view_ = nullptr;

  // View that contains the note action button and the system info labels,
  // placed on the top right corner of the screen without affecting layout of
  // other views.
  views::View* top_header_ = nullptr;

  // View for launching a note taking action handler from the lock screen.
  NoteActionLaunchButton* note_action_ = nullptr;

  // View for showing the version, enterprise and bluetooth info.
  views::View* system_info_ = nullptr;

  // Contains authentication user and the additional user views.
  NonAccessibleView* main_view_ = nullptr;

  // Actions that should be executed before a new layout happens caused by a
  // display change (eg. screen rotation). A full layout pass is performed after
  // all actions are executed.
  std::vector<DisplayLayoutAction> layout_actions_;

  display::ScopedDisplayObserver display_observer_{this};

  // All error bubbles and the tooltip view are child views of LockContentsView,
  // and will be torn down when LockContentsView is torn down.
  // Bubble for displaying authentication error.
  AuthErrorBubble* auth_error_bubble_;
  // Bubble for displaying detachable base errors.
  LoginErrorBubble* detachable_base_error_bubble_;
  // Bubble for displaying easy-unlock tooltips.
  LoginTooltipView* tooltip_bubble_;
  // Bubble for displaying management details.
  ManagementBubble* management_bubble_;
  // Indicator at top of screen for displaying a warning message when a
  // secondary user is being added.
  views::View* user_adding_screen_indicator_ = nullptr;
  // Bubble for displaying warning banner message.
  LoginErrorBubble* warning_banner_bubble_;

  // View that is shown on login timeout with camera usage.
  base::raw_ptr<LoginCameraTimeoutView> login_camera_timeout_view_ = nullptr;

  // Bottom status indicator displaying entreprise domain or ADB enabled alert
  BottomStatusIndicator* bottom_status_indicator_;

  // Tracks the visibility of the extension Ui window.
  bool extension_ui_visible_ = false;

  int unlock_attempt_ = 0;

  // Whether a lock screen app is currently active (i.e. lock screen note action
  // state is reported as kActive by the data dispatcher).
  bool lock_screen_apps_active_ = false;

  // Tracks the visibility of the OOBE dialog.
  bool oobe_dialog_visible_ = false;

  // Whether the lock screen note is disabled. Used to override the actual lock
  // screen note state.
  bool disable_lock_screen_note_ = false;

  // Whether the system information should be displayed or not be displayed
  // forcedly according to policy settings.
  absl::optional<bool> enable_system_info_enforced_ = absl::nullopt;

  // Whether the system information is intended to be displayed if possible.
  // (e.g., Alt-V is pressed, particular OS channels)
  bool enable_system_info_if_possible_ = false;

  // Expanded view for public account user to select language and keyboard.
  LoginExpandedPublicAccountView* expanded_view_ = nullptr;

  // Whether the virtual keyboard is currently shown. Used to determine whether
  // to show the PIN keyboard or not.
  bool keyboard_shown_ = false;

  // Whether there is an ongoing auth layout. The keyboard visibility might
  // change during an auth layout, if triggered by a focus request on the
  // password view. Since a keyboard visibility change triggers an auth layout
  // and since we do not want an auth layout during an auth layout, we cancel
  // the new layout request (the new keyboard visibility info is useless).
  bool ongoing_auth_layout_ = false;

  // Accelerators handled by login screen.
  std::map<ui::Accelerator, LoginAcceleratorAction> accel_map_;

  // Notifies Chrome when user activity is detected on the login screen so that
  // the auto-login timer can be reset.
  std::unique_ptr<AutoLoginUserActivityHandler>
      auto_login_user_activity_handler_;

  BottomIndicatorState bottom_status_indicator_state_ =
      BottomIndicatorState::kNone;

  base::WeakPtrFactory<LockContentsView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_
