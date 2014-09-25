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
#include "ui/display/display_switches.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

namespace {

typedef std::vector<const DisplayMode*> DisplayModeList;

// The delay to perform configuration after RRNotify. See the comment for
// |configure_timer_|.
const int kConfigureDelayMs = 500;

// The delay spent before reading the display configuration after coming out of
// suspend. While coming out of suspend the display state may be updating. This
// is used to wait until the hardware had a chance to update the display state
// such that we read an up to date state.
const int kResumeDelayMs = 500;

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
std::string DisplayStateToString(MultipleDisplayState state) {
  switch (state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      return "INVALID";
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      return "HEADLESS";
    case MULTIPLE_DISPLAY_STATE_SINGLE:
      return "SINGLE";
    case MULTIPLE_DISPLAY_STATE_DUAL_MIRROR:
      return "DUAL_MIRROR";
    case MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED:
      return "DUAL_EXTENDED";
  }
  NOTREACHED() << "Unknown state " << state;
  return "INVALID";
}

// Returns the number of displays in |displays| that should be turned on, per
// |state|.  If |display_power| is non-NULL, it is updated to contain the
// on/off state of each corresponding entry in |displays|.
int GetDisplayPower(
    const std::vector<DisplayConfigurator::DisplayState>& display_states,
    chromeos::DisplayPowerState state,
    std::vector<bool>* display_power) {
  int num_on_displays = 0;
  if (display_power)
    display_power->resize(display_states.size());

  for (size_t i = 0; i < display_states.size(); ++i) {
    bool internal =
        display_states[i].display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool on =
        state == chromeos::DISPLAY_POWER_ALL_ON ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON &&
         !internal) ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (display_power)
      (*display_power)[i] = on;
    if (on)
      num_on_displays++;
  }
  return num_on_displays;
}

}  // namespace


const int DisplayConfigurator::kSetDisplayPowerNoFlags = 0;
const int DisplayConfigurator::kSetDisplayPowerForceProbe = 1 << 0;
const int
DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay = 1 << 1;

DisplayConfigurator::DisplayState::DisplayState()
    : display(NULL),
      selected_mode(NULL),
      mirror_mode(NULL) {}

bool DisplayConfigurator::TestApi::TriggerConfigureTimeout() {
  if (configurator_->configure_timer_.IsRunning()) {
    configurator_->configure_timer_.user_task().Run();
    configurator_->configure_timer_.Stop();
    return true;
  } else {
    return false;
  }
}

// static
const DisplayMode* DisplayConfigurator::FindDisplayModeMatchingSize(
    const DisplaySnapshot& display,
    const gfx::Size& size) {
  const DisplayMode* best_mode = NULL;
  for (DisplayModeList::const_iterator it = display.modes().begin();
       it != display.modes().end();
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
      display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      requested_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      current_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      next_display_protection_client_id_(1) {}

DisplayConfigurator::~DisplayConfigurator() {
  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);
}

void DisplayConfigurator::SetDelegateForTesting(
    scoped_ptr<NativeDisplayDelegate> display_delegate) {
  DCHECK(!native_display_delegate_);

  native_display_delegate_ = display_delegate.Pass();
  configure_display_ = true;
}

void DisplayConfigurator::SetInitialDisplayPower(
    chromeos::DisplayPowerState power_state) {
  DCHECK_EQ(display_state_, MULTIPLE_DISPLAY_STATE_INVALID);
  requested_power_state_ = current_power_state_ = power_state;
}

void DisplayConfigurator::Init(bool is_panel_fitting_enabled) {
  is_panel_fitting_enabled_ = is_panel_fitting_enabled;
  if (!configure_display_)
    return;

  // If the delegate is already initialized don't update it (For example, tests
  // set their own delegates).
  if (!native_display_delegate_) {
    native_display_delegate_ = CreatePlatformNativeDisplayDelegate();
    native_display_delegate_->AddObserver(this);
  }
}

void DisplayConfigurator::ForceInitialConfigure(
    uint32_t background_color_argb) {
  if (!configure_display_)
    return;

  native_display_delegate_->GrabServer();
  native_display_delegate_->Initialize();

  UpdateCachedDisplays();
  if (cached_displays_.size() > 1 && background_color_argb)
    native_display_delegate_->SetBackgroundColor(background_color_argb);
  const MultipleDisplayState new_state = ChooseDisplayState(
      requested_power_state_);
  const bool success = EnterStateOrFallBackToSoftwareMirroring(
      new_state, requested_power_state_);

  // Force the DPMS on chrome startup as the driver doesn't always detect
  // that all displays are on when signing out.
  native_display_delegate_->ForceDPMSOn();
  native_display_delegate_->UngrabServer();
  NotifyObservers(success, new_state);
}

bool DisplayConfigurator::IsMirroring() const {
  return display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR ||
      (mirroring_controller_ &&
       mirroring_controller_->SoftwareMirroringEnabled());
}

bool DisplayConfigurator::ApplyProtections(const ContentProtections& requests) {
  for (DisplayStateList::const_iterator it = cached_displays_.begin();
       it != cached_displays_.end();
       ++it) {
    uint32_t all_desired = 0;

    // In mirror mode, protection request of all displays need to be fulfilled.
    // In non-mirror mode, only request of client's display needs to be
    // fulfilled.
    ContentProtections::const_iterator request_it;
    if (IsMirroring()) {
      for (request_it = requests.begin();
           request_it != requests.end();
           ++request_it)
        all_desired |= request_it->second;
    } else {
      request_it = requests.find(it->display->display_id());
      if (request_it != requests.end())
        all_desired = request_it->second;
    }

    switch (it->display->type()) {
      case DISPLAY_CONNECTION_TYPE_UNKNOWN:
        return false;
      // DisplayPort, DVI, and HDMI all support HDCP.
      case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      case DISPLAY_CONNECTION_TYPE_DVI:
      case DISPLAY_CONNECTION_TYPE_HDMI: {
        HDCPState current_state;
        // Need to poll the driver for updates since other applications may
        // have updated the state.
        if (!native_display_delegate_->GetHDCPState(*it->display,
                                                    &current_state))
          return false;
        bool current_desired = (current_state != HDCP_STATE_UNDESIRED);
        bool new_desired = (all_desired & CONTENT_PROTECTION_METHOD_HDCP);
        // Don't enable again if HDCP is already active. Some buggy drivers
        // may disable and enable if setting "desired" in active state.
        if (current_desired != new_desired) {
          HDCPState new_state =
              new_desired ? HDCP_STATE_DESIRED : HDCP_STATE_UNDESIRED;
          if (!native_display_delegate_->SetHDCPState(*it->display, new_state))
            return false;
        }
        break;
      }
      case DISPLAY_CONNECTION_TYPE_INTERNAL:
      case DISPLAY_CONNECTION_TYPE_VGA:
      case DISPLAY_CONNECTION_TYPE_NETWORK:
        // No protections for these types. Do nothing.
        break;
      case DISPLAY_CONNECTION_TYPE_NONE:
        NOTREACHED();
        break;
    }
  }

  return true;
}

DisplayConfigurator::ContentProtectionClientId
DisplayConfigurator::RegisterContentProtectionClient() {
  if (!configure_display_)
    return kInvalidClientId;

  return next_display_protection_client_id_++;
}

void DisplayConfigurator::UnregisterContentProtectionClient(
    ContentProtectionClientId client_id) {
  client_protection_requests_.erase(client_id);

  ContentProtections protections;
  for (ProtectionRequests::const_iterator it =
           client_protection_requests_.begin();
       it != client_protection_requests_.end();
       ++it) {
    for (ContentProtections::const_iterator it2 = it->second.begin();
         it2 != it->second.end();
         ++it2) {
      protections[it2->first] |= it2->second;
    }
  }

  ApplyProtections(protections);
}

bool DisplayConfigurator::QueryContentProtectionStatus(
    ContentProtectionClientId client_id,
    int64_t display_id,
    uint32_t* link_mask,
    uint32_t* protection_mask) {
  if (!configure_display_)
    return false;

  uint32_t enabled = 0;
  uint32_t unfulfilled = 0;
  *link_mask = 0;
  for (DisplayStateList::const_iterator it = cached_displays_.begin();
       it != cached_displays_.end();
       ++it) {
    // Query display if it is in mirror mode or client on the same display.
    if (!IsMirroring() && it->display->display_id() != display_id)
      continue;

    *link_mask |= it->display->type();
    switch (it->display->type()) {
      case DISPLAY_CONNECTION_TYPE_UNKNOWN:
        return false;
      // DisplayPort, DVI, and HDMI all support HDCP.
      case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      case DISPLAY_CONNECTION_TYPE_DVI:
      case DISPLAY_CONNECTION_TYPE_HDMI: {
        HDCPState state;
        if (!native_display_delegate_->GetHDCPState(*it->display, &state))
          return false;
        if (state == HDCP_STATE_ENABLED)
          enabled |= CONTENT_PROTECTION_METHOD_HDCP;
        else
          unfulfilled |= CONTENT_PROTECTION_METHOD_HDCP;
        break;
      }
      case DISPLAY_CONNECTION_TYPE_INTERNAL:
      case DISPLAY_CONNECTION_TYPE_VGA:
      case DISPLAY_CONNECTION_TYPE_NETWORK:
        // No protections for these types. Do nothing.
        break;
      case DISPLAY_CONNECTION_TYPE_NONE:
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

bool DisplayConfigurator::EnableContentProtection(
    ContentProtectionClientId client_id,
    int64_t display_id,
    uint32_t desired_method_mask) {
  if (!configure_display_)
    return false;

  ContentProtections protections;
  for (ProtectionRequests::const_iterator it =
           client_protection_requests_.begin();
       it != client_protection_requests_.end();
       ++it) {
    for (ContentProtections::const_iterator it2 = it->second.begin();
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

  if (desired_method_mask == CONTENT_PROTECTION_METHOD_NONE) {
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
    for (size_t i = 0; i < cached_displays_.size(); ++i) {
      if (cached_displays_[i].display &&
          cached_displays_[i].display->display_id() == display_id) {
        return native_display_delegate_->GetAvailableColorCalibrationProfiles(
            *cached_displays_[i].display);
      }
    }
  }

  return std::vector<ui::ColorCalibrationProfile>();
}

bool DisplayConfigurator::SetColorCalibrationProfile(
    int64_t display_id,
    ui::ColorCalibrationProfile new_profile) {
  for (size_t i = 0; i < cached_displays_.size(); ++i) {
    if (cached_displays_[i].display &&
        cached_displays_[i].display->display_id() == display_id) {
      return native_display_delegate_->SetColorCalibrationProfile(
          *cached_displays_[i].display, new_profile);
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
          << (configure_timer_.IsRunning() ? "Running" : "Stopped");
  if (power_state == current_power_state_ &&
      !(flags & kSetDisplayPowerForceProbe))
    return true;

  native_display_delegate_->GrabServer();
  UpdateCachedDisplays();

  const MultipleDisplayState new_state = ChooseDisplayState(power_state);
  bool attempted_change = false;
  bool success = false;

  bool only_if_single_internal_display =
      flags & kSetDisplayPowerOnlyIfSingleInternalDisplay;
  bool single_internal_display =
      cached_displays_.size() == 1 &&
      cached_displays_[0].display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
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
  return success;
}

bool DisplayConfigurator::SetDisplayMode(MultipleDisplayState new_state) {
  if (!configure_display_)
    return false;

  VLOG(1) << "SetDisplayMode: state=" << DisplayStateToString(new_state);
  if (display_state_ == new_state) {
    // Cancel software mirroring if the state is moving from
    // MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED to
    // MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED.
    if (mirroring_controller_ &&
        new_state == MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED)
      mirroring_controller_->SetSoftwareMirroring(false);
    NotifyObservers(true, new_state);
    return true;
  }

  native_display_delegate_->GrabServer();
  UpdateCachedDisplays();
  const bool success = EnterStateOrFallBackToSoftwareMirroring(
      new_state, requested_power_state_);
  native_display_delegate_->UngrabServer();

  NotifyObservers(success, new_state);
  return success;
}

void DisplayConfigurator::OnConfigurationChanged() {
  // Configure displays with |kConfigureDelayMs| delay,
  // so that time-consuming ConfigureDisplays() won't be called multiple times.
  if (configure_timer_.IsRunning()) {
    // Note: when the timer is running it is possible that a different task
    // (SetDisplayPower()) is scheduled. In these cases, prefer the already
    // scheduled task to ConfigureDisplays() since ConfigureDisplays() performs
    // only basic configuration while SetDisplayPower() will perform additional
    // operations.
    configure_timer_.Reset();
  } else {
    configure_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
        this,
        &DisplayConfigurator::ConfigureDisplays);
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
  if (requested_power_state_ == chromeos::DISPLAY_POWER_ALL_OFF) {
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
  configure_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kResumeDelayMs),
      base::Bind(base::IgnoreResult(&DisplayConfigurator::SetDisplayPower),
                 base::Unretained(this),
                 requested_power_state_,
                 kSetDisplayPowerForceProbe));
}

void DisplayConfigurator::UpdateCachedDisplays() {
  std::vector<DisplaySnapshot*> snapshots =
      native_display_delegate_->GetDisplays();

  cached_displays_.clear();
  for (size_t i = 0; i < snapshots.size(); ++i) {
    DisplayState display_state;
    display_state.display = snapshots[i];
    cached_displays_.push_back(display_state);
  }

  // Set |selected_mode| fields.
  for (size_t i = 0; i < cached_displays_.size(); ++i) {
    DisplayState* display_state = &cached_displays_[i];
    if (display_state->display->has_proper_display_id()) {
      gfx::Size size;
      if (state_controller_ &&
          state_controller_->GetResolutionForDisplayId(
              display_state->display->display_id(), &size)) {
        display_state->selected_mode =
            FindDisplayModeMatchingSize(*display_state->display, size);
      }
    }
    // Fall back to native mode.
    if (!display_state->selected_mode)
      display_state->selected_mode = display_state->display->native_mode();
  }

  // Set |mirror_mode| fields.
  if (cached_displays_.size() == 2) {
    bool one_is_internal =
        cached_displays_[0].display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool two_is_internal =
        cached_displays_[1].display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    int internal_displays =
        (one_is_internal ? 1 : 0) + (two_is_internal ? 1 : 0);
    DCHECK_LT(internal_displays, 2);
    LOG_IF(WARNING, internal_displays == 2)
        << "Two internal displays detected.";

    bool can_mirror = false;
    for (int attempt = 0; !can_mirror && attempt < 2; ++attempt) {
      // Try preserving external display's aspect ratio on the first attempt.
      // If that fails, fall back to the highest matching resolution.
      bool preserve_aspect = attempt == 0;

      if (internal_displays == 1) {
        if (one_is_internal) {
          can_mirror = FindMirrorMode(&cached_displays_[0],
                                      &cached_displays_[1],
                                      is_panel_fitting_enabled_,
                                      preserve_aspect);
        } else {
          DCHECK(two_is_internal);
          can_mirror = FindMirrorMode(&cached_displays_[1],
                                      &cached_displays_[0],
                                      is_panel_fitting_enabled_,
                                      preserve_aspect);
        }
      } else {  // if (internal_displays == 0)
        // No panel fitting for external displays, so fall back to exact match.
        can_mirror = FindMirrorMode(
            &cached_displays_[0], &cached_displays_[1], false, preserve_aspect);
        if (!can_mirror && preserve_aspect) {
          // FindMirrorMode() will try to preserve aspect ratio of what it
          // thinks is external display, so if it didn't succeed with one, maybe
          // it will succeed with the other.  This way we will have the correct
          // aspect ratio on at least one of them.
          can_mirror = FindMirrorMode(&cached_displays_[1],
                                      &cached_displays_[0],
                                      false,
                                      preserve_aspect);
        }
      }
    }
  }
}

bool DisplayConfigurator::FindMirrorMode(DisplayState* internal_display,
                                         DisplayState* external_display,
                                         bool try_panel_fitting,
                                         bool preserve_aspect) {
  const DisplayMode* internal_native_info =
      internal_display->display->native_mode();
  const DisplayMode* external_native_info =
      external_display->display->native_mode();
  if (!internal_native_info || !external_native_info)
    return false;

  // Check if some external display resolution can be mirrored on internal.
  // Prefer the modes in the order they're present in DisplaySnapshot, assuming
  // this is the order in which they look better on the monitor.
  for (DisplayModeList::const_iterator external_it =
           external_display->display->modes().begin();
       external_it != external_display->display->modes().end();
       ++external_it) {
    const DisplayMode& external_info = **external_it;
    bool is_native_aspect_ratio =
        external_native_info->size().width() * external_info.size().height() ==
        external_native_info->size().height() * external_info.size().width();
    if (preserve_aspect && !is_native_aspect_ratio)
      continue;  // Allow only aspect ratio preserving modes for mirroring.

    // Try finding an exact match.
    for (DisplayModeList::const_iterator internal_it =
             internal_display->display->modes().begin();
         internal_it != internal_display->display->modes().end();
         ++internal_it) {
      const DisplayMode& internal_info = **internal_it;
      if (internal_info.size().width() == external_info.size().width() &&
          internal_info.size().height() == external_info.size().height() &&
          internal_info.is_interlaced() == external_info.is_interlaced()) {
        internal_display->mirror_mode = *internal_it;
        external_display->mirror_mode = *external_it;
        return true;  // Mirror mode found.
      }
    }

    // Try to create a matching internal display mode by panel fitting.
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
        native_display_delegate_->AddMode(*internal_display->display,
                                          *external_it);
        internal_display->display->add_mode(*external_it);
        internal_display->mirror_mode = *external_it;
        external_display->mirror_mode = *external_it;
        return true;  // Mirror mode created.
      }
    }
  }

  return false;
}

void DisplayConfigurator::ConfigureDisplays() {
  if (!configure_display_)
    return;

  native_display_delegate_->GrabServer();
  UpdateCachedDisplays();
  const MultipleDisplayState new_state = ChooseDisplayState(
      requested_power_state_);
  const bool success = EnterStateOrFallBackToSoftwareMirroring(
      new_state, requested_power_state_);
  native_display_delegate_->UngrabServer();

  NotifyObservers(success, new_state);
}

void DisplayConfigurator::NotifyObservers(
    bool success,
    MultipleDisplayState attempted_state) {
  if (success) {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChanged(cached_displays_));
  } else {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChangeFailed(attempted_state));
  }
}

bool DisplayConfigurator::EnterStateOrFallBackToSoftwareMirroring(
    MultipleDisplayState display_state,
    chromeos::DisplayPowerState power_state) {
  bool success = EnterState(display_state, power_state);
  if (mirroring_controller_) {
    bool enable_software_mirroring = false;
    if (!success && display_state == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR) {
      if (display_state_ != MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED ||
          current_power_state_ != power_state)
        EnterState(MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED, power_state);
      enable_software_mirroring = success =
          display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
    }
    mirroring_controller_->SetSoftwareMirroring(enable_software_mirroring);
  }
  return success;
}

bool DisplayConfigurator::EnterState(MultipleDisplayState display_state,
                                     chromeos::DisplayPowerState power_state) {
  std::vector<bool> display_power;
  int num_on_displays =
      GetDisplayPower(cached_displays_, power_state, &display_power);
  VLOG(1) << "EnterState: display=" << DisplayStateToString(display_state)
          << " power=" << DisplayPowerStateToString(power_state);

  // Save the requested state so we'll try to use it next time even if we fail.
  requested_power_state_ = power_state;

  // Framebuffer dimensions.
  gfx::Size size;

  std::vector<gfx::Point> new_origins(cached_displays_.size(), gfx::Point());
  std::vector<const DisplayMode*> new_mode;
  for (size_t i = 0; i < cached_displays_.size(); ++i)
    new_mode.push_back(cached_displays_[i].display->current_mode());

  switch (display_state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      NOTREACHED() << "Ignoring request to enter invalid state with "
                   << cached_displays_.size() << " connected display(s)";
      return false;
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      if (cached_displays_.size() != 0) {
        LOG(WARNING) << "Ignoring request to enter headless mode with "
                     << cached_displays_.size() << " connected display(s)";
        return false;
      }
      break;
    case MULTIPLE_DISPLAY_STATE_SINGLE: {
      // If there are multiple displays connected, only one should be turned on.
      if (cached_displays_.size() != 1 && num_on_displays != 1) {
        LOG(WARNING) << "Ignoring request to enter single mode with "
                     << cached_displays_.size() << " connected displays and "
                     << num_on_displays << " turned on";
        return false;
      }

      for (size_t i = 0; i < cached_displays_.size(); ++i) {
        DisplayState* state = &cached_displays_[i];
        new_mode[i] = display_power[i] ? state->selected_mode : NULL;

        if (display_power[i] || cached_displays_.size() == 1) {
          const DisplayMode* mode_info = state->selected_mode;
          if (!mode_info) {
            LOG(WARNING) << "No selected mode when configuring display: "
                         << state->display->ToString();
            return false;
          }
          if (mode_info->size() == gfx::Size(1024, 768)) {
            VLOG(1) << "Potentially misdetecting display(1024x768):"
                    << " displays size=" << cached_displays_.size()
                    << ", num_on_displays=" << num_on_displays
                    << ", current size:" << size.width() << "x" << size.height()
                    << ", i=" << i << ", display=" << state->display->ToString()
                    << ", display_mode=" << mode_info->ToString();
          }
          size = mode_info->size();
        }
      }
      break;
    }
    case MULTIPLE_DISPLAY_STATE_DUAL_MIRROR: {
      if (cached_displays_.size() != 2 ||
          (num_on_displays != 0 && num_on_displays != 2)) {
        LOG(WARNING) << "Ignoring request to enter mirrored mode with "
                     << cached_displays_.size() << " connected display(s) and "
                     << num_on_displays << " turned on";
        return false;
      }

      const DisplayMode* mode_info = cached_displays_[0].mirror_mode;
      if (!mode_info) {
        LOG(WARNING) << "No mirror mode when configuring display: "
                     << cached_displays_[0].display->ToString();
        return false;
      }
      size = mode_info->size();

      for (size_t i = 0; i < cached_displays_.size(); ++i) {
        DisplayState* state = &cached_displays_[i];
        new_mode[i] = display_power[i] ? state->mirror_mode : NULL;
      }
      break;
    }
    case MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED: {
      if (cached_displays_.size() != 2 ||
          (num_on_displays != 0 && num_on_displays != 2)) {
        LOG(WARNING) << "Ignoring request to enter extended mode with "
                     << cached_displays_.size() << " connected display(s) and "
                     << num_on_displays << " turned on";
        return false;
      }

      for (size_t i = 0; i < cached_displays_.size(); ++i) {
        DisplayState* state = &cached_displays_[i];
        new_origins[i].set_y(size.height() ? size.height() + kVerticalGap : 0);
        new_mode[i] = display_power[i] ? state->selected_mode : NULL;

        // Retain the full screen size even if all displays are off so the
        // same desktop configuration can be restored when the displays are
        // turned back on.
        const DisplayMode* mode_info = cached_displays_[i].selected_mode;
        if (!mode_info) {
          LOG(WARNING) << "No selected mode when configuring display: "
                       << state->display->ToString();
          return false;
        }

        size.set_width(std::max<int>(size.width(), mode_info->size().width()));
        size.set_height(size.height() + (size.height() ? kVerticalGap : 0) +
                        mode_info->size().height());
      }
      break;
    }
  }

  // Finally, apply the desired changes.
  bool all_succeeded = true;
  if (!cached_displays_.empty()) {
    native_display_delegate_->CreateFrameBuffer(size);
    for (size_t i = 0; i < cached_displays_.size(); ++i) {
      const DisplayState& state = cached_displays_[i];
      bool configure_succeeded = false;

      while (true) {
        if (native_display_delegate_->Configure(
                *state.display, new_mode[i], new_origins[i])) {
          state.display->set_current_mode(new_mode[i]);
          state.display->set_origin(new_origins[i]);

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
                 state.display->modes().begin();
             it != state.display->modes().end();
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

      if (!configure_succeeded)
        all_succeeded = false;

      // If we are trying to set mirror mode and one of the modesets fails,
      // then the two monitors will be mis-matched.  In this case, return
      // false to let the observers be aware.
      if (display_state == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR &&
          display_power[i] &&
          state.display->current_mode() != state.mirror_mode)
        all_succeeded = false;
    }
  }

  if (all_succeeded) {
    display_state_ = display_state;
    current_power_state_ = power_state;
    framebuffer_size_ = size;
  }
  return all_succeeded;
}

MultipleDisplayState DisplayConfigurator::ChooseDisplayState(
    chromeos::DisplayPowerState power_state) const {
  int num_on_displays = GetDisplayPower(cached_displays_, power_state, NULL);
  switch (cached_displays_.size()) {
    case 0:
      return MULTIPLE_DISPLAY_STATE_HEADLESS;
    case 1:
      return MULTIPLE_DISPLAY_STATE_SINGLE;
    case 2: {
      if (num_on_displays == 1) {
        // If only one display is currently turned on, return the "single"
        // state so that its native mode will be used.
        return MULTIPLE_DISPLAY_STATE_SINGLE;
      } else {
        if (!state_controller_)
          return MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
        // With either both displays on or both displays off, use one of the
        // dual modes.
        std::vector<int64_t> display_ids;
        for (size_t i = 0; i < cached_displays_.size(); ++i) {
          // If display id isn't available, switch to extended mode.
          if (!cached_displays_[i].display->has_proper_display_id())
            return MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
          display_ids.push_back(cached_displays_[i].display->display_id());
        }
        return state_controller_->GetStateForDisplayIds(display_ids);
      }
    }
    default:
      NOTREACHED();
  }
  return MULTIPLE_DISPLAY_STATE_INVALID;
}

}  // namespace ui
