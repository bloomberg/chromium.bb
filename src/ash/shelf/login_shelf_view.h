// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_VIEW_H_
#define ASH_SHELF_LOGIN_SHELF_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/lock_screen_action/lock_screen_action_background_observer.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/scoped_guest_button_blocker.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace session_manager {
enum class SessionState;
}

namespace ash {

enum class LockScreenActionBackgroundState;

class KioskAppsButton;
class TrayBackgroundView;

// LoginShelfView contains the shelf buttons visible outside of an active user
// session. ShelfView and LoginShelfView should never be shown together.
class ASH_EXPORT LoginShelfView : public views::View,
                                  public TrayActionObserver,
                                  public LockScreenActionBackgroundObserver,
                                  public ShutdownControllerImpl::Observer,
                                  public LoginDataDispatcher::Observer {
 public:
  enum ButtonId {
    kShutdown = 1,          // Shut down the device.
    kRestart,               // Restart the device.
    kSignOut,               // Sign out the active user session.
    kCloseNote,             // Close the lock screen note.
    kCancel,                // Cancel multiple user sign-in.
    kBrowseAsGuest,         // Use in guest mode.
    kAddUser,               // Add a new user.
    kApps,                  // Show list of available kiosk apps.
    kParentAccess,          // Unlock child device with Parent Access Code.
    kEnterpriseEnrollment,  // Start enterprise enrollment flow.
  };

  // Stores and notifies UiUpdate test callbacks.
  class TestUiUpdateDelegate {
   public:
    virtual ~TestUiUpdateDelegate();
    virtual void OnUiUpdate() = 0;
  };

 public:
  explicit LoginShelfView(
      LockScreenActionBackgroundController* lock_screen_action_background);
  ~LoginShelfView() override;

  // ShelfWidget observes SessionController for higher-level UI changes and
  // then notifies LoginShelfView to update its own UI.
  void UpdateAfterSessionChange();

  // Sets the contents of the kiosk app menu, as well as the callback used when
  // a menu item is selected.
  void SetKioskApps(
      const std::vector<KioskAppMenuEntry>& kiosk_apps,
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
      const base::RepeatingClosure& on_show_menu);

  // Sets the state of the login dialog.
  void SetLoginDialogState(OobeDialogState state);

  // Sets if the guest button on the login shelf can be shown. Even if set to
  // true the button may still not be visible.
  void SetAllowLoginAsGuest(bool allow_guest);

  // Sets whether parent access button can be shown on the login shelf.
  void ShowParentAccessButton(bool show);

  // Sets if the guest button and apps button on the login shelf can be
  // shown during gaia signin screen.
  void SetIsFirstSigninStep(bool is_first);

  // Sets whether users can be added from the login screen.
  void SetAddUserButtonEnabled(bool enable_add_user);

  // Sets whether shutdown button is enabled in the login screen.
  void SetShutdownButtonEnabled(bool enable_shutdown_button);

  // Disable shelf buttons and tray buttons temporarily and enable them back
  // later. It could be used for temporary disable due to opened modal dialog.
  void SetButtonEnabled(bool enabled);

  // Sets and animates the opacity of login shelf buttons.
  void SetButtonOpacity(float target_opacity);

  // views::View:
  const char* GetClassName() const override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void Layout() override;

  gfx::Rect get_button_union_bounds() const { return button_union_bounds_; }

  // Test API. Returns true if request was successful (i.e. button was
  // clickable).
  bool LaunchAppForTesting(const std::string& app_id);

  // Adds test delegate. Delegate will become owned by LoginShelfView.
  void InstallTestUiUpdateDelegate(
      std::unique_ptr<TestUiUpdateDelegate> delegate);

  TestUiUpdateDelegate* test_ui_update_delegate() {
    return test_ui_update_delegate_.get();
  }

  // Returns scoped object to temporarily block Browse as Guest login button.
  std::unique_ptr<ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker();

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // LockScreenActionBackgroundObserver:
  void OnLockScreenActionBackgroundStateChanged(
      LockScreenActionBackgroundState state) override;

  // ShutdownControllerImpl::Observer:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(const std::vector<LoginUserInfo>& users) override;
  void OnOobeDialogStateChanged(OobeDialogState state) override;

  // Called when a locale change is detected. Updates the login shelf button
  // strings.
  void HandleLocaleChange();

 private:
  class ScopedGuestButtonBlockerImpl;

  bool LockScreenActionBackgroundAnimating() const;

  // Updates the visibility of buttons based on state changes, e.g. shutdown
  // policy updates, session state changes etc.
  void UpdateUi();

  // Updates the color of all buttons. Uses dark colors if |use_dark_colors| is
  // true, light colors otherwise.
  void UpdateButtonColors(bool use_dark_colors);

  // Updates the total bounds of all buttons.
  void UpdateButtonUnionBounds();

  bool ShouldShowGuestButton() const;

  bool ShouldShowEnterpriseEnrollmentButton() const;

  bool ShouldShowAppsButton() const;

  bool ShouldShowGuestAndAppsButtons() const;

  // Helper function which calls `closure` when device display is on. Or if the
  // number of dropped calls exceeds 'kMaxDroppedCallsWhenDisplaysOff'
  void CallIfDisplayIsOn(const base::RepeatingClosure& closure);

  OobeDialogState dialog_state_ = OobeDialogState::HIDDEN;
  bool allow_guest_ = true;
  bool is_first_signin_step_ = false;
  bool show_parent_access_ = false;
  // When the Gaia screen is active during Login, the guest-login button should
  // appear if there are no user views.
  bool login_screen_has_users_ = false;

  LockScreenActionBackgroundController* lock_screen_action_background_;

  base::ScopedObservation<TrayAction, TrayActionObserver>
      tray_action_observation_{this};

  base::ScopedObservation<LockScreenActionBackgroundController,
                          LockScreenActionBackgroundObserver>
      lock_screen_action_background_observation_{this};

  base::ScopedObservation<ShutdownControllerImpl,
                          ShutdownControllerImpl::Observer>
      shutdown_controller_observation_{this};

  base::ScopedObservation<LoginDataDispatcher, LoginDataDispatcher::Observer>
      login_data_dispatcher_observation_{this};

  // The kiosk app button will only be created for the primary display's login
  // shelf.
  KioskAppsButton* kiosk_apps_button_ = nullptr;

  // This is used in tests to wait until UI is updated.
  std::unique_ptr<TestUiUpdateDelegate> test_ui_update_delegate_;

  // The bounds of all the buttons that this view is showing. Useful for
  // letting events that target the "empty space" pass through. These
  // coordinates are local to the view.
  gfx::Rect button_union_bounds_;

  // Number of active scoped Guest button blockers.
  int scoped_guest_button_blockers_ = 0;

  // Whether shelf buttons are temporarily disabled due to opened modal dialog.
  bool is_shelf_temp_disabled_ = false;

  // Counter for dropped shutdown and signout calls due to turned off displays.
  int dropped_calls_when_displays_off_ = 0;

  // Set of the tray buttons which are in disabled state. It is used to record
  // and recover the states of tray buttons after temporarily disable of the
  // buttons.
  std::set<TrayBackgroundView*> disabled_tray_buttons_;

  base::WeakPtrFactory<LoginShelfView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_VIEW_H_
