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
#include "base/memory/weak_ptr.h"
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
struct DisplayConfigureRequest;
class DisplayMode;
class DisplaySnapshot;
class NativeDisplayDelegate;
class UpdateDisplayConfigurationTask;

// This class interacts directly with the system display configurator.
class DISPLAY_EXPORT DisplayConfigurator : public NativeDisplayObserver {
 public:
  typedef uint64_t ContentProtectionClientId;
  static const ContentProtectionClientId kInvalidClientId = 0;

  typedef base::Callback<void(bool)> ConfigurationCallback;

  typedef std::vector<DisplaySnapshot*> DisplayStateList;

  class Observer {
   public:
    virtual ~Observer() {}

    // Called after the display mode has been changed. |display| contains the
    // just-applied configuration. Note that the X server is no longer grabbed
    // when this method is called, so the actual configuration could've changed
    // already.
    virtual void OnDisplayModeChanged(
        const DisplayStateList& displays) {}

    // Called after a display mode change attempt failed. |displays| contains
    // displays that are detected when failed.
    // |failed_new_state| is the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(
        const DisplayStateList& displays,
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

  class DisplayLayoutManager {
   public:
    virtual ~DisplayLayoutManager() {}

    virtual SoftwareMirroringController* GetSoftwareMirroringController()
        const = 0;

    virtual StateController* GetStateController() const = 0;

    // Returns the current display state.
    virtual MultipleDisplayState GetDisplayState() const = 0;

    // Returns the current power state.
    virtual chromeos::DisplayPowerState GetPowerState() const = 0;

    // Based on the given |displays|, display state and power state, it will
    // create display configuration requests which will then be used to
    // configure the hardware. The requested configuration is stored in
    // |requests| and |framebuffer_size|.
    virtual bool GetDisplayLayout(
        const std::vector<DisplaySnapshot*>& displays,
        MultipleDisplayState new_display_state,
        chromeos::DisplayPowerState new_power_state,
        std::vector<DisplayConfigureRequest>* requests,
        gfx::Size* framebuffer_size) const = 0;
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
  ~DisplayConfigurator() override;

  MultipleDisplayState display_state() const { return current_display_state_; }
  chromeos::DisplayPowerState requested_power_state() const {
    return requested_power_state_;
  }
  const gfx::Size framebuffer_size() const { return framebuffer_size_; }
  const std::vector<DisplaySnapshot*>& cached_displays() const {
    return cached_displays_;
  }

  // Called when an external process no longer needs to control the display
  // and Chrome can take control.
  void TakeControl();

  // Called when an external process needs to control the display and thus
  // Chrome should relinquish it.
  void RelinquishControl();

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
  // |flags| contains bitwise-or-ed kSetDisplayPower* values. After the
  // configuration finishes |callback| is called with the status of the
  // operation.
  void SetDisplayPower(chromeos::DisplayPowerState power_state,
                       int flags,
                       const ConfigurationCallback& callback);

  // Force switching the display mode to |new_state|. Returns false if
  // switching failed (possibly because |new_state| is invalid for the
  // current set of connected displays).
  void SetDisplayMode(MultipleDisplayState new_state);

  // NativeDisplayDelegate::Observer overrides:
  void OnConfigurationChanged() override;

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
  class DisplayLayoutManagerImpl;

  // Mapping a display_id to a protection request bitmask.
  typedef std::map<int64_t, uint32_t> ContentProtections;
  // Mapping a client to its protection request.
  typedef std::map<ContentProtectionClientId, ContentProtections>
      ProtectionRequests;

  // Performs platform specific delegate initialization.
  scoped_ptr<NativeDisplayDelegate> CreatePlatformNativeDisplayDelegate();

  // Configures displays. Invoked by |configure_timer_|.
  void ConfigureDisplays();

  // Restores |requested_power_state_| after the system has resumed,
  // additionally forcing a probe. Invoked by |configure_timer_|.
  void RestoreRequestedPowerStateAfterResume();

  // Notifies observers about an attempted state change.
  void NotifyObservers(bool success, MultipleDisplayState attempted_state);

  // Returns the display state that should be used with |cached_displays_| while
  // in |power_state|.
  MultipleDisplayState ChooseDisplayState(
      chromeos::DisplayPowerState power_state) const;

  // Returns true if in either hardware or software mirroring mode.
  bool IsMirroring() const;

  // Applies display protections according to requests.
  bool ApplyProtections(const ContentProtections& requests);

  // If |configuration_task_| isn't initialized, initializes it and starts the
  // configuration task.
  void RunPendingConfiguration();

  // Callback for |configuration_taks_|. When the configuration process finishes
  // this is called with the result (|success|) and the updated display state.
  void OnConfigured(bool success,
                    const std::vector<DisplaySnapshot*>& displays,
                    const gfx::Size& framebuffer_size,
                    MultipleDisplayState new_display_state,
                    chromeos::DisplayPowerState new_power_state);

  // Helps in identifying if a configuration task needs to be scheduled.
  // Return true if any of the |requested_*| parameters have been updated. False
  // otherwise.
  bool ShouldRunConfigurationTask() const;

  // Helper functions which will call the callbacks in
  // |in_progress_configuration_callbacks_| and
  // |queued_configuration_callbacks_| and clear the lists after. |success| is
  // the configuration status used when calling the callbacks.
  void CallAndClearInProgressCallbacks(bool success);
  void CallAndClearQueuedCallbacks(bool success);

  StateController* state_controller_;
  SoftwareMirroringController* mirroring_controller_;
  scoped_ptr<NativeDisplayDelegate> native_display_delegate_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled.  If we aren't running on Chrome OS, we can't assume that the
  // Xrandr X11 extension or the Ozone underlying display hotplug system are
  // supported.
  // If this flag is set to false, any attempts to change the display
  // configuration to immediately fail without changing the state.
  bool configure_display_;

  // Current configuration state.
  MultipleDisplayState current_display_state_;
  chromeos::DisplayPowerState current_power_state_;

  // Pending requests. These values are used when triggering the next display
  // configuration.
  //
  // Stores the user requested state or INVALID if nothing was requested.
  MultipleDisplayState requested_display_state_;

  // Stores the requested power state.
  chromeos::DisplayPowerState requested_power_state_;

  // True if |requested_power_state_| has been changed due to a user request.
  bool requested_power_state_change_;

  // Bitwise-or value of the |kSetDisplayPower*| flags defined above.
  int requested_power_flags_;

  // List of callbacks from callers waiting for the display configuration to
  // start/finish. Note these callbacks belong to the pending request, not a
  // request currently active.
  std::vector<ConfigurationCallback> queued_configuration_callbacks_;

  // List of callbacks belonging to the currently running display configuration
  // task.
  std::vector<ConfigurationCallback> in_progress_configuration_callbacks_;

  // True if the caller wants to force the display configuration process.
  bool force_configure_;

  // Most-recently-used display configuration. Note that the actual
  // configuration changes asynchronously.
  DisplayStateList cached_displays_;

  // Most-recently-used framebuffer size.
  gfx::Size framebuffer_size_;

  ObserverList<Observer> observers_;

  // The timer to delay configuring displays. This is used to aggregate multiple
  // display configuration events when they are reported in short time spans.
  // See comment for NativeDisplayEventDispatcherX11 for more details.
  base::OneShotTimer<DisplayConfigurator> configure_timer_;

  // Id for next display protection client.
  ContentProtectionClientId next_display_protection_client_id_;

  // Display protection requests of each client.
  ProtectionRequests client_protection_requests_;

  // Display controlled by an external entity.
  bool display_externally_controlled_;

  // Whether the displays are currently suspended.
  bool displays_suspended_;

  scoped_ptr<DisplayLayoutManager> layout_manager_;

  scoped_ptr<UpdateDisplayConfigurationTask> configuration_task_;

  // This must be the last variable.
  base::WeakPtrFactory<DisplayConfigurator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurator);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_DISPLAY_CONFIGURATOR_H_
