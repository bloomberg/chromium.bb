// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_

#include "ash/ash_export.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/scoped_observation.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class ImageButton;
}
namespace ash {

class EcheIconLoadingIndicatorView;
class PhoneHubContentView;
class TrayBubbleWrapper;
class SessionControllerImpl;

namespace phonehub {
class PhoneHubManager;
}

// This class represents the Phone Hub tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT PhoneHubTray : public TrayBackgroundView,
                                public OnboardingView::Delegate,
                                public PhoneStatusView::Delegate,
                                public PhoneHubUiController::Observer,
                                public SessionObserver {
 public:
  explicit PhoneHubTray(Shelf* shelf);
  PhoneHubTray(const PhoneHubTray&) = delete;
  ~PhoneHubTray() override;
  PhoneHubTray& operator=(const PhoneHubTray&) = delete;

  // Sets the PhoneHubManager that provides the data to drive the UI.
  void SetPhoneHubManager(phonehub::PhoneHubManager* phone_hub_manager);

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubble() override;
  void ShowBubble() override;
  bool PerformAction(const ui::Event& event) override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // PhoneStatusView::Delegate:
  bool CanOpenConnectedDeviceSettings() override;
  void OpenConnectedDevicesSettings() override;

  // OnboardingView::Delegate:
  void HideStatusHeaderView() override;

  // Provides the Eche icon and Eche loading indicator to
  // `EcheTray` in order to let `EcheTray` control the visibiliity
  // of them. Please note that these views are in control of 'EcheTray'
  // and the phone hub area is "borrowed" by `EcheTray` for the
  // purpose of grouping the icons together.
  views::ImageButton* eche_icon_view() { return eche_icon_; }
  EcheIconLoadingIndicatorView* eche_loading_indicator() {
    return eche_loading_indicator_;
  }

  // Sets a callback that will be called when eche icon is activated.
  void SetEcheIconActivationCallback(
      base::RepeatingCallback<bool(const ui::Event&)> callback);

  views::View* content_view_for_testing() { return content_view_; }

  PhoneHubUiController* ui_controller_for_testing() {
    return ui_controller_.get();
  }

 private:
  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Updates the visibility of the tray in the shelf based on the feature is
  // enabled.
  void UpdateVisibility();

  // Disables the animation and enables it back after a 5s delay. This tray's
  // visibility can be updated when the connection is complete. After a session
  // has started (login/unlock/user-switch), a duration is added here to delay
  // the animation being enabled, since it would take a few seconds to get
  // connected.
  void TemporarilyDisableAnimation();

  // Button click/press handlers for main phone hub icon and secondary
  // Eche icon.
  void EcheIconActivated(const ui::Event& event);
  void PhoneHubIconActivated(const ui::Event& event);

  // Icon of the tray. Unowned.
  views::ImageButton* icon_;

  // Icon for Eche. Unowned.
  views::ImageButton* eche_icon_ = nullptr;

  // The loading indicator, showing a throbber animation on top of the icon.
  EcheIconLoadingIndicatorView* eche_loading_indicator_ = nullptr;

  // This callback is called when the Eche icon is activated.
  base::RepeatingCallback<bool(const ui::Event&)> eche_icon_callback_ =
      base::BindRepeating([](const ui::Event&) { return true; });

  // Controls the main content view displayed in the bubble based on the current
  // PhoneHub state.
  std::unique_ptr<PhoneHubUiController> ui_controller_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The header status view on top of the bubble.
  views::View* phone_status_view_ = nullptr;

  // The main content view of the bubble, which changes depending on the state.
  // Unowned.
  PhoneHubContentView* content_view_ = nullptr;

  base::ScopedObservation<PhoneHubUiController, PhoneHubUiController::Observer>
      observed_phone_hub_ui_controller_{this};
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      observed_session_{this};

  base::WeakPtrFactory<PhoneHubTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
