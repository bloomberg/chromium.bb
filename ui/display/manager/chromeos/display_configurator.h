// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_CONFIGURATOR_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_CONFIGURATOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/event_types.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/manager/chromeos/query_content_protection_task.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Size;
}

namespace display {
struct GammaRampRGBEntry;
class DisplayLayoutManager;
class DisplayMode;
class DisplaySnapshot;
class NativeDisplayDelegate;
class UpdateDisplayConfigurationTask;

// This class interacts directly with the system display configurator.
class DISPLAY_MANAGER_EXPORT DisplayConfigurator
    : public NativeDisplayObserver {
 public:
  enum : uint64_t {
    INVALID_CLIENT_ID = 0,
  };

  using ConfigurationCallback = base::Callback<void(bool /* success */)>;

  using SetProtectionCallback = base::Callback<void(bool /* success */)>;

  // link_mask: The type of connected display links, which is a bitmask of
  // DisplayConnectionType values.
  // protection_mask: The desired protection methods, which is a bitmask of the
  // ContentProtectionMethod values.
  using QueryProtectionCallback =
      base::Callback<void(bool /* success */,
                          uint32_t /* link_mask */,
                          uint32_t /* protection_mask */)>;
  using DisplayControlCallback = base::OnceCallback<void(bool /* success */)>;

  using DisplayStateList = std::vector<DisplaySnapshot*>;

  // Mapping a display_id to a protection request bitmask.
  using ContentProtections = std::map<int64_t, uint32_t>;

  class Observer {
   public:
    virtual ~Observer() {}

    // Called after the display mode has been changed. |display| contains the
    // just-applied configuration. Note that the X server is no longer grabbed
    // when this method is called, so the actual configuration could've changed
    // already.
    virtual void OnDisplayModeChanged(const DisplayStateList& displays) {}

    // Called after a display mode change attempt failed. |displays| contains
    // displays that are detected when failed.
    // |failed_new_state| is the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(
        const DisplayStateList& displays,
        MultipleDisplayState failed_new_state) {}

    // Called after the power state has been changed. |power_state| contains
    // the just-applied power state.
    virtual void OnPowerStateChanged(chromeos::DisplayPowerState power_state) {}
  };

  // Interface for classes that make decisions about which display state
  // should be used.
  class StateController {
   public:
    virtual ~StateController() {}

    // Called when displays are detected.
    virtual MultipleDisplayState GetStateForDisplayIds(
        const DisplayConfigurator::DisplayStateList& outputs) = 0;

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
    explicit TestApi(DisplayConfigurator* configurator)
        : configurator_(configurator) {}
    ~TestApi() {}

    // If |configure_timer_| is started, stops the timer, runs
    // ConfigureDisplays(), and returns true; returns false otherwise.
    bool TriggerConfigureTimeout() WARN_UNUSED_RESULT;

    // Gets the current delay of the |configure_timer_| if it's running, or zero
    // time delta otherwise.
    base::TimeDelta GetConfigureDelay() const;

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

  // The delay to perform configuration after RRNotify. See the comment for
  // |configure_timer_|.
  static const int kConfigureDelayMs = 1000;

  // The delay to perform configuration after waking up from suspend when in
  // multi display mode. Should be bigger than |kConfigureDelayMs|. Generally
  // big enough for external displays to be detected and added.
  // crbug.com/614624.
  static const int kResumeConfigureMultiDisplayDelayMs = 2000;

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
  const std::vector<DisplaySnapshot*>& cached_displays() const {
    return cached_displays_;
  }
  void set_state_controller(StateController* controller) {
    state_controller_ = controller;
  }
  void set_mirroring_controller(SoftwareMirroringController* controller) {
    mirroring_controller_ = controller;
  }
  void set_configure_display(bool configure_display) {
    configure_display_ = configure_display;
  }
  chromeos::DisplayPowerState current_power_state() const {
    return current_power_state_;
  }

  // Called when an external process no longer needs to control the display
  // and Chrome can take control.
  void TakeControl(DisplayControlCallback callback);

  // Called when an external process needs to control the display and thus
  // Chrome should relinquish it.
  void RelinquishControl(DisplayControlCallback callback);

  // Replaces |native_display_delegate_| with the delegate passed in and sets
  // |configure_display_| to true. Should be called before Init().
  void SetDelegateForTesting(
      std::unique_ptr<NativeDisplayDelegate> display_delegate);

  // Sets the initial value of |power_state_|.  Must be called before Start().
  void SetInitialDisplayPower(chromeos::DisplayPowerState power_state);

  // Initialization, must be called right after constructor.
  // |is_panel_fitting_enabled| indicates hardware panel fitting support.
  void Init(std::unique_ptr<NativeDisplayDelegate> delegate,
            bool is_panel_fitting_enabled);

  // Does initial configuration of displays during startup.
  void ForceInitialConfigure();

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
  void OnDisplaySnapshotsInvalidated() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets all the displays into pre-suspend mode; usually this means
  // configure them for their resume state. This allows faster resume on
  // machines where display configuration is slow. On completion of the display
  // configuration |callback| is executed synchronously or asynchronously.
  void SuspendDisplays(const ConfigurationCallback& callback);

  // Reprobes displays to handle changes made while the system was
  // suspended.
  void ResumeDisplays();

  // Registers a client for display protection and requests a client id. Returns
  // 0 if requesting failed.
  uint64_t RegisterContentProtectionClient();

  // Unregisters the client.
  void UnregisterContentProtectionClient(uint64_t client_id);

  // Queries link status and protection status. |callback| is used to respond
  // to the query.
  void QueryContentProtectionStatus(uint64_t client_id,
                                    int64_t display_id,
                                    const QueryProtectionCallback& callback);

  // Requests the desired protection methods.
  // |protection_mask| is the desired protection methods, which is a bitmask
  // of the ContentProtectionMethod values.
  // Returns true when the protection request has been made.
  void SetContentProtection(uint64_t client_id,
                            int64_t display_id,
                            uint32_t protection_mask,
                            const SetProtectionCallback& callback);

  // Returns true if there is at least one display on.
  bool IsDisplayOn() const;

  // Sets the gamma, degamma and correction matrix for |display_id| to the
  // values in |degamma_lut|, |gamma_lut| and |correction_matrix|.
  bool SetColorCorrection(int64_t display_id,
                          const std::vector<GammaRampRGBEntry>& degamma_lut,
                          const std::vector<GammaRampRGBEntry>& gamma_lut,
                          const std::vector<float>& correction_matrix);

 private:
  class DisplayLayoutManagerImpl;

  // Mapping a client to its protection request.
  using ProtectionRequests = std::map<uint64_t, ContentProtections>;

  // Updates |pending_*| members and applies the passed-in state. |callback| is
  // invoked (perhaps synchronously) on completion.
  void SetDisplayPowerInternal(chromeos::DisplayPowerState power_state,
                               int flags,
                               const ConfigurationCallback& callback);

  // Configures displays. Invoked by |configure_timer_|.
  void ConfigureDisplays();

  // Notifies observers about an attempted state change.
  void NotifyDisplayStateObservers(bool success,
                                   MultipleDisplayState attempted_state);

  // Notifies observers about a power state change.
  void NotifyPowerStateObservers();

  // Returns the display state that should be used with |cached_displays_| while
  // in |power_state|.
  MultipleDisplayState ChooseDisplayState(
      chromeos::DisplayPowerState power_state) const;

  // Applies display protections according to requests.
  bool ApplyProtections(const ContentProtections& requests);

  // If |configuration_task_| isn't initialized, initializes it and starts the
  // configuration task.
  void RunPendingConfiguration();

  // Callback for |configuration_taks_|. When the configuration process finishes
  // this is called with the result (|success|) and the updated display state.
  void OnConfigured(bool success,
                    const std::vector<DisplaySnapshot*>& displays,
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

  // Content protection callbacks called by the tasks when they finish. These
  // are responsible for destroying the task, replying to the caller that made
  // the task and starting the a new content protection task if one is queued.
  void OnContentProtectionQueried(
      uint64_t client_id,
      int64_t display_id,
      QueryContentProtectionTask::Response response);
  void OnSetContentProtectionCompleted(uint64_t client_id,
                                       int64_t display_id,
                                       uint32_t desired_method_mask,
                                       bool success);
  void OnContentProtectionClientUnregistered(bool success);

  // Callbacks used to signal when the native platform has released/taken
  // display control.
  void OnDisplayControlTaken(DisplayControlCallback callback, bool success);
  void OnDisplayControlRelinquished(DisplayControlCallback callback,
                                    bool success);

  // Helper function that sends the actual command.
  // |callback| is called upon completion of the relinquish command.
  // |success| is the result from calling SetDisplayPowerInternal() in
  // RelinquishDisplay().
  void SendRelinquishDisplayControl(DisplayControlCallback callback,
                                    bool success);

  StateController* state_controller_;
  SoftwareMirroringController* mirroring_controller_;
  std::unique_ptr<NativeDisplayDelegate> native_display_delegate_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled. If this flag is set to false, any attempts to change the
  // display configuration will immediately fail without changing the state.
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

  // The power state used by RunPendingConfiguration(). May be
  // |requested_power_state_| or DISPLAY_POWER_ALL_OFF for suspend.
  chromeos::DisplayPowerState pending_power_state_;

  // True if |pending_power_state_| has been changed.
  bool has_pending_power_state_;

  // Bitwise-or value of the |kSetDisplayPower*| flags defined above.
  int pending_power_flags_;

  // List of callbacks from callers waiting for the display configuration to
  // start/finish. Note these callbacks belong to the pending request, not a
  // request currently active.
  std::vector<ConfigurationCallback> queued_configuration_callbacks_;

  // List of callbacks belonging to the currently running display configuration
  // task.
  std::vector<ConfigurationCallback> in_progress_configuration_callbacks_;

  base::queue<base::Closure> content_protection_tasks_;
  base::queue<QueryProtectionCallback> query_protection_callbacks_;
  base::queue<SetProtectionCallback> set_protection_callbacks_;

  // True if the caller wants to force the display configuration process.
  bool force_configure_;

  // Most-recently-used display configuration. Note that the actual
  // configuration changes asynchronously.
  DisplayStateList cached_displays_;

  base::ObserverList<Observer> observers_;

  // The timer to delay configuring displays. This is used to aggregate multiple
  // display configuration events when they are reported in short time spans.
  base::OneShotTimer configure_timer_;

  // Id for next display protection client.
  uint64_t next_display_protection_client_id_;

  // Display protection requests of each client.
  ProtectionRequests client_protection_requests_;

  // Display controlled by an external entity.
  bool display_externally_controlled_;

  // True if a TakeControl or RelinquishControl has been called but the response
  // hasn't arrived yet.
  bool display_control_changing_;

  // Whether the displays are currently suspended.
  bool displays_suspended_;

  std::unique_ptr<DisplayLayoutManager> layout_manager_;

  std::unique_ptr<UpdateDisplayConfigurationTask> configuration_task_;

  // This must be the last variable.
  base::WeakPtrFactory<DisplayConfigurator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurator);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_CONFIGURATOR_H_
