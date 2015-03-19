// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/display_configurator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "ui/display/chromeos/display_util.h"
#include "ui/display/chromeos/update_display_configuration_task.h"
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

struct DisplayState {
  DisplaySnapshot* display = nullptr;  // Not owned.

  // User-selected mode for the display.
  const DisplayMode* selected_mode = nullptr;

  // Mode used when displaying the same desktop on multiple displays.
  const DisplayMode* mirror_mode = nullptr;
};

void DoNothing(bool status) {
}

}  // namespace

const int DisplayConfigurator::kSetDisplayPowerNoFlags = 0;
const int DisplayConfigurator::kSetDisplayPowerForceProbe = 1 << 0;
const int
DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay = 1 << 1;

bool DisplayConfigurator::TestApi::TriggerConfigureTimeout() {
  if (configurator_->configure_timer_.IsRunning()) {
    configurator_->configure_timer_.user_task().Run();
    configurator_->configure_timer_.Stop();
    return true;
  } else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// DisplayConfigurator::DisplayLayoutManagerImpl implementation

class DisplayConfigurator::DisplayLayoutManagerImpl
    : public DisplayLayoutManager {
 public:
  DisplayLayoutManagerImpl(DisplayConfigurator* configurator);
  ~DisplayLayoutManagerImpl() override;

  // DisplayConfigurator::DisplayLayoutManager:
  SoftwareMirroringController* GetSoftwareMirroringController() const override;
  StateController* GetStateController() const override;
  MultipleDisplayState GetDisplayState() const override;
  chromeos::DisplayPowerState GetPowerState() const override;
  bool GetDisplayLayout(const std::vector<DisplaySnapshot*>& displays,
                        MultipleDisplayState new_display_state,
                        chromeos::DisplayPowerState new_power_state,
                        std::vector<DisplayConfigureRequest>* requests,
                        gfx::Size* framebuffer_size) const override;

 private:
  // Parses the |displays| into a list of DisplayStates. This effectively adds
  // |mirror_mode| and |selected_mode| to the returned results.
  // TODO(dnicoara): Break this into GetSelectedMode() and GetMirrorMode() and
  // remove DisplayState.
  std::vector<DisplayState> ParseDisplays(
      const std::vector<DisplaySnapshot*>& displays) const;

  // Helper method for ParseDisplays() that initializes the passed-in
  // displays' |mirror_mode| fields by looking for a mode in |internal_display|
  // and |external_display| having the same resolution. Returns false if a
  // shared mode wasn't found or created.
  //
  // |try_panel_fitting| allows creating a panel-fitting mode for
  // |internal_display| instead of only searching for a matching mode (note that
  // it may lead to a crash if |internal_display| is not capable of panel
  // fitting).
  //
  // |preserve_aspect| limits the search/creation only to the modes having the
  // native aspect ratio of |external_display|.
  bool FindMirrorMode(DisplayState* internal_display,
                      DisplayState* external_display,
                      bool try_panel_fitting,
                      bool preserve_aspect) const;

  DisplayConfigurator* configurator_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DisplayLayoutManagerImpl);
};

DisplayConfigurator::DisplayLayoutManagerImpl::DisplayLayoutManagerImpl(
    DisplayConfigurator* configurator)
    : configurator_(configurator) {
}

DisplayConfigurator::DisplayLayoutManagerImpl::~DisplayLayoutManagerImpl() {
}

DisplayConfigurator::SoftwareMirroringController*
DisplayConfigurator::DisplayLayoutManagerImpl::GetSoftwareMirroringController()
    const {
  return configurator_->mirroring_controller_;
}

DisplayConfigurator::StateController*
DisplayConfigurator::DisplayLayoutManagerImpl::GetStateController() const {
  return configurator_->state_controller_;
}

MultipleDisplayState
DisplayConfigurator::DisplayLayoutManagerImpl::GetDisplayState() const {
  return configurator_->current_display_state_;
}

chromeos::DisplayPowerState
DisplayConfigurator::DisplayLayoutManagerImpl::GetPowerState() const {
  return configurator_->current_power_state_;
}

std::vector<DisplayState>
DisplayConfigurator::DisplayLayoutManagerImpl::ParseDisplays(
    const std::vector<DisplaySnapshot*>& snapshots) const {
  std::vector<DisplayState> cached_displays;
  for (auto snapshot : snapshots) {
    DisplayState display_state;
    display_state.display = snapshot;
    cached_displays.push_back(display_state);
  }

  // Set |selected_mode| fields.
  for (size_t i = 0; i < cached_displays.size(); ++i) {
    DisplayState* display_state = &cached_displays[i];
    gfx::Size size;
    if (GetStateController() &&
        GetStateController()->GetResolutionForDisplayId(
            display_state->display->display_id(), &size)) {
      display_state->selected_mode =
          FindDisplayModeMatchingSize(*display_state->display, size);
    }

    // Fall back to native mode.
    if (!display_state->selected_mode)
      display_state->selected_mode = display_state->display->native_mode();
  }

  // Set |mirror_mode| fields.
  if (cached_displays.size() == 2) {
    bool one_is_internal =
        cached_displays[0].display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool two_is_internal =
        cached_displays[1].display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    int internal_displays =
        (one_is_internal ? 1 : 0) + (two_is_internal ? 1 : 0);
    DCHECK_LT(internal_displays, 2);
    LOG_IF(WARNING, internal_displays >= 2)
        << "At least two internal displays detected.";

    bool can_mirror = false;
    for (int attempt = 0; !can_mirror && attempt < 2; ++attempt) {
      // Try preserving external display's aspect ratio on the first attempt.
      // If that fails, fall back to the highest matching resolution.
      bool preserve_aspect = attempt == 0;

      if (internal_displays == 1) {
        can_mirror = FindMirrorMode(&cached_displays[one_is_internal ? 0 : 1],
                                    &cached_displays[one_is_internal ? 1 : 0],
                                    configurator_->is_panel_fitting_enabled_,
                                    preserve_aspect);
      } else {  // if (internal_displays == 0)
        // No panel fitting for external displays, so fall back to exact match.
        can_mirror = FindMirrorMode(&cached_displays[0], &cached_displays[1],
                                    false, preserve_aspect);
        if (!can_mirror && preserve_aspect) {
          // FindMirrorMode() will try to preserve aspect ratio of what it
          // thinks is external display, so if it didn't succeed with one, maybe
          // it will succeed with the other.  This way we will have the correct
          // aspect ratio on at least one of them.
          can_mirror = FindMirrorMode(&cached_displays[1], &cached_displays[0],
                                      false, preserve_aspect);
        }
      }
    }
  }

  return cached_displays;
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::GetDisplayLayout(
    const std::vector<DisplaySnapshot*>& displays,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    std::vector<DisplayConfigureRequest>* requests,
    gfx::Size* framebuffer_size) const {
  std::vector<DisplayState> states = ParseDisplays(displays);
  std::vector<bool> display_power;
  int num_on_displays =
      GetDisplayPower(displays, new_power_state, &display_power);
  VLOG(1) << "EnterState: display="
          << MultipleDisplayStateToString(new_display_state)
          << " power=" << DisplayPowerStateToString(new_power_state);

  // Framebuffer dimensions.
  gfx::Size size;

  for (size_t i = 0; i < displays.size(); ++i) {
    requests->push_back(DisplayConfigureRequest(
        displays[i], displays[i]->current_mode(), gfx::Point()));
  }

  switch (new_display_state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      NOTREACHED() << "Ignoring request to enter invalid state with "
                   << displays.size() << " connected display(s)";
      return false;
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      if (displays.size() != 0) {
        LOG(WARNING) << "Ignoring request to enter headless mode with "
                     << displays.size() << " connected display(s)";
        return false;
      }
      break;
    case MULTIPLE_DISPLAY_STATE_SINGLE: {
      // If there are multiple displays connected, only one should be turned on.
      if (displays.size() != 1 && num_on_displays != 1) {
        LOG(WARNING) << "Ignoring request to enter single mode with "
                     << displays.size() << " connected displays and "
                     << num_on_displays << " turned on";
        return false;
      }

      for (size_t i = 0; i < states.size(); ++i) {
        const DisplayState* state = &states[i];
        (*requests)[i].mode = display_power[i] ? state->selected_mode : NULL;

        if (display_power[i] || states.size() == 1) {
          const DisplayMode* mode_info = state->selected_mode;
          if (!mode_info) {
            LOG(WARNING) << "No selected mode when configuring display: "
                         << state->display->ToString();
            return false;
          }
          if (mode_info->size() == gfx::Size(1024, 768)) {
            VLOG(1) << "Potentially misdetecting display(1024x768):"
                    << " displays size=" << states.size()
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
      if (states.size() != 2 ||
          (num_on_displays != 0 && num_on_displays != 2)) {
        LOG(WARNING) << "Ignoring request to enter mirrored mode with "
                     << states.size() << " connected display(s) and "
                     << num_on_displays << " turned on";
        return false;
      }

      const DisplayMode* mode_info = states[0].mirror_mode;
      if (!mode_info) {
        LOG(WARNING) << "No mirror mode when configuring display: "
                     << states[0].display->ToString();
        return false;
      }
      size = mode_info->size();

      for (size_t i = 0; i < states.size(); ++i) {
        const DisplayState* state = &states[i];
        (*requests)[i].mode = display_power[i] ? state->mirror_mode : NULL;
      }
      break;
    }
    case MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED:
    case MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED: {
      if ((new_display_state == MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED &&
           states.size() != 2) ||
          (new_display_state == MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED &&
           states.size() <= 2) ||
          (num_on_displays != 0 &&
           num_on_displays != static_cast<int>(displays.size()))) {
        LOG(WARNING) << "Ignoring request to enter extended mode with "
                     << states.size() << " connected display(s) and "
                     << num_on_displays << " turned on";
        return false;
      }

      for (size_t i = 0; i < states.size(); ++i) {
        const DisplayState* state = &states[i];
        (*requests)[i].origin.set_y(size.height() ? size.height() + kVerticalGap
                                                  : 0);
        (*requests)[i].mode = display_power[i] ? state->selected_mode : NULL;

        // Retain the full screen size even if all displays are off so the
        // same desktop configuration can be restored when the displays are
        // turned back on.
        const DisplayMode* mode_info = states[i].selected_mode;
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

  *framebuffer_size = size;
  return true;
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::FindMirrorMode(
    DisplayState* internal_display,
    DisplayState* external_display,
    bool try_panel_fitting,
    bool preserve_aspect) const {
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
       external_it != external_display->display->modes().end(); ++external_it) {
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
        configurator_->native_display_delegate_->AddMode(
            *internal_display->display, *external_it);
        internal_display->display->add_mode(*external_it);
        internal_display->mirror_mode = *external_it;
        external_display->mirror_mode = *external_it;
        return true;  // Mirror mode created.
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayConfigurator implementation

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

    if (mode == display.native_mode()) {
      best_mode = mode;
      break;
    }

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
      current_display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      current_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      requested_display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      requested_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      requested_power_state_change_(false),
      requested_power_flags_(kSetDisplayPowerNoFlags),
      force_configure_(false),
      next_display_protection_client_id_(1),
      display_externally_controlled_(false),
      displays_suspended_(false),
      layout_manager_(new DisplayLayoutManagerImpl(this)),
      weak_ptr_factory_(this) {
}

DisplayConfigurator::~DisplayConfigurator() {
  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);

  CallAndClearInProgressCallbacks(false);
  CallAndClearQueuedCallbacks(false);
}

void DisplayConfigurator::SetDelegateForTesting(
    scoped_ptr<NativeDisplayDelegate> display_delegate) {
  DCHECK(!native_display_delegate_);

  native_display_delegate_ = display_delegate.Pass();
  configure_display_ = true;
}

void DisplayConfigurator::SetInitialDisplayPower(
    chromeos::DisplayPowerState power_state) {
  DCHECK_EQ(current_display_state_, MULTIPLE_DISPLAY_STATE_INVALID);
  requested_power_state_ = current_power_state_ = power_state;
}

void DisplayConfigurator::Init(bool is_panel_fitting_enabled) {
  is_panel_fitting_enabled_ = is_panel_fitting_enabled;
  if (!configure_display_ || display_externally_controlled_)
    return;

  // If the delegate is already initialized don't update it (For example, tests
  // set their own delegates).
  if (!native_display_delegate_) {
    native_display_delegate_ = CreatePlatformNativeDisplayDelegate();
    native_display_delegate_->AddObserver(this);
  }
}

void DisplayConfigurator::TakeControl() {
  if (cached_displays_.empty())
    return;

  if (!display_externally_controlled_)
    return;

  if (!native_display_delegate_->TakeDisplayControl())
    return;

  display_externally_controlled_ = false;
  force_configure_ = true;
  RunPendingConfiguration();
}

void DisplayConfigurator::RelinquishControl() {
  if (display_externally_controlled_)
    return;

  display_externally_controlled_ = true;
  native_display_delegate_->RelinquishDisplayControl();
}

void DisplayConfigurator::ForceInitialConfigure(
    uint32_t background_color_argb) {
  if (!configure_display_ || display_externally_controlled_)
    return;

  native_display_delegate_->Initialize();

  // ForceInitialConfigure should be the first configuration so there shouldn't
  // be anything scheduled.
  DCHECK(!configuration_task_);

  configuration_task_.reset(new UpdateDisplayConfigurationTask(
      native_display_delegate_.get(), layout_manager_.get(),
      requested_display_state_, requested_power_state_,
      kSetDisplayPowerForceProbe, background_color_argb, true,
      base::Bind(&DisplayConfigurator::OnConfigured,
                 weak_ptr_factory_.GetWeakPtr())));
  configuration_task_->Run();
}

bool DisplayConfigurator::IsMirroring() const {
  return current_display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR ||
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
      request_it = requests.find((*it)->display_id());
      if (request_it != requests.end())
        all_desired = request_it->second;
    }

    switch ((*it)->type()) {
      case DISPLAY_CONNECTION_TYPE_UNKNOWN:
        return false;
      // DisplayPort, DVI, and HDMI all support HDCP.
      case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      case DISPLAY_CONNECTION_TYPE_DVI:
      case DISPLAY_CONNECTION_TYPE_HDMI: {
        HDCPState current_state;
        // Need to poll the driver for updates since other applications may
        // have updated the state.
        if (!native_display_delegate_->GetHDCPState(**it, &current_state))
          return false;
        bool current_desired = (current_state != HDCP_STATE_UNDESIRED);
        bool new_desired = (all_desired & CONTENT_PROTECTION_METHOD_HDCP);
        // Don't enable again if HDCP is already active. Some buggy drivers
        // may disable and enable if setting "desired" in active state.
        if (current_desired != new_desired) {
          HDCPState new_state =
              new_desired ? HDCP_STATE_DESIRED : HDCP_STATE_UNDESIRED;
          if (!native_display_delegate_->SetHDCPState(**it, new_state))
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
  if (!configure_display_ || display_externally_controlled_)
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
  if (!configure_display_ || display_externally_controlled_)
    return false;

  uint32_t enabled = 0;
  uint32_t unfulfilled = 0;
  *link_mask = 0;
  for (DisplayStateList::const_iterator it = cached_displays_.begin();
       it != cached_displays_.end();
       ++it) {
    // Query display if it is in mirror mode or client on the same display.
    if (!IsMirroring() && (*it)->display_id() != display_id)
      continue;

    *link_mask |= (*it)->type();
    switch ((*it)->type()) {
      case DISPLAY_CONNECTION_TYPE_UNKNOWN:
        return false;
      // DisplayPort, DVI, and HDMI all support HDCP.
      case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      case DISPLAY_CONNECTION_TYPE_DVI:
      case DISPLAY_CONNECTION_TYPE_HDMI: {
        HDCPState state;
        if (!native_display_delegate_->GetHDCPState(**it, &state))
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
  if (!configure_display_ || display_externally_controlled_)
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
      if (cached_displays_[i]->display_id() == display_id) {
        return native_display_delegate_->GetAvailableColorCalibrationProfiles(
            *cached_displays_[i]);
      }
    }
  }

  return std::vector<ui::ColorCalibrationProfile>();
}

bool DisplayConfigurator::SetColorCalibrationProfile(
    int64_t display_id,
    ui::ColorCalibrationProfile new_profile) {
  for (size_t i = 0; i < cached_displays_.size(); ++i) {
    if (cached_displays_[i]->display_id() == display_id) {
      return native_display_delegate_->SetColorCalibrationProfile(
          *cached_displays_[i], new_profile);
    }
  }

  return false;
}

void DisplayConfigurator::PrepareForExit() {
  configure_display_ = false;
}

void DisplayConfigurator::SetDisplayPower(
    chromeos::DisplayPowerState power_state,
    int flags,
    const ConfigurationCallback& callback) {
  if (!configure_display_ || display_externally_controlled_) {
    callback.Run(false);
    return;
  }

  VLOG(1) << "SetDisplayPower: power_state="
          << DisplayPowerStateToString(power_state) << " flags=" << flags
          << ", configure timer="
          << (configure_timer_.IsRunning() ? "Running" : "Stopped");
  if (power_state == requested_power_state_ &&
      !(flags & kSetDisplayPowerForceProbe)) {
    callback.Run(true);
    return;
  }

  requested_power_state_ = power_state;
  requested_power_state_change_ = true;
  requested_power_flags_ = flags;
  queued_configuration_callbacks_.push_back(callback);

  RunPendingConfiguration();
}

void DisplayConfigurator::SetDisplayMode(MultipleDisplayState new_state) {
  if (!configure_display_ || display_externally_controlled_)
    return;

  VLOG(1) << "SetDisplayMode: state="
          << MultipleDisplayStateToString(new_state);
  if (current_display_state_ == new_state) {
    // Cancel software mirroring if the state is moving from
    // MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED to
    // MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED.
    if (mirroring_controller_ &&
        new_state == MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED)
      mirroring_controller_->SetSoftwareMirroring(false);
    NotifyObservers(true, new_state);
    return;
  }

  requested_display_state_ = new_state;

  RunPendingConfiguration();
}

void DisplayConfigurator::OnConfigurationChanged() {
  // Don't do anything if the displays are currently suspended.  Instead we will
  // probe and reconfigure the displays if necessary in ResumeDisplays().
  if (displays_suspended_) {
    VLOG(1) << "Displays are currently suspended.  Not attempting to "
            << "reconfigure them.";
    return;
  }

  // Configure displays with |kConfigureDelayMs| delay,
  // so that time-consuming ConfigureDisplays() won't be called multiple times.
  if (configure_timer_.IsRunning()) {
    // Note: when the timer is running it is possible that a different task
    // (RestoreRequestedPowerStateAfterResume()) is scheduled. In these cases,
    // prefer the already scheduled task to ConfigureDisplays() since
    // ConfigureDisplays() performs only basic configuration while
    // RestoreRequestedPowerStateAfterResume() will perform additional
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
                    kSetDisplayPowerOnlyIfSingleInternalDisplay,
                    base::Bind(&DoNothing));

    // We need to make sure that the monitor configuration we just did actually
    // completes before we return, because otherwise the X message could be
    // racing with the HandleSuspendReadiness message.
    native_display_delegate_->SyncWithServer();
  }

  displays_suspended_ = true;

  // Stop |configure_timer_| because we will force probe and configure all the
  // displays at resume time anyway.
  configure_timer_.Stop();
}

void DisplayConfigurator::ResumeDisplays() {
  displays_suspended_ = false;

  configure_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kResumeDelayMs),
      base::Bind(&DisplayConfigurator::RestoreRequestedPowerStateAfterResume,
                 base::Unretained(this)));
}

void DisplayConfigurator::ConfigureDisplays() {
  if (!configure_display_ || display_externally_controlled_)
    return;

  force_configure_ = true;
  RunPendingConfiguration();
}

void DisplayConfigurator::RunPendingConfiguration() {
  // Configuration task is currently running. Do not start a second
  // configuration.
  if (configuration_task_)
    return;

  if (!ShouldRunConfigurationTask()) {
    LOG(ERROR) << "Called RunPendingConfiguration without any changes"
                  " requested";
    CallAndClearQueuedCallbacks(true);
    return;
  }

  configuration_task_.reset(new UpdateDisplayConfigurationTask(
      native_display_delegate_.get(), layout_manager_.get(),
      requested_display_state_, requested_power_state_, requested_power_flags_,
      0, force_configure_, base::Bind(&DisplayConfigurator::OnConfigured,
                                      weak_ptr_factory_.GetWeakPtr())));

  // Reset the flags before running the task; otherwise it may end up scheduling
  // another configuration.
  force_configure_ = false;
  requested_power_flags_ = kSetDisplayPowerNoFlags;
  requested_power_state_change_ = false;
  requested_display_state_ = MULTIPLE_DISPLAY_STATE_INVALID;

  DCHECK(in_progress_configuration_callbacks_.empty());
  in_progress_configuration_callbacks_.swap(queued_configuration_callbacks_);

  configuration_task_->Run();
}

void DisplayConfigurator::OnConfigured(
    bool success,
    const std::vector<DisplaySnapshot*>& displays,
    const gfx::Size& framebuffer_size,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state) {
  VLOG(1) << "OnConfigured: success=" << success << " new_display_state="
          << MultipleDisplayStateToString(new_display_state)
          << " new_power_state=" << DisplayPowerStateToString(new_power_state);

  cached_displays_ = displays;
  if (success) {
    current_display_state_ = new_display_state;
    current_power_state_ = new_power_state;
    framebuffer_size_ = framebuffer_size;
    // If the requested power state hasn't changed then make sure that value
    // gets updated as well since the last requested value may have been
    // dependent on certain conditions (ie: if only the internal monitor was
    // present).
    if (!requested_power_state_change_)
      requested_power_state_ = new_power_state;
  }

  configuration_task_.reset();
  NotifyObservers(success, new_display_state);
  CallAndClearInProgressCallbacks(success);

  if (success && !configure_timer_.IsRunning() &&
      ShouldRunConfigurationTask()) {
    configure_timer_.Start(FROM_HERE,
                           base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
                           this, &DisplayConfigurator::RunPendingConfiguration);
  } else {
    // If a new configuration task isn't scheduled respond to all queued
    // callbacks (for example if requested state is current state).
    if (!configure_timer_.IsRunning())
      CallAndClearQueuedCallbacks(success);
  }
}

bool DisplayConfigurator::ShouldRunConfigurationTask() const {
  if (force_configure_)
    return true;

  // Schedule if there is a request to change the display state.
  if (requested_display_state_ != current_display_state_ &&
      requested_display_state_ != MULTIPLE_DISPLAY_STATE_INVALID)
    return true;

  // Schedule if there is a request to change the power state.
  if (requested_power_state_change_)
    return true;

  return false;
}

void DisplayConfigurator::CallAndClearInProgressCallbacks(bool success) {
  for (const auto& callback : in_progress_configuration_callbacks_)
    callback.Run(success);

  in_progress_configuration_callbacks_.clear();
}

void DisplayConfigurator::CallAndClearQueuedCallbacks(bool success) {
  for (const auto& callback : queued_configuration_callbacks_)
    callback.Run(success);

  queued_configuration_callbacks_.clear();
}

void DisplayConfigurator::RestoreRequestedPowerStateAfterResume() {
  // Force probing to ensure that we pick up any changes that were made while
  // the system was suspended.
  SetDisplayPower(requested_power_state_, kSetDisplayPowerForceProbe,
                  base::Bind(&DoNothing));
}

void DisplayConfigurator::NotifyObservers(
    bool success,
    MultipleDisplayState attempted_state) {
  if (success) {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChanged(cached_displays_));
  } else {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChangeFailed(cached_displays_,
                                                        attempted_state));
  }
}

}  // namespace ui
