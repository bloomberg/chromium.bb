// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "ui/display/chromeos/display_mode.h"
#include "ui/display/chromeos/display_snapshot.h"
#include "ui/display/chromeos/native_display_delegate.h"
#include "ui/display/display_switches.h"

#if defined(USE_OZONE)
#include "ui/display/chromeos/ozone/native_display_delegate_ozone.h"
#include "ui/display/chromeos/ozone/touchscreen_delegate_ozone.h"
#elif defined(USE_X11)
#include "ui/display/chromeos/x11/native_display_delegate_x11.h"
#include "ui/display/chromeos/x11/touchscreen_delegate_x11.h"
#endif

namespace ui {

namespace {

typedef std::vector<const DisplayMode*> DisplayModeList;

// The delay to perform configuration after RRNotify.  See the comment
// in |Dispatch()|.
const int kConfigureDelayMs = 500;

// Returns a string describing |state|.
std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "ALL_ON";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "ALL_OFF";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "INTERNAL_OFF_EXTERNAL_ON";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "INTERNAL_ON_EXTERNAL_OFF";
    default:
      return "unknown (" + base::IntToString(state) + ")";
  }
}

// Returns a string describing |state|.
std::string OutputStateToString(OutputState state) {
  switch (state) {
    case OUTPUT_STATE_INVALID:
      return "INVALID";
    case OUTPUT_STATE_HEADLESS:
      return "HEADLESS";
    case OUTPUT_STATE_SINGLE:
      return "SINGLE";
    case OUTPUT_STATE_DUAL_MIRROR:
      return "DUAL_MIRROR";
    case OUTPUT_STATE_DUAL_EXTENDED:
      return "DUAL_EXTENDED";
  }
  NOTREACHED() << "Unknown state " << state;
  return "INVALID";
}

// Returns the number of outputs in |outputs| that should be turned on, per
// |state|.  If |output_power| is non-NULL, it is updated to contain the
// on/off state of each corresponding entry in |outputs|.
int GetOutputPower(
    const std::vector<DisplayConfigurator::DisplayState>& outputs,
    chromeos::DisplayPowerState state,
    std::vector<bool>* output_power) {
  int num_on_outputs = 0;
  if (output_power)
    output_power->resize(outputs.size());

  for (size_t i = 0; i < outputs.size(); ++i) {
    bool internal = outputs[i].display->type() == OUTPUT_TYPE_INTERNAL;
    bool on =
        state == chromeos::DISPLAY_POWER_ALL_ON ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON &&
         !internal) ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (output_power)
      (*output_power)[i] = on;
    if (on)
      num_on_outputs++;
  }
  return num_on_outputs;
}

}  // namespace

DisplayConfigurator::CoordinateTransformation::CoordinateTransformation()
    : x_scale(1.0),
      x_offset(0.0),
      y_scale(1.0),
      y_offset(0.0) {}

DisplayConfigurator::DisplayState::DisplayState()
    : display(NULL),
      touch_device_id(0),
      selected_mode(NULL),
      mirror_mode(NULL) {}

bool DisplayConfigurator::TestApi::TriggerConfigureTimeout() {
  if (configurator_->configure_timer_.get() &&
      configurator_->configure_timer_->IsRunning()) {
    configurator_->configure_timer_.reset();
    configurator_->ConfigureOutputs();
    return true;
  } else {
    return false;
  }
}

// static
const DisplayMode* DisplayConfigurator::FindDisplayModeMatchingSize(
    const DisplaySnapshot& output,
    const gfx::Size& size) {
  const DisplayMode* best_mode = NULL;
  for (DisplayModeList::const_iterator it = output.modes().begin();
       it != output.modes().end();
       ++it) {
    const DisplayMode* mode = *it;

    if (mode->size() != size)
      continue;

    if (!best_mode) {
      best_mode = mode;
      continue;
    }

    if (mode->is_interlaced()) {
      if (!best_mode->is_interlaced())
        continue;
    } else {
      // Reset the best rate if the non interlaced is
      // found the first time.
      if (best_mode->is_interlaced()) {
        best_mode = mode;
        continue;
      }
    }
    if (mode->refresh_rate() < best_mode->refresh_rate())
      continue;

    best_mode = mode;
  }

  return best_mode;
}

DisplayConfigurator::DisplayConfigurator()
    : state_controller_(NULL),
      mirroring_controller_(NULL),
      is_panel_fitting_enabled_(false),
      configure_display_(base::SysInfo::IsRunningOnChromeOS()),
      output_state_(OUTPUT_STATE_INVALID),
      power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      next_output_protection_client_id_(1) {}

DisplayConfigurator::~DisplayConfigurator() {
  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);
}

void DisplayConfigurator::SetNativeDisplayDelegateForTesting(
    scoped_ptr<NativeDisplayDelegate> delegate) {
  DCHECK(!native_display_delegate_);

  native_display_delegate_ = delegate.Pass();
  native_display_delegate_->AddObserver(this);
  configure_display_ = true;
}

void DisplayConfigurator::SetTouchscreenDelegateForTesting(
    scoped_ptr<TouchscreenDelegate> delegate) {
  DCHECK(!touchscreen_delegate_);

  touchscreen_delegate_ = delegate.Pass();
}

void DisplayConfigurator::SetInitialDisplayPower(
    chromeos::DisplayPowerState power_state) {
  DCHECK_EQ(output_state_, OUTPUT_STATE_INVALID);
  power_state_ = power_state;
}

void DisplayConfigurator::Init(bool is_panel_fitting_enabled) {
  is_panel_fitting_enabled_ = is_panel_fitting_enabled;
  if (!configure_display_)
    return;

  if (!native_display_delegate_) {
#if defined(USE_OZONE)
    native_display_delegate_.reset(new NativeDisplayDelegateOzone());
#elif defined(USE_X11)
    native_display_delegate_.reset(new NativeDisplayDelegateX11());
#else
    NOTREACHED();
#endif
    native_display_delegate_->AddObserver(this);
  }

  if (!touchscreen_delegate_) {
#if defined(USE_OZONE)
    touchscreen_delegate_.reset(new TouchscreenDelegateOzone());
#elif defined(USE_X11)
    touchscreen_delegate_.reset(new TouchscreenDelegateX11());
#else
    NOTREACHED();
#endif
  }
}

void DisplayConfigurator::ForceInitialConfigure(
    uint32_t background_color_argb) {
  if (!configure_display_)
    return;

  native_display_delegate_->GrabServer();
  native_display_delegate_->Initialize();

  UpdateCachedOutputs();
  if (cached_outputs_.size() > 1 && background_color_argb)
    native_display_delegate_->SetBackgroundColor(background_color_argb);
  const OutputState new_state = ChooseOutputState(power_state_);
  const bool success =
      EnterStateOrFallBackToSoftwareMirroring(new_state, power_state_);

  // Force the DPMS on chrome startup as the driver doesn't always detect
  // that all displays are on when signing out.
  native_display_delegate_->ForceDPMSOn();
  native_display_delegate_->UngrabServer();
  NotifyObservers(success, new_state);
}

bool DisplayConfigurator::ApplyProtections(const DisplayProtections& requests) {
  for (DisplayStateList::const_iterator it = cached_outputs_.begin();
       it != cached_outputs_.end();
       ++it) {
    uint32_t all_desired = 0;
    DisplayProtections::const_iterator request_it =
        requests.find(it->display->display_id());
    if (request_it != requests.end())
      all_desired = request_it->second;
    switch (it->display->type()) {
      case OUTPUT_TYPE_UNKNOWN:
        return false;
      // DisplayPort, DVI, and HDMI all support HDCP.
      case OUTPUT_TYPE_DISPLAYPORT:
      case OUTPUT_TYPE_DVI:
      case OUTPUT_TYPE_HDMI: {
        HDCPState new_desired_state =
            (all_desired & OUTPUT_PROTECTION_METHOD_HDCP) ?
                HDCP_STATE_DESIRED : HDCP_STATE_UNDESIRED;
        if (!native_display_delegate_->SetHDCPState(*it->display,
                                                    new_desired_state))
          return false;
        break;
      }
      case OUTPUT_TYPE_INTERNAL:
      case OUTPUT_TYPE_VGA:
      case OUTPUT_TYPE_NETWORK:
        // No protections for these types. Do nothing.
        break;
      case OUTPUT_TYPE_NONE:
        NOTREACHED();
        break;
    }
  }

  return true;
}

DisplayConfigurator::OutputProtectionClientId
DisplayConfigurator::RegisterOutputProtectionClient() {
  if (!configure_display_)
    return kInvalidClientId;

  return next_output_protection_client_id_++;
}

void DisplayConfigurator::UnregisterOutputProtectionClient(
    OutputProtectionClientId client_id) {
  client_protection_requests_.erase(client_id);

  DisplayProtections protections;
  for (ProtectionRequests::const_iterator it =
           client_protection_requests_.begin();
       it != client_protection_requests_.end();
       ++it) {
    for (DisplayProtections::const_iterator it2 = it->second.begin();
         it2 != it->second.end();
         ++it2) {
      protections[it2->first] |= it2->second;
    }
  }

  ApplyProtections(protections);
}

bool DisplayConfigurator::QueryOutputProtectionStatus(
    OutputProtectionClientId client_id,
    int64_t display_id,
    uint32_t* link_mask,
    uint32_t* protection_mask) {
  if (!configure_display_)
    return false;

  uint32_t enabled = 0;
  uint32_t unfulfilled = 0;
  *link_mask = 0;
  for (DisplayStateList::const_iterator it = cached_outputs_.begin();
       it != cached_outputs_.end();
       ++it) {
    if (it->display->display_id() != display_id)
      continue;
    *link_mask |= it->display->type();
    switch (it->display->type()) {
      case OUTPUT_TYPE_UNKNOWN:
        return false;
      // DisplayPort, DVI, and HDMI all support HDCP.
      case OUTPUT_TYPE_DISPLAYPORT:
      case OUTPUT_TYPE_DVI:
      case OUTPUT_TYPE_HDMI: {
        HDCPState state;
        if (!native_display_delegate_->GetHDCPState(*it->display, &state))
          return false;
        if (state == HDCP_STATE_ENABLED)
          enabled |= OUTPUT_PROTECTION_METHOD_HDCP;
        else
          unfulfilled |= OUTPUT_PROTECTION_METHOD_HDCP;
        break;
      }
      case OUTPUT_TYPE_INTERNAL:
      case OUTPUT_TYPE_VGA:
      case OUTPUT_TYPE_NETWORK:
        // No protections for these types. Do nothing.
        break;
      case OUTPUT_TYPE_NONE:
        NOTREACHED();
        break;
    }
  }

  // Don't reveal protections requested by other clients.
  ProtectionRequests::iterator it = client_protection_requests_.find(client_id);
  if (it != client_protection_requests_.end()) {
    uint32_t requested_mask = 0;
    if (it->second.find(display_id) != it->second.end())
      requested_mask = it->second[display_id];
    *protection_mask = enabled & ~unfulfilled & requested_mask;
  } else {
    *protection_mask = 0;
  }
  return true;
}

bool DisplayConfigurator::EnableOutputProtection(
    OutputProtectionClientId client_id,
    int64_t display_id,
    uint32_t desired_method_mask) {
  if (!configure_display_)
    return false;

  DisplayProtections protections;
  for (ProtectionRequests::const_iterator it =
           client_protection_requests_.begin();
       it != client_protection_requests_.end();
       ++it) {
    for (DisplayProtections::const_iterator it2 = it->second.begin();
         it2 != it->second.end();
         ++it2) {
      if (it->first == client_id && it2->first == display_id)
        continue;
      protections[it2->first] |= it2->second;
    }
  }
  protections[display_id] |= desired_method_mask;

  if (!ApplyProtections(protections))
    return false;

  if (desired_method_mask == OUTPUT_PROTECTION_METHOD_NONE) {
    if (client_protection_requests_.find(client_id) !=
        client_protection_requests_.end()) {
      client_protection_requests_[client_id].erase(display_id);
      if (client_protection_requests_[client_id].size() == 0)
        client_protection_requests_.erase(client_id);
    }
  } else {
    client_protection_requests_[client_id][display_id] = desired_method_mask;
  }

  return true;
}

std::vector<ui::ColorCalibrationProfile>
DisplayConfigurator::GetAvailableColorCalibrationProfiles(int64_t display_id) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDisplayColorCalibration)) {
    for (size_t i = 0; i < cached_outputs_.size(); ++i) {
      if (cached_outputs_[i].display &&
          cached_outputs_[i].display->display_id() == display_id) {
        return native_display_delegate_->GetAvailableColorCalibrationProfiles(
            *cached_outputs_[i].display);
      }
    }
  }

  return std::vector<ui::ColorCalibrationProfile>();
}

bool DisplayConfigurator::SetColorCalibrationProfile(
    int64_t display_id,
    ui::ColorCalibrationProfile new_profile) {
  for (size_t i = 0; i < cached_outputs_.size(); ++i) {
    if (cached_outputs_[i].display &&
        cached_outputs_[i].display->display_id() == display_id) {
      return native_display_delegate_->SetColorCalibrationProfile(
          *cached_outputs_[i].display, new_profile);
    }
  }

  return false;
}

void DisplayConfigurator::PrepareForExit() {
  configure_display_ = false;
}

bool DisplayConfigurator::SetDisplayPower(
    chromeos::DisplayPowerState power_state,
    int flags) {
  if (!configure_display_)
    return false;

  VLOG(1) << "SetDisplayPower: power_state="
          << DisplayPowerStateToString(power_state) << " flags=" << flags
          << ", configure timer="
          << ((configure_timer_.get() && configure_timer_->IsRunning()) ?
                  "Running" : "Stopped");
  if (power_state == power_state_ && !(flags & kSetDisplayPowerForceProbe))
    return true;

  native_display_delegate_->GrabServer();
  UpdateCachedOutputs();

  const OutputState new_state = ChooseOutputState(power_state);
  bool attempted_change = false;
  bool success = false;

  bool only_if_single_internal_display =
      flags & kSetDisplayPowerOnlyIfSingleInternalDisplay;
  bool single_internal_display =
      cached_outputs_.size() == 1 &&
      cached_outputs_[0].display->type() == OUTPUT_TYPE_INTERNAL;
  if (single_internal_display || !only_if_single_internal_display) {
    success = EnterStateOrFallBackToSoftwareMirroring(new_state, power_state);
    attempted_change = true;

    // Force the DPMS on since the driver doesn't always detect that it
    // should turn on. This is needed when coming back from idle suspend.
    if (success && power_state != chromeos::DISPLAY_POWER_ALL_OFF)
      native_display_delegate_->ForceDPMSOn();
  }

  native_display_delegate_->UngrabServer();
  if (attempted_change)
    NotifyObservers(success, new_state);
  return true;
}

bool DisplayConfigurator::SetDisplayMode(OutputState new_state) {
  if (!configure_display_)
    return false;

  VLOG(1) << "SetDisplayMode: state=" << OutputStateToString(new_state);
  if (output_state_ == new_state) {
    // Cancel software mirroring if the state is moving from
    // OUTPUT_STATE_DUAL_EXTENDED to OUTPUT_STATE_DUAL_EXTENDED.
    if (mirroring_controller_ && new_state == OUTPUT_STATE_DUAL_EXTENDED)
      mirroring_controller_->SetSoftwareMirroring(false);
    NotifyObservers(true, new_state);
    return true;
  }

  native_display_delegate_->GrabServer();
  UpdateCachedOutputs();
  const bool success =
      EnterStateOrFallBackToSoftwareMirroring(new_state, power_state_);
  native_display_delegate_->UngrabServer();

  NotifyObservers(success, new_state);
  return success;
}

void DisplayConfigurator::OnConfigurationChanged() {
  // Configure outputs with |kConfigureDelayMs| delay,
  // so that time-consuming ConfigureOutputs() won't be called multiple times.
  if (configure_timer_.get()) {
    configure_timer_->Reset();
  } else {
    configure_timer_.reset(new base::OneShotTimer<DisplayConfigurator>());
    configure_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
        this,
        &DisplayConfigurator::ConfigureOutputs);
  }
}

void DisplayConfigurator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DisplayConfigurator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DisplayConfigurator::SuspendDisplays() {
  // If the display is off due to user inactivity and there's only a single
  // internal display connected, switch to the all-on state before
  // suspending.  This shouldn't be very noticeable to the user since the
  // backlight is off at this point, and doing this lets us resume directly
  // into the "on" state, which greatly reduces resume times.
  if (power_state_ == chromeos::DISPLAY_POWER_ALL_OFF) {
    SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                    kSetDisplayPowerOnlyIfSingleInternalDisplay);

    // We need to make sure that the monitor configuration we just did actually
    // completes before we return, because otherwise the X message could be
    // racing with the HandleSuspendReadiness message.
    native_display_delegate_->SyncWithServer();
  }
}

void DisplayConfigurator::ResumeDisplays() {
  // Force probing to ensure that we pick up any changes that were made
  // while the system was suspended.
  SetDisplayPower(power_state_, kSetDisplayPowerForceProbe);
}

void DisplayConfigurator::UpdateCachedOutputs() {
  std::vector<DisplaySnapshot*> snapshots =
      native_display_delegate_->GetOutputs();

  cached_outputs_.clear();
  for (size_t i = 0; i < snapshots.size(); ++i) {
    DisplayState display_state;
    display_state.display = snapshots[i];
    cached_outputs_.push_back(display_state);
  }

  touchscreen_delegate_->AssociateTouchscreens(&cached_outputs_);

  // Set |selected_mode| fields.
  for (size_t i = 0; i < cached_outputs_.size(); ++i) {
    DisplayState* output = &cached_outputs_[i];
    if (output->display->has_proper_display_id()) {
      gfx::Size size;
      if (state_controller_ && state_controller_->GetResolutionForDisplayId(
                                   output->display->display_id(), &size)) {
        output->selected_mode =
            FindDisplayModeMatchingSize(*output->display, size);
      }
    }
    // Fall back to native mode.
    if (!output->selected_mode)
      output->selected_mode = output->display->native_mode();
  }

  // Set |mirror_mode| fields.
  if (cached_outputs_.size() == 2) {
    bool one_is_internal =
        cached_outputs_[0].display->type() == OUTPUT_TYPE_INTERNAL;
    bool two_is_internal =
        cached_outputs_[1].display->type() == OUTPUT_TYPE_INTERNAL;
    int internal_outputs =
        (one_is_internal ? 1 : 0) + (two_is_internal ? 1 : 0);
    DCHECK_LT(internal_outputs, 2);
    LOG_IF(WARNING, internal_outputs == 2) << "Two internal outputs detected.";

    bool can_mirror = false;
    for (int attempt = 0; !can_mirror && attempt < 2; ++attempt) {
      // Try preserving external output's aspect ratio on the first attempt.
      // If that fails, fall back to the highest matching resolution.
      bool preserve_aspect = attempt == 0;

      if (internal_outputs == 1) {
        if (one_is_internal) {
          can_mirror = FindMirrorMode(&cached_outputs_[0],
                                      &cached_outputs_[1],
                                      is_panel_fitting_enabled_,
                                      preserve_aspect);
        } else {
          DCHECK(two_is_internal);
          can_mirror = FindMirrorMode(&cached_outputs_[1],
                                      &cached_outputs_[0],
                                      is_panel_fitting_enabled_,
                                      preserve_aspect);
        }
      } else {  // if (internal_outputs == 0)
        // No panel fitting for external outputs, so fall back to exact match.
        can_mirror = FindMirrorMode(
            &cached_outputs_[0], &cached_outputs_[1], false, preserve_aspect);
        if (!can_mirror && preserve_aspect) {
          // FindMirrorMode() will try to preserve aspect ratio of what it
          // thinks is external display, so if it didn't succeed with one, maybe
          // it will succeed with the other.  This way we will have the correct
          // aspect ratio on at least one of them.
          can_mirror = FindMirrorMode(
              &cached_outputs_[1], &cached_outputs_[0], false, preserve_aspect);
        }
      }
    }
  }
}

bool DisplayConfigurator::FindMirrorMode(DisplayState* internal_output,
                                         DisplayState* external_output,
                                         bool try_panel_fitting,
                                         bool preserve_aspect) {
  const DisplayMode* internal_native_info =
      internal_output->display->native_mode();
  const DisplayMode* external_native_info =
      external_output->display->native_mode();
  if (!internal_native_info || !external_native_info)
    return false;

  // Check if some external output resolution can be mirrored on internal.
  // Prefer the modes in the order that X sorts them, assuming this is the order
  // in which they look better on the monitor.
  for (DisplayModeList::const_iterator external_it =
           external_output->display->modes().begin();
       external_it != external_output->display->modes().end();
       ++external_it) {
    const DisplayMode& external_info = **external_it;
    bool is_native_aspect_ratio =
        external_native_info->size().width() * external_info.size().height() ==
        external_native_info->size().height() * external_info.size().width();
    if (preserve_aspect && !is_native_aspect_ratio)
      continue;  // Allow only aspect ratio preserving modes for mirroring.

    // Try finding an exact match.
    for (DisplayModeList::const_iterator internal_it =
             internal_output->display->modes().begin();
         internal_it != internal_output->display->modes().end();
         ++internal_it) {
      const DisplayMode& internal_info = **internal_it;
      if (internal_info.size().width() == external_info.size().width() &&
          internal_info.size().height() == external_info.size().height() &&
          internal_info.is_interlaced() == external_info.is_interlaced()) {
        internal_output->mirror_mode = *internal_it;
        external_output->mirror_mode = *external_it;
        return true;  // Mirror mode found.
      }
    }

    // Try to create a matching internal output mode by panel fitting.
    if (try_panel_fitting) {
      // We can downscale by 1.125, and upscale indefinitely. Downscaling looks
      // ugly, so, can fit == can upscale. Also, internal panels don't support
      // fitting interlaced modes.
      bool can_fit = internal_native_info->size().width() >=
                         external_info.size().width() &&
                     internal_native_info->size().height() >=
                         external_info.size().height() &&
                     !external_info.is_interlaced();
      if (can_fit) {
        native_display_delegate_->AddMode(*internal_output->display,
                                          *external_it);
        internal_output->display->add_mode(*external_it);
        internal_output->mirror_mode = *external_it;
        external_output->mirror_mode = *external_it;
        return true;  // Mirror mode created.
      }
    }
  }

  return false;
}

void DisplayConfigurator::ConfigureOutputs() {
  configure_timer_.reset();

  if (!configure_display_)
    return;

  native_display_delegate_->GrabServer();
  UpdateCachedOutputs();
  const OutputState new_state = ChooseOutputState(power_state_);
  const bool success =
      EnterStateOrFallBackToSoftwareMirroring(new_state, power_state_);
  native_display_delegate_->UngrabServer();

  NotifyObservers(success, new_state);
}

void DisplayConfigurator::NotifyObservers(bool success,
                                          OutputState attempted_state) {
  if (success) {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChanged(cached_outputs_));
  } else {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChangeFailed(attempted_state));
  }
}

bool DisplayConfigurator::EnterStateOrFallBackToSoftwareMirroring(
    OutputState output_state,
    chromeos::DisplayPowerState power_state) {
  bool success = EnterState(output_state, power_state);
  if (mirroring_controller_) {
    bool enable_software_mirroring = false;
    if (!success && output_state == OUTPUT_STATE_DUAL_MIRROR) {
      if (output_state_ != OUTPUT_STATE_DUAL_EXTENDED ||
          power_state_ != power_state)
        EnterState(OUTPUT_STATE_DUAL_EXTENDED, power_state);
      enable_software_mirroring = success =
          output_state_ == OUTPUT_STATE_DUAL_EXTENDED;
    }
    mirroring_controller_->SetSoftwareMirroring(enable_software_mirroring);
  }
  return success;
}

bool DisplayConfigurator::EnterState(OutputState output_state,
                                     chromeos::DisplayPowerState power_state) {
  std::vector<bool> output_power;
  int num_on_outputs =
      GetOutputPower(cached_outputs_, power_state, &output_power);
  VLOG(1) << "EnterState: output=" << OutputStateToString(output_state)
          << " power=" << DisplayPowerStateToString(power_state);

  // Framebuffer dimensions.
  gfx::Size size;

  std::vector<gfx::Point> new_origins(cached_outputs_.size(), gfx::Point());
  std::vector<const DisplayMode*> new_mode;
  for (size_t i = 0; i < cached_outputs_.size(); ++i)
    new_mode.push_back(cached_outputs_[i].display->current_mode());

  switch (output_state) {
    case OUTPUT_STATE_INVALID:
      NOTREACHED() << "Ignoring request to enter invalid state with "
                   << cached_outputs_.size() << " connected output(s)";
      return false;
    case OUTPUT_STATE_HEADLESS:
      if (cached_outputs_.size() != 0) {
        LOG(WARNING) << "Ignoring request to enter headless mode with "
                     << cached_outputs_.size() << " connected output(s)";
        return false;
      }
      break;
    case OUTPUT_STATE_SINGLE: {
      // If there are multiple outputs connected, only one should be turned on.
      if (cached_outputs_.size() != 1 && num_on_outputs != 1) {
        LOG(WARNING) << "Ignoring request to enter single mode with "
                     << cached_outputs_.size() << " connected outputs and "
                     << num_on_outputs << " turned on";
        return false;
      }

      for (size_t i = 0; i < cached_outputs_.size(); ++i) {
        DisplayState* output = &cached_outputs_[i];
        new_mode[i] = output_power[i] ? output->selected_mode : NULL;

        if (output_power[i] || cached_outputs_.size() == 1) {
          const DisplayMode* mode_info = output->selected_mode;
          if (!mode_info)
            return false;
          if (mode_info->size() == gfx::Size(1024, 768)) {
            VLOG(1) << "Potentially misdetecting display(1024x768):"
                    << " outputs size=" << cached_outputs_.size()
                    << ", num_on_outputs=" << num_on_outputs
                    << ", current size:" << size.width() << "x" << size.height()
                    << ", i=" << i << ", output=" << output->display->ToString()
                    << ", display_mode=" << mode_info->ToString();
          }
          size = mode_info->size();
        }
      }
      break;
    }
    case OUTPUT_STATE_DUAL_MIRROR: {
      if (cached_outputs_.size() != 2 ||
          (num_on_outputs != 0 && num_on_outputs != 2)) {
        LOG(WARNING) << "Ignoring request to enter mirrored mode with "
                     << cached_outputs_.size() << " connected output(s) and "
                     << num_on_outputs << " turned on";
        return false;
      }

      const DisplayMode* mode_info = cached_outputs_[0].mirror_mode;
      if (!mode_info)
        return false;
      size = mode_info->size();

      for (size_t i = 0; i < cached_outputs_.size(); ++i) {
        DisplayState* output = &cached_outputs_[i];
        new_mode[i] = output_power[i] ? output->mirror_mode : NULL;
        if (output->touch_device_id) {
          // CTM needs to be calculated if aspect preserving scaling is used.
          // Otherwise, assume it is full screen, and use identity CTM.
          if (output->mirror_mode != output->display->native_mode() &&
              output->display->is_aspect_preserving_scaling()) {
            output->transform = GetMirrorModeCTM(*output);
            mirrored_display_area_ratio_map_[output->touch_device_id] =
                GetMirroredDisplayAreaRatio(*output);
          }
        }
      }
      break;
    }
    case OUTPUT_STATE_DUAL_EXTENDED: {
      if (cached_outputs_.size() != 2 ||
          (num_on_outputs != 0 && num_on_outputs != 2)) {
        LOG(WARNING) << "Ignoring request to enter extended mode with "
                     << cached_outputs_.size() << " connected output(s) and "
                     << num_on_outputs << " turned on";
        return false;
      }

      for (size_t i = 0; i < cached_outputs_.size(); ++i) {
        DisplayState* output = &cached_outputs_[i];
        new_origins[i].set_y(size.height() ? size.height() + kVerticalGap : 0);
        new_mode[i] = output_power[i] ? output->selected_mode : NULL;

        // Retain the full screen size even if all outputs are off so the
        // same desktop configuration can be restored when the outputs are
        // turned back on.
        const DisplayMode* mode_info = cached_outputs_[i].selected_mode;
        if (!mode_info)
          return false;

        size.set_width(std::max<int>(size.width(), mode_info->size().width()));
        size.set_height(size.height() + (size.height() ? kVerticalGap : 0) +
                        mode_info->size().height());
      }

      for (size_t i = 0; i < cached_outputs_.size(); ++i) {
        DisplayState* output = &cached_outputs_[i];
        if (output->touch_device_id)
          output->transform = GetExtendedModeCTM(*output, new_origins[i], size);
      }
      break;
    }
  }

  // Finally, apply the desired changes.
  bool all_succeeded = true;
  if (!cached_outputs_.empty()) {
    native_display_delegate_->CreateFrameBuffer(size);
    for (size_t i = 0; i < cached_outputs_.size(); ++i) {
      const DisplayState& output = cached_outputs_[i];
      bool configure_succeeded = false;

      while (true) {
        if (native_display_delegate_->Configure(
                *output.display, new_mode[i], new_origins[i])) {
          output.display->set_current_mode(new_mode[i]);
          output.display->set_origin(new_origins[i]);

          configure_succeeded = true;
          break;
        }

        const DisplayMode* mode_info = new_mode[i];
        if (!mode_info)
          break;

        // Find the mode with the next-best resolution and see if that can
        // be set.
        int best_mode_pixels = 0;

        int current_mode_pixels = mode_info->size().GetArea();
        for (DisplayModeList::const_iterator it =
                 output.display->modes().begin();
             it != output.display->modes().end();
             it++) {
          int pixel_count = (*it)->size().GetArea();
          if ((pixel_count < current_mode_pixels) &&
              (pixel_count > best_mode_pixels)) {
            new_mode[i] = *it;
            best_mode_pixels = pixel_count;
          }
        }

        if (best_mode_pixels == 0)
          break;
      }

      if (configure_succeeded) {
        if (output.touch_device_id)
          touchscreen_delegate_->ConfigureCTM(output.touch_device_id,
                                              output.transform);
      } else {
        all_succeeded = false;
      }

      // If we are trying to set mirror mode and one of the modesets fails,
      // then the two monitors will be mis-matched.  In this case, return
      // false to let the observers be aware.
      if (output_state == OUTPUT_STATE_DUAL_MIRROR && output_power[i] &&
          output.display->current_mode() != output.mirror_mode)
        all_succeeded = false;
    }
  }

  if (all_succeeded) {
    output_state_ = output_state;
    power_state_ = power_state;
  }
  return all_succeeded;
}

OutputState DisplayConfigurator::ChooseOutputState(
    chromeos::DisplayPowerState power_state) const {
  int num_on_outputs = GetOutputPower(cached_outputs_, power_state, NULL);
  switch (cached_outputs_.size()) {
    case 0:
      return OUTPUT_STATE_HEADLESS;
    case 1:
      return OUTPUT_STATE_SINGLE;
    case 2: {
      if (num_on_outputs == 1) {
        // If only one output is currently turned on, return the "single"
        // state so that its native mode will be used.
        return OUTPUT_STATE_SINGLE;
      } else {
        if (!state_controller_)
          return OUTPUT_STATE_DUAL_EXTENDED;
        // With either both outputs on or both outputs off, use one of the
        // dual modes.
        std::vector<int64_t> display_ids;
        for (size_t i = 0; i < cached_outputs_.size(); ++i) {
          // If display id isn't available, switch to extended mode.
          if (!cached_outputs_[i].display->has_proper_display_id())
            return OUTPUT_STATE_DUAL_EXTENDED;
          display_ids.push_back(cached_outputs_[i].display->display_id());
        }
        return state_controller_->GetStateForDisplayIds(display_ids);
      }
    }
    default:
      NOTREACHED();
  }
  return OUTPUT_STATE_INVALID;
}

DisplayConfigurator::CoordinateTransformation
DisplayConfigurator::GetMirrorModeCTM(const DisplayState& output) {
  CoordinateTransformation ctm;  // Default to identity
  const DisplayMode* native_mode_info = output.display->native_mode();
  const DisplayMode* mirror_mode_info = output.mirror_mode;

  if (!native_mode_info || !mirror_mode_info ||
      native_mode_info->size().height() == 0 ||
      mirror_mode_info->size().height() == 0 ||
      native_mode_info->size().width() == 0 ||
      mirror_mode_info->size().width() == 0)
    return ctm;

  float native_mode_ar = static_cast<float>(native_mode_info->size().width()) /
                         static_cast<float>(native_mode_info->size().height());
  float mirror_mode_ar = static_cast<float>(mirror_mode_info->size().width()) /
                         static_cast<float>(mirror_mode_info->size().height());

  if (mirror_mode_ar > native_mode_ar) {  // Letterboxing
    ctm.x_scale = 1.0;
    ctm.x_offset = 0.0;
    ctm.y_scale = mirror_mode_ar / native_mode_ar;
    ctm.y_offset = (native_mode_ar / mirror_mode_ar - 1.0) * 0.5;
    return ctm;
  }
  if (native_mode_ar > mirror_mode_ar) {  // Pillarboxing
    ctm.y_scale = 1.0;
    ctm.y_offset = 0.0;
    ctm.x_scale = native_mode_ar / mirror_mode_ar;
    ctm.x_offset = (mirror_mode_ar / native_mode_ar - 1.0) * 0.5;
    return ctm;
  }

  return ctm;  // Same aspect ratio - return identity
}

DisplayConfigurator::CoordinateTransformation
DisplayConfigurator::GetExtendedModeCTM(const DisplayState& output,
                                        const gfx::Point& new_origin,
                                        const gfx::Size& framebuffer_size) {
  CoordinateTransformation ctm;  // Default to identity
  const DisplayMode* mode_info = output.selected_mode;
  DCHECK(mode_info);
  if (!mode_info)
    return ctm;
  // An example of how to calculate the CTM.
  // Suppose we have 2 monitors, the first one has size 1366 x 768.
  // The second one has size 2560 x 1600
  // The total size of framebuffer is 2560 x 2428
  // where 2428 = 768 + 60 (hidden gap) + 1600
  // and the sceond monitor is translated to Point (0, 828) in the
  // framebuffer.
  // X will first map input event location to [0, 2560) x [0, 2428),
  // then apply CTM on it.
  // So to compute CTM, for monitor1, we have
  // x_scale = (1366 - 1) / (2560 - 1)
  // x_offset = 0 / (2560 - 1)
  // y_scale = (768 - 1) / (2428 - 1)
  // y_offset = 0 / (2428 -1)
  // For Monitor 2, we have
  // x_scale = (2560 - 1) / (2560 - 1)
  // x_offset = 0 / (2560 - 1)
  // y_scale = (1600 - 1) / (2428 - 1)
  // y_offset = 828 / (2428 -1)
  // See the unittest DisplayConfiguratorTest.CTMForMultiScreens.
  ctm.x_scale = static_cast<float>(mode_info->size().width() - 1) /
                (framebuffer_size.width() - 1);
  ctm.x_offset =
      static_cast<float>(new_origin.x()) / (framebuffer_size.width() - 1);
  ctm.y_scale = static_cast<float>(mode_info->size().height() - 1) /
                (framebuffer_size.height() - 1);
  ctm.y_offset =
      static_cast<float>(new_origin.y()) / (framebuffer_size.height() - 1);
  return ctm;
}

float DisplayConfigurator::GetMirroredDisplayAreaRatio(
    const DisplayState& output) {
  float area_ratio = 1.0f;
  const DisplayMode* native_mode_info = output.display->native_mode();
  const DisplayMode* mirror_mode_info = output.mirror_mode;

  if (!native_mode_info || !mirror_mode_info ||
      native_mode_info->size().height() == 0 ||
      mirror_mode_info->size().height() == 0 ||
      native_mode_info->size().width() == 0 ||
      mirror_mode_info->size().width() == 0)
    return area_ratio;

  float width_ratio = static_cast<float>(mirror_mode_info->size().width()) /
                      static_cast<float>(native_mode_info->size().width());
  float height_ratio = static_cast<float>(mirror_mode_info->size().height()) /
                       static_cast<float>(native_mode_info->size().height());

  area_ratio = width_ratio * height_ratio;
  return area_ratio;
}

}  // namespace ui
