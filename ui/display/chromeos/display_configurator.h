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
#include "ui/display/chromeos/native_display_observer.h"
#include "ui/display/display_constants.h"
#include "ui/display/display_export.h"

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
  typedef uint64_t OutputProtectionClientId;
  static const OutputProtectionClientId kInvalidClientId = 0;

  struct CoordinateTransformation {
    // Initialized to the identity transformation.
    CoordinateTransformation();

    float x_scale;
    float x_offset;
    float y_scale;
    float y_offset;
  };

  struct DisplayState {
    DisplayState();

    DisplaySnapshot* display;  // Not owned.

    // XInput device ID or 0 if this output isn't a touchscreen.
    int touch_device_id;

    CoordinateTransformation transform;

    // User-selected mode for the output.
    const DisplayMode* selected_mode;

    // Mode used when displaying the same desktop on multiple outputs.
    const DisplayMode* mirror_mode;
  };

  typedef std::vector<DisplayState> DisplayStateList;

  class Observer {
   public:
    virtual ~Observer() {}

    // Called after the display mode has been changed. |output| contains the
    // just-applied configuration. Note that the X server is no longer grabbed
    // when this method is called, so the actual configuration could've changed
    // already.
    virtual void OnDisplayModeChanged(
        const std::vector<DisplayState>& outputs) {}

    // Called after a display mode change attempt failed. |failed_new_state| is
    // the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(OutputState failed_new_state) {}
  };

  // Interface for classes that make decisions about which output state
  // should be used.
  class StateController {
   public:
    virtual ~StateController() {}

    // Called when displays are detected.
    virtual OutputState GetStateForDisplayIds(
        const std::vector<int64_t>& display_ids) const = 0;

    // Queries the resolution (|size|) in pixels to select output mode for the
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
  };

  class TouchscreenDelegate {
   public:
    virtual ~TouchscreenDelegate() {}

    // Searches for touchscreens among input devices,
    // and tries to match them up to screens in |outputs|.
    // |outputs| is an array of detected screens.
    // If a touchscreen with same resolution as an output's native mode
    // is detected, its id will be stored in this output.
    virtual void AssociateTouchscreens(std::vector<DisplayState>* outputs) = 0;

    // Configures XInput's Coordinate Transformation Matrix property.
    // |touch_device_id| the ID of the touchscreen device to configure.
    // |ctm| contains the desired transformation parameters.  The offsets
    // in it should be normalized so that 1 corresponds to the X or Y axis
    // size for the corresponding offset.
    virtual void ConfigureCTM(int touch_device_id,
                              const CoordinateTransformation& ctm) = 0;
  };

  // Helper class used by tests.
  class TestApi {
   public:
    TestApi(DisplayConfigurator* configurator) : configurator_(configurator) {}
    ~TestApi() {}

    // If |configure_timer_| is started, stops the timer, runs
    // ConfigureOutputs(), and returns true; returns false otherwise.
    bool TriggerConfigureTimeout();

   private:
    DisplayConfigurator* configurator_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Flags that can be passed to SetDisplayPower().
  static const int kSetDisplayPowerNoFlags = 0;
  // Configure displays even if the passed-in state matches |power_state_|.
  static const int kSetDisplayPowerForceProbe = 1 << 0;
  // Do not change the state if multiple displays are connected or if the
  // only connected display is external.
  static const int kSetDisplayPowerOnlyIfSingleInternalDisplay = 1 << 1;

  // Gap between screens so cursor at bottom of active display doesn't
  // partially appear on top of inactive display. Higher numbers guard
  // against larger cursors, but also waste more memory.
  // For simplicity, this is hard-coded to avoid the complexity of always
  // determining the DPI of the screen and rationalizing which screen we
  // need to use for the DPI calculation.
  // See crbug.com/130188 for initial discussion.
  static const int kVerticalGap = 60;

  // Returns the mode within |output| that matches the given size with highest
  // refresh rate. Returns None if no matching output was found.
  static const DisplayMode* FindDisplayModeMatchingSize(
      const DisplaySnapshot& output,
      const gfx::Size& size);

  DisplayConfigurator();
  virtual ~DisplayConfigurator();

  OutputState output_state() const { return output_state_; }
  chromeos::DisplayPowerState power_state() const { return power_state_; }
  const std::vector<DisplayState>& cached_outputs() const {
    return cached_outputs_;
  }

  void set_state_controller(StateController* controller) {
    state_controller_ = controller;
  }
  void set_mirroring_controller(SoftwareMirroringController* controller) {
    mirroring_controller_ = controller;
  }

  // Replaces |native_display_delegate_| with |delegate| and sets
  // |configure_display_| to true.  Should be called before Init().
  void SetNativeDisplayDelegateForTesting(
      scoped_ptr<NativeDisplayDelegate> delegate);

  void SetTouchscreenDelegateForTesting(
      scoped_ptr<TouchscreenDelegate> delegate);

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
  // |flags| contains bitwise-or-ed kSetDisplayPower* values.
  bool SetDisplayPower(chromeos::DisplayPowerState power_state, int flags);

  // Force switching the display mode to |new_state|. Returns false if
  // switching failed (possibly because |new_state| is invalid for the
  // current set of connected outputs).
  bool SetDisplayMode(OutputState new_state);

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

  const std::map<int, float>& GetMirroredDisplayAreaRatioMap() {
    return mirrored_display_area_ratio_map_;
  }

  // Registers a client for output protection and requests a client id. Returns
  // 0 if requesting failed.
  OutputProtectionClientId RegisterOutputProtectionClient();

  // Unregisters the client.
  void UnregisterOutputProtectionClient(OutputProtectionClientId client_id);

  // Queries link status and protection status.
  // |link_mask| is the type of connected output links, which is a bitmask of
  // OutputType values. |protection_mask| is the desired protection methods,
  // which is a bitmask of the OutputProtectionMethod values.
  // Returns true on success.
  bool QueryOutputProtectionStatus(OutputProtectionClientId client_id,
                                   int64_t display_id,
                                   uint32_t* link_mask,
                                   uint32_t* protection_mask);

  // Requests the desired protection methods.
  // |protection_mask| is the desired protection methods, which is a bitmask
  // of the OutputProtectionMethod values.
  // Returns true when the protection request has been made.
  bool EnableOutputProtection(OutputProtectionClientId client_id,
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
  typedef std::map<int64_t, uint32_t> DisplayProtections;
  // Mapping a client to its protection request.
  typedef std::map<OutputProtectionClientId, DisplayProtections>
      ProtectionRequests;

  // Updates |cached_outputs_| to contain currently-connected outputs. Calls
  // |delegate_->GetOutputs()| and then does additional work, like finding the
  // mirror mode and setting user-preferred modes. Note that the server must be
  // grabbed via |delegate_->GrabServer()| first.
  void UpdateCachedOutputs();

  // Helper method for UpdateCachedOutputs() that initializes the passed-in
  // outputs' |mirror_mode| fields by looking for a mode in |internal_output|
  // and |external_output| having the same resolution. Returns false if a shared
  // mode wasn't found or created.
  //
  // |try_panel_fitting| allows creating a panel-fitting mode for
  // |internal_output| instead of only searching for a matching mode (note that
  // it may lead to a crash if |internal_info| is not capable of panel fitting).
  //
  // |preserve_aspect| limits the search/creation only to the modes having the
  // native aspect ratio of |external_output|.
  bool FindMirrorMode(DisplayState* internal_output,
                      DisplayState* external_output,
                      bool try_panel_fitting,
                      bool preserve_aspect);

  // Configures outputs.
  void ConfigureOutputs();

  // Notifies observers about an attempted state change.
  void NotifyObservers(bool success, OutputState attempted_state);

  // Switches to the state specified in |output_state| and |power_state|.
  // If the hardware mirroring failed and |mirroring_controller_| is set,
  // it switches to |STATE_DUAL_EXTENDED| and calls |SetSoftwareMirroring()|
  // to enable software based mirroring.
  // On success, updates |output_state_|, |power_state_|, and |cached_outputs_|
  // and returns true.
  bool EnterStateOrFallBackToSoftwareMirroring(
      OutputState output_state,
      chromeos::DisplayPowerState power_state);

  // Switches to the state specified in |output_state| and |power_state|.
  // On success, updates |output_state_|, |power_state_|, and
  // |cached_outputs_| and returns true.
  bool EnterState(OutputState output_state,
                  chromeos::DisplayPowerState power_state);

  // Returns the output state that should be used with |cached_outputs_| while
  // in |power_state|.
  OutputState ChooseOutputState(chromeos::DisplayPowerState power_state) const;

  // Computes the relevant transformation for mirror mode.
  // |output| is the output on which mirror mode is being applied.
  // Returns the transformation or identity if computations fail.
  CoordinateTransformation GetMirrorModeCTM(const DisplayState& output);

  // Computes the relevant transformation for extended mode. |output| is the
  // output on which extended mode is being applied. |new_origin| is the
  // position of the output on the framebuffer. |framebuffer_size| is the
  // size of the combined framebuffer.
  // Returns the transformation or identity if computations fail.
  CoordinateTransformation GetExtendedModeCTM(
      const DisplayState& output,
      const gfx::Point& new_origin,
      const gfx::Size& framebuffer_size);

  // Returns the ratio between mirrored mode area and native mode area:
  // (mirror_mode_width * mirrow_mode_height) / (native_width * native_height)
  float GetMirroredDisplayAreaRatio(const DisplayState& output);

  // Applies output protections according to requests.
  bool ApplyProtections(const DisplayProtections& requests);

  StateController* state_controller_;
  SoftwareMirroringController* mirroring_controller_;
  scoped_ptr<NativeDisplayDelegate> native_display_delegate_;
  scoped_ptr<TouchscreenDelegate> touchscreen_delegate_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  // Key of the map is the touch display's id, and the value of the map is the
  // touch display's area ratio in mirror mode defined as :
  // mirror_mode_area / native_mode_area.
  // This is used for scaling touch event's radius when the touch display is in
  // mirror mode :
  // new_touch_radius = sqrt(area_ratio) * old_touch_radius
  std::map<int, float> mirrored_display_area_ratio_map_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled.  If we aren't running on ChromeOS, we can't assume that the
  // Xrandr X11 extension is supported.
  // If this flag is set to false, any attempts to change the output
  // configuration to immediately fail without changing the state.
  bool configure_display_;

  // The current display state.
  OutputState output_state_;

  // The current power state.
  chromeos::DisplayPowerState power_state_;

  // Most-recently-used output configuration. Note that the actual
  // configuration changes asynchronously.
  DisplayStateList cached_outputs_;

  ObserverList<Observer> observers_;

  // The timer to delay configuring outputs. See also the comments in
  // Dispatch().
  scoped_ptr<base::OneShotTimer<DisplayConfigurator> > configure_timer_;

  // Id for next output protection client.
  OutputProtectionClientId next_output_protection_client_id_;

  // Output protection requests of each client.
  ProtectionRequests client_protection_requests_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurator);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_DISPLAY_CONFIGURATOR_H_
