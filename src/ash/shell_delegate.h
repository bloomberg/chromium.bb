// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/bluetooth_system.mojom-forward.h"
#include "services/device/public/mojom/fingerprint.mojom-forward.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace aura {
class Window;
}

namespace ui {
class OSExchangeData;
}

namespace ash {

class AccessibilityDelegate;
class BackGestureContextualNudgeController;
class BackGestureContextualNudgeDelegate;
class CaptureModeDelegate;
class DesksTemplatesDelegate;
class NearbyShareController;
class NearbyShareDelegate;

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() = default;

  // Returns true if |window| can be shown for the delegate's concept of current
  // user.
  virtual bool CanShowWindowForUser(const aura::Window* window) const = 0;

  // Creates and returns the delegate of the Capture Mode feature.
  virtual std::unique_ptr<CaptureModeDelegate> CreateCaptureModeDelegate()
      const = 0;

  // Creates a accessibility delegate. Shell takes ownership of the delegate.
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() = 0;

  // Creates a back gesture contextual nudge delegate for |controller|.
  virtual std::unique_ptr<BackGestureContextualNudgeDelegate>
  CreateBackGestureContextualNudgeDelegate(
      BackGestureContextualNudgeController* controller) = 0;

  virtual std::unique_ptr<NearbyShareDelegate> CreateNearbyShareDelegate(
      NearbyShareController* controller) const = 0;

  virtual std::unique_ptr<DesksTemplatesDelegate> CreateDesksTemplatesDelegate()
      const = 0;

  // Check whether the current tab of the browser window can go back.
  virtual bool CanGoBack(gfx::NativeWindow window) const = 0;

  // Sets the tab scrubber |enabled_| field to |enabled|.
  virtual void SetTabScrubberChromeOSEnabled(bool enabled) = 0;

  // Returns true if |window| allows default touch behaviors. If false, it means
  // no default touch behavior is allowed (i.e., the touch action of window is
  // cc::TouchAction::kNone). This function is used by BackGestureEventHandler
  // to decide if we can perform the system default back gesture.
  virtual bool AllowDefaultTouchActions(gfx::NativeWindow window);

  // Returns true if we should wait for touch press ack when deciding if back
  // gesture can be performed.
  virtual bool ShouldWaitForTouchPressAck(gfx::NativeWindow window);

  // Checks whether a drag-drop operation is a tab drag.
  virtual bool IsTabDrag(const ui::OSExchangeData& drop_data);

  // Return the height of WebUI tab strip used to determine if a tab has
  // dragged out of it.
  virtual int GetBrowserWebUITabStripHeight() = 0;

  // Binds a BluetoothSystemFactory receiver if possible.
  virtual void BindBluetoothSystemFactory(
      mojo::PendingReceiver<device::mojom::BluetoothSystemFactory> receiver) {}

  // Binds a fingerprint receiver in the Device Service if possible.
  virtual void BindFingerprint(
      mojo::PendingReceiver<device::mojom::Fingerprint> receiver) {}

  // Binds a MultiDeviceSetup receiver for the primary profile.
  virtual void BindMultiDeviceSetup(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup> receiver) = 0;

  // Returns an interface to the Media Session service, or null if not
  // available.
  virtual media_session::MediaSessionService* GetMediaSessionService();

  virtual void OpenKeyboardShortcutHelpPage() const {}

  // Returns if window browser sessions are restoring.
  virtual bool IsSessionRestoreInProgress() const = 0;

  // Adjust system configuration for a Locked Fullscreen window.
  virtual void SetUpEnvironmentForLockedFullscreen(bool locked) = 0;

  // Ui Dev Tools control.
  virtual bool IsUiDevToolsStarted() const;
  virtual void StartUiDevTools() {}
  virtual void StopUiDevTools() {}
  virtual int GetUiDevToolsPort() const;

  // Returns true if Chrome was started with --disable-logging-redirect option.
  virtual bool IsLoggingRedirectDisabled() const = 0;

  // Returns empty path if user session has not started yet, or path to the
  // primary user Downloads folder if user has already logged in.
  virtual base::FilePath GetPrimaryUserDownloadsFolder() const = 0;

  // Opens the feedback page with pre-populated description #BentoBar for
  // persistent desks bar. Note, this will be removed once the feature is fully
  // launched or removed.
  virtual void OpenFeedbackPageForPersistentDesksBar() = 0;

  // Returns the last committed URL from the web contents if the given |window|
  // contains a browser frame, otherwise returns GURL::EmptyURL().
  virtual const GURL& GetLastCommittedURLForWindowIfAny(aura::Window* window);
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
