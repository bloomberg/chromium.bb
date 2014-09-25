// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_DISPLAY_CONFIGURATOR_H_
#define UI_DISPLAY_CHROMEOS_DISPLAY_CONFIGURATOR_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/event_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Point;
class Size;
}

namespace ui {
class DisplayMode;
class DisplaySnapshot;
class NativeDisplayDelegate;

// This class interacts directly with the system display configurator.
class DISPLAY_EXPORT DisplayConfigurator : public NativeDisplayObserver {
 public:
  typedef uint64_t ContentProtectionClientId;
  static const ContentProtectionClientId kInvalidClientId = 0;

  struct DisplayState {
    DisplayState();

    DisplaySnapshot* display;  // Not owned.

    // User-selected mode for the display.
    const DisplayMode* selected_mode;

    // Mode used when displaying the same desktop on multiple displays.
    const DisplayMode* mirror_mode;
  };

  typedef std::vector<DisplayState> DisplayStateList;

  class Observer {
   public:
    virtual ~Observer() {}

    // Called after the display mode has been changed. |display| contains the
    // just-applied configuration. Note that the X server is no longer grabbed
    // when this method is called, so the actual configuration could've changed
    // already.
    virtual void OnDisplayModeChanged(
        const std::vector<DisplayState>& displays) {}

    // Called after a display mode change attempt failed. |failed_new_state| is
    // the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(
        MultipleDisplayState failed_new_state) {}
  };

  // Interface for classes that make decisions about which display state
  // should be used.
  class StateController {
   public:
    virtual ~StateController() {}

    // Called when displays are detected.
    virtual MultipleDisplayState GetStateForDisplayIds(
        const std::vector<int64_t>& display_ids) const = 0;

    // Queries the resolution (|size|) in pixels to select display mode for the
    // given display id.
    virtual bool GetResolutionForDisplayId(int64_t display_id,
                                           gfx::Size* size) const = 0;
  };

  // Interface for classes that implement software based mirroring.
  class SoftwareMirroringController {
   public:
    virtual ~SoftwareMirroringController() {}

    // Called when the hardware mirroring failed.
    virtual void SetSoftwareMirroring(bool enabled) = 0;
    virtual bool SoftwareMirroringEnabled() const = 0;
  };

  // Helper class used by tests.
  class TestApi {
   public:
    TestApi(DisplayConfigurator* configurator) : configurator_(configurator) {}
    ~TestApi() {}

    // If |configure_timer_| is started, stops the timer, runs
    // ConfigureDisplays(), and returns true; returns false otherwise.
    bool TriggerConfigureTimeout() WARN_UNUSED_RESULT;

   private:
    DisplayConfigurator* configurator_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Flags that can be passed to SetDisplayPower().
  static const int kSetDisplayPowerNoFlags;
  // Configure displays even if the passed-in state matches |power_state_|.
  static const int kSetDisplayPowerForceProbe;
  // Do not change the state if multiple displays are connected or if the
  // only connected display is external.
  static const int kSetDisplayPowerOnlyIfSingleInternalDisplay;

  // Gap between screens so cursor at bottom of active display doesn't
  // partially appear on top of inactive display. Higher numbers guard
  // against larger cursors, but also waste more memory.
  // For simplicity, this is hard-coded to avoid the complexity of always
  // determining the DPI of the screen and rationalizing which screen we
  // need to use for the DPI calculation.
  // See crbug.com/130188 for initial discussion.
  static const int kVerticalGap = 60;

  // Returns the mode within |display| that matches the given size with highest
  // refresh rate. Returns None if no matching display was found.
  static const DisplayMode* FindDisplayModeMatchingSize(
      const DisplaySnapshot& display,
      const gfx::Size& size);

  DisplayConfigurator();
  virtual ~DisplayConfigurator();

  MultipleDisplayState display_state() const { return display_state_; }
  chromeos::DisplayPowerState requested_power_state() const {
    return requested_power_state_;
  }
  const gfx::Size framebuffer_size() const { return framebuffer_size_; }
  const std::vector<DisplayState>& cached_displays() const {
    return cached_displays_;
  }

  void set_state_controller(StateController* controller) {
    state_controller_ = controller;
  }
  void set_mirroring_controller(SoftwareMirroringController* controller) {
    mirroring_controller_ = controller;
  }

  // Replaces |native_display_delegate_| with the delegate passed in and sets
  // |configure_display_| to true. Should be called before Init().
  void SetDelegateForTesting(
      scoped_ptr<NativeDisplayDelegate> display_delegate);

  // Sets the initial value of |power_state_|.  Must be called before Start().
  void SetInitialDisplayPower(chromeos::DisplayPowerState power_state);

  // Initialization, must be called right after constructor.
  // |is_panel_fitting_enabled| indicates hardware panel fitting support.
  void Init(bool is_panel_fitting_enabled);

  // Does initial configuration of displays during startup.
  // If |background_color_argb| is non zero and there are multiple displays,
  // DisplayConfigurator sets the background color of X's RootWindow to this
  // color.
  void ForceInitialConfigure(uint32_t background_color_argb);

  // Stop handling display configuration events/requests.
  void PrepareForExit();

  // Called when powerd notifies us that some set of displays should be turned
  // on or off.  This requires enabling or disabling the CRTC associated with
  // the display(s) in question so that the low power state is engaged.
  // |flags| contains bitwise-or-ed kSetDisplayPower* values. Returns true if
  // the system successfully enters (or was already in) |power_state|.
  bool SetDisplayPower(chromeos::DisplayPowerState power_state, int flags);

  // Force switching the display mode to |new_state|. Returns false if
  // switching failed (possibly because |new_state| is invalid for the
  // current set of connected displays).
  bool SetDisplayMode(MultipleDisplayState new_state);

  // NativeDisplayDelegate::Observer overrides:
  virtual void OnConfigurationChanged() OVERRIDE;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets all the displays into pre-suspend mode; usually this means
  // configure them for their resume state. This allows faster resume on
  // machines where display configuration is slow.
  void SuspendDisplays();

  // Reprobes displays to handle changes made while the system was
  // suspended.
  void ResumeDisplays();

  // Registers a client for display protection and requests a client id. Returns
  // 0 if requesting failed.
  ContentProtectionClientId RegisterContentProtectionClient();

  // Unregisters the client.
  void UnregisterContentProtectionClient(ContentProtectionClientId client_id);

  // Queries link status and protection status.
  // |link_mask| is the type of connected display links, which is a bitmask of
  // DisplayConnectionType values. |protection_mask| is the desired protection
  // methods, which is a bitmask of the ContentProtectionMethod values.
  // Returns true on success.
  bool QueryContentProtectionStatus(ContentProtectionClientId client_id,
                                    int64_t display_id,
                                    uint32_t* link_mask,
                                    uint32_t* protection_mask);

  // Requests the desired protection methods.
  // |protection_mask| is the desired protection methods, which is a bitmask
  // of the ContentProtectionMethod values.
  // Returns true when the protection request has been made.
  bool EnableContentProtection(ContentProtectionClientId client_id,
                               int64_t display_id,
                               uint32_t desired_protection_mask);

  // Checks the available color profiles for |display_id| and fills the result
  // into |profiles|.
  std::vector<ui::ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      int64_t display_id);

  // Updates the color calibration to |new_profile|.
  bool SetColorCalibrationProfile(int64_t display_id,
                                  ui::ColorCalibrationProfile new_profile);

 private:
  // Mapping a display_id to a protection request bitmask.
  typedef std::map<int64_t, uint32_t> ContentProtections;
  // Mapping a client to its protection request.
  typedef std::map<ContentProtectionClientId, ContentProtections>
      ProtectionRequests;

  // Performs platform specific delegate initialization.
  scoped_ptr<NativeDisplayDelegate> CreatePlatformNativeDisplayDelegate();

  // Updates |cached_displays_| to contain currently-connected displays. Calls
  // |delegate_->GetDisplays()| and then does additional work, like finding the
  // mirror mode and setting user-preferred modes. Note that the server must be
  // grabbed via |delegate_->GrabServer()| first.
  void UpdateCachedDisplays();

  // Helper method for UpdateCachedDisplays() that initializes the passed-in
  // displays' |mirror_mode| fields by looking for a mode in |internal_display|
  // and |external_display| having the same resolution. Returns false if a
  // shared
  // mode wasn't found or created.
  //
  // |try_panel_fitting| allows creating a panel-fitting mode for
  // |internal_display| instead of only searching for a matching mode (note that
  // it may lead to a crash if |internal_info| is not capable of panel fitting).
  //
  // |preserve_aspect| limits the search/creation only to the modes having the
  // native aspect ratio of |external_display|.
  bool FindMirrorMode(DisplayState* internal_display,
                      DisplayState* external_display,
                      bool try_panel_fitting,
                      bool preserve_aspect);

  // Configures displays.
  void ConfigureDisplays();

  // Notifies observers about an attempted state change.
  void NotifyObservers(bool success, MultipleDisplayState attempted_state);

  // Switches to the state specified in |display_state| and |power_state|.
  // If the hardware mirroring failed and |mirroring_controller_| is set,
  // it switches to |STATE_DUAL_EXTENDED| and calls |SetSoftwareMirroring()|
  // to enable software based mirroring.
  // On success, updates |display_state_|, |power_state_|, and
  // |cached_displays_| and returns true.
  bool EnterStateOrFallBackToSoftwareMirroring(
      MultipleDisplayState display_state,
      chromeos::DisplayPowerState power_state);

  // Switches to the state specified in |display_state| and |power_state|.
  // On success, updates |display_state_|, |power_state_|, and
  // |cached_displays_| and returns true.
  bool EnterState(MultipleDisplayState display_state,
                  chromeos::DisplayPowerState power_state);

  // Returns the display state that should be used with |cached_displays_| while
  // in |power_state|.
  MultipleDisplayState ChooseDisplayState(
      chromeos::DisplayPowerState power_state) const;

  // Returns the ratio between mirrored mode area and native mode area:
  // (mirror_mode_width * mirrow_mode_height) / (native_width * native_height)
  float GetMirroredDisplayAreaRatio(const DisplayState& display);

  // Returns true if in either hardware or software mirroring mode.
  bool IsMirroring() const;

  // Applies display protections according to requests.
  bool ApplyProtections(const ContentProtections& requests);

  StateController* state_controller_;
  SoftwareMirroringController* mirroring_controller_;
  scoped_ptr<NativeDisplayDelegate> native_display_delegate_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled.  If we aren't running on ChromeOS, we can't assume that the
  // Xrandr X11 extension is supported.
  // If this flag is set to false, any attempts to change the display
  // configuration to immediately fail without changing the state.
  bool configure_display_;

  // The current display state.
  MultipleDisplayState display_state_;

  gfx::Size framebuffer_size_;

  // The last-requested and current power state. These may differ if
  // configuration fails: SetDisplayMode() needs the last-requested state while
  // SetDisplayPower() needs the current state.
  chromeos::DisplayPowerState requested_power_state_;
  chromeos::DisplayPowerState current_power_state_;

  // Most-recently-used display configuration. Note that the actual
  // configuration changes asynchronously.
  DisplayStateList cached_displays_;

  ObserverList<Observer> observers_;

  // The timer to delay configuring displays. This is used to aggregate multiple
  // display configuration events when they are reported in short time spans.
  // See comment for NativeDisplayEventDispatcherX11 for more details.
  base::OneShotTimer<DisplayConfigurator> configure_timer_;

  // Id for next display protection client.
  ContentProtectionClientId next_display_protection_client_id_;

  // Display protection requests of each client.
  ProtectionRequests client_protection_requests_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurator);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_DISPLAY_CONFIGURATOR_H_
