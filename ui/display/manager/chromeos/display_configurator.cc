// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/display_configurator.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chromeos/system/devicemode.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/chromeos/apply_content_protection_task.h"
#include "ui/display/manager/chromeos/display_layout_manager.h"
#include "ui/display/manager/chromeos/display_snapshot_virtual.h"
#include "ui/display/manager/chromeos/display_util.h"
#include "ui/display/manager/chromeos/update_display_configuration_task.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/util/display_util.h"

namespace display {

namespace {

typedef std::vector<const DisplayMode*> DisplayModeList;

// The EDID specification marks the top bit of the manufacturer id as reserved.
const int16_t kReservedManufacturerID = static_cast<int16_t>(1 << 15);

struct DisplayState {
  DisplaySnapshot* display = nullptr;  // Not owned.

  // User-selected mode for the display.
  const DisplayMode* selected_mode = nullptr;

  // Mode used when displaying the same desktop on multiple displays.
  const DisplayMode* mirror_mode = nullptr;
};

void DoNothing(bool status) {}

}  // namespace

const int DisplayConfigurator::kSetDisplayPowerNoFlags = 0;
const int DisplayConfigurator::kSetDisplayPowerForceProbe = 1 << 0;
const int DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay =
    1 << 1;

bool DisplayConfigurator::TestApi::TriggerConfigureTimeout() {
  if (configurator_->configure_timer_.IsRunning()) {
    configurator_->configure_timer_.user_task().Run();
    configurator_->configure_timer_.Stop();
    return true;
  } else {
    return false;
  }
}

base::TimeDelta DisplayConfigurator::TestApi::GetConfigureDelay() const {
  return configurator_->configure_timer_.IsRunning()
             ? configurator_->configure_timer_.GetCurrentDelay()
             : base::TimeDelta();
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
  DisplayStateList GetDisplayStates() const override;
  bool IsMirroring() const override;

 private:
  // Parses the |displays| into a list of DisplayStates. This effectively adds
  // |mirror_mode| and |selected_mode| to the returned results.
  // TODO(dnicoara): Break this into GetSelectedMode() and GetMirrorMode() and
  // remove DisplayState.
  std::vector<DisplayState> ParseDisplays(
      const std::vector<DisplaySnapshot*>& displays) const;

  const DisplayMode* GetUserSelectedMode(const DisplaySnapshot& display) const;

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
    : configurator_(configurator) {}

DisplayConfigurator::DisplayLayoutManagerImpl::~DisplayLayoutManagerImpl() {}

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
  for (auto* snapshot : snapshots) {
    DisplayState display_state;
    display_state.display = snapshot;
    display_state.selected_mode = GetUserSelectedMode(*snapshot);
    cached_displays.push_back(display_state);
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
      // In docked mode (with internal display + 2 external displays) the state
      // will be DUAL_EXTENDED with internal display turned off and the 2
      // external displays turned on.
      if (new_display_state == MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED &&
          states.size() != 2 && num_on_displays != 2) {
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
  DCHECK(new_display_state == MULTIPLE_DISPLAY_STATE_HEADLESS ||
         !size.IsEmpty());
  *framebuffer_size = size;
  return true;
}

DisplayConfigurator::DisplayStateList
DisplayConfigurator::DisplayLayoutManagerImpl::GetDisplayStates() const {
  return configurator_->cached_displays();
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::IsMirroring() const {
  if (GetDisplayState() == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR)
    return true;

  return GetSoftwareMirroringController() &&
         GetSoftwareMirroringController()->SoftwareMirroringEnabled();
}

const DisplayMode*
DisplayConfigurator::DisplayLayoutManagerImpl::GetUserSelectedMode(
    const DisplaySnapshot& display) const {
  gfx::Size size;
  const DisplayMode* selected_mode = nullptr;
  if (GetStateController() &&
      GetStateController()->GetResolutionForDisplayId(display.display_id(),
                                                      &size)) {
    selected_mode = FindDisplayModeMatchingSize(display, size);
  }

  // Fall back to native mode.
  return selected_mode ? selected_mode : display.native_mode();
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
  for (const auto& external_mode : external_display->display->modes()) {
    bool is_native_aspect_ratio =
        external_native_info->size().width() * external_mode->size().height() ==
        external_native_info->size().height() * external_mode->size().width();
    if (preserve_aspect && !is_native_aspect_ratio)
      continue;  // Allow only aspect ratio preserving modes for mirroring.

    // Try finding an exact match.
    for (const auto& internal_mode : internal_display->display->modes()) {
      if (internal_mode->size() == external_mode->size() &&
          internal_mode->is_interlaced() == external_mode->is_interlaced()) {
        internal_display->mirror_mode = internal_mode.get();
        external_display->mirror_mode = external_mode.get();
        return true;  // Mirror mode found.
      }
    }

    // Try to create a matching internal display mode by panel fitting.
    if (try_panel_fitting) {
      // We can downscale by 1.125, and upscale indefinitely. Downscaling looks
      // ugly, so, can fit == can upscale. Also, internal panels don't support
      // fitting interlaced modes.
      bool can_fit = internal_native_info->size().width() >=
                         external_mode->size().width() &&
                     internal_native_info->size().height() >=
                         external_mode->size().height() &&
                     !external_mode->is_interlaced();
      if (can_fit) {
        configurator_->native_display_delegate_->AddMode(
            *internal_display->display, external_mode.get());
        internal_display->display->add_mode(external_mode.get());
        internal_display->mirror_mode =
            internal_display->display->modes().back().get();
        external_display->mirror_mode = external_mode.get();
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
  for (const std::unique_ptr<const DisplayMode>& mode : display.modes()) {
    if (mode->size() != size)
      continue;

    if (mode.get() == display.native_mode()) {
      best_mode = mode.get();
      break;
    }

    if (!best_mode) {
      best_mode = mode.get();
      continue;
    }

    if (mode->is_interlaced()) {
      if (!best_mode->is_interlaced())
        continue;
    } else {
      // Reset the best rate if the non interlaced is
      // found the first time.
      if (best_mode->is_interlaced()) {
        best_mode = mode.get();
        continue;
      }
    }
    if (mode->refresh_rate() < best_mode->refresh_rate())
      continue;

    best_mode = mode.get();
  }

  return best_mode;
}

DisplayConfigurator::DisplayConfigurator()
    : state_controller_(NULL),
      mirroring_controller_(NULL),
      is_panel_fitting_enabled_(false),
      configure_display_(chromeos::IsRunningAsSystemCompositor()),
      current_display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      current_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      requested_display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      requested_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      pending_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      has_pending_power_state_(false),
      pending_power_flags_(kSetDisplayPowerNoFlags),
      force_configure_(false),
      next_display_protection_client_id_(1),
      display_externally_controlled_(false),
      display_control_changing_(false),
      displays_suspended_(false),
      layout_manager_(new DisplayLayoutManagerImpl(this)),
      weak_ptr_factory_(this) {}

DisplayConfigurator::~DisplayConfigurator() {
  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);

  CallAndClearInProgressCallbacks(false);
  CallAndClearQueuedCallbacks(false);

  while (!query_protection_callbacks_.empty()) {
    query_protection_callbacks_.front().Run(false, 0, 0);
    query_protection_callbacks_.pop();
  }

  while (!set_protection_callbacks_.empty()) {
    set_protection_callbacks_.front().Run(false);
    set_protection_callbacks_.pop();
  }
}

void DisplayConfigurator::SetDelegateForTesting(
    std::unique_ptr<NativeDisplayDelegate> display_delegate) {
  DCHECK(!native_display_delegate_);

  native_display_delegate_ = std::move(display_delegate);
  configure_display_ = true;
}

void DisplayConfigurator::SetInitialDisplayPower(
    chromeos::DisplayPowerState power_state) {
  DCHECK_EQ(current_display_state_, MULTIPLE_DISPLAY_STATE_INVALID);
  requested_power_state_ = current_power_state_ = power_state;
  NotifyPowerStateObservers();
}

void DisplayConfigurator::Init(
    std::unique_ptr<NativeDisplayDelegate> display_delegate,
    bool is_panel_fitting_enabled) {
  is_panel_fitting_enabled_ = is_panel_fitting_enabled;
  if (!configure_display_ || display_externally_controlled_)
    return;

  // If the delegate is already initialized don't update it (For example, tests
  // set their own delegates).
  if (!native_display_delegate_)
    native_display_delegate_ = std::move(display_delegate);

  native_display_delegate_->AddObserver(this);
}

void DisplayConfigurator::TakeControl(const DisplayControlCallback& callback) {
  if (display_control_changing_) {
    callback.Run(false);
    return;
  }

  if (!display_externally_controlled_) {
    callback.Run(true);
    return;
  }

  display_control_changing_ = true;
  native_display_delegate_->TakeDisplayControl(
      base::Bind(&DisplayConfigurator::OnDisplayControlTaken,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void DisplayConfigurator::OnDisplayControlTaken(
    const DisplayControlCallback& callback,
    bool success) {
  display_control_changing_ = false;
  display_externally_controlled_ = !success;
  if (success) {
    // Force a configuration since the display configuration may have changed.
    force_configure_ = true;
    // Restore the last power state used before releasing control.
    SetDisplayPower(requested_power_state_, kSetDisplayPowerNoFlags,
                    base::Bind(&DoNothing));
  }

  callback.Run(success);
}

void DisplayConfigurator::RelinquishControl(
    const DisplayControlCallback& callback) {
  if (display_control_changing_) {
    callback.Run(false);
    return;
  }

  if (display_externally_controlled_) {
    callback.Run(true);
    return;
  }

  // For simplicity, just fail if in the middle of a display configuration.
  if (configuration_task_) {
    callback.Run(false);
    return;
  }

  display_control_changing_ = true;

  // Turn off the displays before releasing control since we're no longer using
  // them for output.
  SetDisplayPowerInternal(
      chromeos::DISPLAY_POWER_ALL_OFF, kSetDisplayPowerNoFlags,
      base::Bind(&DisplayConfigurator::SendRelinquishDisplayControl,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void DisplayConfigurator::SendRelinquishDisplayControl(
    const DisplayControlCallback& callback,
    bool success) {
  if (success) {
    // Set the flag early such that an incoming configuration event won't start
    // while we're releasing control of the displays.
    display_externally_controlled_ = true;
    native_display_delegate_->RelinquishDisplayControl(
        base::Bind(&DisplayConfigurator::OnDisplayControlRelinquished,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    display_control_changing_ = false;
    callback.Run(false);
  }
}

void DisplayConfigurator::OnDisplayControlRelinquished(
    const DisplayControlCallback& callback,
    bool success) {
  display_control_changing_ = false;
  display_externally_controlled_ = success;
  if (!success) {
    force_configure_ = true;
    RunPendingConfiguration();
  }

  callback.Run(success);
}

void DisplayConfigurator::ForceInitialConfigure(
    uint32_t background_color_argb) {
  if (!configure_display_ || display_externally_controlled_)
    return;

  DCHECK(native_display_delegate_);
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

uint64_t DisplayConfigurator::RegisterContentProtectionClient() {
  if (!configure_display_ || display_externally_controlled_)
    return INVALID_CLIENT_ID;

  return next_display_protection_client_id_++;
}

void DisplayConfigurator::UnregisterContentProtectionClient(
    uint64_t client_id) {
  client_protection_requests_.erase(client_id);

  ContentProtections protections;
  for (const auto& requests_pair : client_protection_requests_) {
    for (const auto& protections_pair : requests_pair.second) {
      protections[protections_pair.first] |= protections_pair.second;
    }
  }

  set_protection_callbacks_.push(base::Bind(&DoNothing));
  ApplyContentProtectionTask* task = new ApplyContentProtectionTask(
      layout_manager_.get(), native_display_delegate_.get(), protections,
      base::Bind(&DisplayConfigurator::OnContentProtectionClientUnregistered,
                 weak_ptr_factory_.GetWeakPtr()));
  content_protection_tasks_.push(
      base::Bind(&ApplyContentProtectionTask::Run, base::Owned(task)));

  if (content_protection_tasks_.size() == 1)
    content_protection_tasks_.front().Run();
}

void DisplayConfigurator::OnContentProtectionClientUnregistered(bool success) {
  DCHECK(!content_protection_tasks_.empty());
  content_protection_tasks_.pop();

  DCHECK(!set_protection_callbacks_.empty());
  SetProtectionCallback callback = set_protection_callbacks_.front();
  set_protection_callbacks_.pop();

  if (!content_protection_tasks_.empty())
    content_protection_tasks_.front().Run();
}

void DisplayConfigurator::QueryContentProtectionStatus(
    uint64_t client_id,
    int64_t display_id,
    const QueryProtectionCallback& callback) {
  // Exclude virtual displays so that protected content will not be recaptured
  // through the cast stream.
  for (const DisplaySnapshot* display : cached_displays_) {
    if (display->display_id() == display_id &&
        !IsPhysicalDisplayType(display->type())) {
      callback.Run(false, 0, 0);
      return;
    }
  }

  if (!configure_display_ || display_externally_controlled_) {
    callback.Run(false, 0, 0);
    return;
  }

  query_protection_callbacks_.push(callback);
  QueryContentProtectionTask* task = new QueryContentProtectionTask(
      layout_manager_.get(), native_display_delegate_.get(), display_id,
      base::Bind(&DisplayConfigurator::OnContentProtectionQueried,
                 weak_ptr_factory_.GetWeakPtr(), client_id, display_id));
  content_protection_tasks_.push(
      base::Bind(&QueryContentProtectionTask::Run, base::Owned(task)));
  if (content_protection_tasks_.size() == 1)
    content_protection_tasks_.front().Run();
}

void DisplayConfigurator::OnContentProtectionQueried(
    uint64_t client_id,
    int64_t display_id,
    QueryContentProtectionTask::Response task_response) {
  bool success = task_response.success;
  uint32_t link_mask = task_response.link_mask;
  uint32_t protection_mask = 0;

  // Don't reveal protections requested by other clients.
  ProtectionRequests::iterator it = client_protection_requests_.find(client_id);
  if (success && it != client_protection_requests_.end()) {
    uint32_t requested_mask = 0;
    if (it->second.find(display_id) != it->second.end())
      requested_mask = it->second[display_id];
    protection_mask =
        task_response.enabled & ~task_response.unfulfilled & requested_mask;
  }

  DCHECK(!content_protection_tasks_.empty());
  content_protection_tasks_.pop();

  DCHECK(!query_protection_callbacks_.empty());
  QueryProtectionCallback callback = query_protection_callbacks_.front();
  query_protection_callbacks_.pop();
  callback.Run(success, link_mask, protection_mask);

  if (!content_protection_tasks_.empty())
    content_protection_tasks_.front().Run();
}

void DisplayConfigurator::SetContentProtection(
    uint64_t client_id,
    int64_t display_id,
    uint32_t desired_method_mask,
    const SetProtectionCallback& callback) {
  if (!configure_display_ || display_externally_controlled_) {
    callback.Run(false);
    return;
  }

  ContentProtections protections;
  for (const auto& requests_pair : client_protection_requests_) {
    for (const auto& protections_pair : requests_pair.second) {
      if (requests_pair.first == client_id &&
          protections_pair.first == display_id)
        continue;

      protections[protections_pair.first] |= protections_pair.second;
    }
  }
  protections[display_id] |= desired_method_mask;

  set_protection_callbacks_.push(callback);
  ApplyContentProtectionTask* task = new ApplyContentProtectionTask(
      layout_manager_.get(), native_display_delegate_.get(), protections,
      base::Bind(&DisplayConfigurator::OnSetContentProtectionCompleted,
                 weak_ptr_factory_.GetWeakPtr(), client_id, display_id,
                 desired_method_mask));
  content_protection_tasks_.push(
      base::Bind(&ApplyContentProtectionTask::Run, base::Owned(task)));
  if (content_protection_tasks_.size() == 1)
    content_protection_tasks_.front().Run();
}

void DisplayConfigurator::OnSetContentProtectionCompleted(
    uint64_t client_id,
    int64_t display_id,
    uint32_t desired_method_mask,
    bool success) {
  DCHECK(!content_protection_tasks_.empty());
  content_protection_tasks_.pop();

  DCHECK(!set_protection_callbacks_.empty());
  SetProtectionCallback callback = set_protection_callbacks_.front();
  set_protection_callbacks_.pop();

  if (!success) {
    callback.Run(false);
    return;
  }

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

  callback.Run(true);
  if (!content_protection_tasks_.empty())
    content_protection_tasks_.front().Run();
}

std::vector<ColorCalibrationProfile>
DisplayConfigurator::GetAvailableColorCalibrationProfiles(int64_t display_id) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDisplayColorCalibration)) {
    for (const DisplaySnapshot* display : cached_displays_) {
      if (display->display_id() == display_id &&
          IsPhysicalDisplayType(display->type())) {
        return native_display_delegate_->GetAvailableColorCalibrationProfiles(
            *display);
      }
    }
  }

  return std::vector<ColorCalibrationProfile>();
}

bool DisplayConfigurator::SetColorCalibrationProfile(
    int64_t display_id,
    ColorCalibrationProfile new_profile) {
  for (const DisplaySnapshot* display : cached_displays_) {
    if (display->display_id() == display_id &&
        IsPhysicalDisplayType(display->type())) {
      return native_display_delegate_->SetColorCalibrationProfile(*display,
                                                                  new_profile);
    }
  }

  return false;
}

bool DisplayConfigurator::SetColorCorrection(
    int64_t display_id,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  for (const DisplaySnapshot* display : cached_displays_) {
    if (display->display_id() == display_id)
      return native_display_delegate_->SetColorCorrection(
          *display, degamma_lut, gamma_lut, correction_matrix);
  }

  return false;
}

void DisplayConfigurator::PrepareForExit() {
  configure_display_ = false;
}

void DisplayConfigurator::SetDisplayPowerInternal(
    chromeos::DisplayPowerState power_state,
    int flags,
    const ConfigurationCallback& callback) {
  // Only skip if the current power state is the same and the latest requested
  // power state is the same. If |pending_power_state_ != current_power_state_|
  // then there is a current task pending or the last configuration failed. In
  // either case request a new configuration to make sure the state is
  // consistent with the expectations.
  if (power_state == current_power_state_ &&
      power_state == pending_power_state_ &&
      !(flags & kSetDisplayPowerForceProbe)) {
    callback.Run(true);
    return;
  }

  pending_power_state_ = power_state;
  has_pending_power_state_ = true;
  pending_power_flags_ = flags;
  queued_configuration_callbacks_.push_back(callback);

  if (configure_timer_.IsRunning()) {
    // If there is a configuration task scheduled, avoid performing
    // configuration immediately. Instead reset the timer to wait for things to
    // settle.
    configure_timer_.Reset();
    return;
  }

  RunPendingConfiguration();
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

  requested_power_state_ = power_state;
  SetDisplayPowerInternal(requested_power_state_, flags, callback);
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
    NotifyDisplayStateObservers(true, new_state);
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
  configure_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
                         this, &DisplayConfigurator::ConfigureDisplays);
}

void DisplayConfigurator::OnDisplaySnapshotsInvalidated() {
  VLOG(1) << "Display snapshots invalidated.";
  cached_displays_.clear();
}

void DisplayConfigurator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DisplayConfigurator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DisplayConfigurator::SuspendDisplays(
    const ConfigurationCallback& callback) {
  if (!configure_display_ || display_externally_controlled_) {
    callback.Run(false);
    return;
  }

  displays_suspended_ = true;

  // Stop |configure_timer_| because we will force probe and configure all the
  // displays at resume time anyway.
  configure_timer_.Stop();

  // Turn off the displays for suspend. This way, if we wake up for lucid sleep,
  // the displays will not turn on (all displays should be off for lucid sleep
  // unless explicitly requested by lucid sleep code). Use
  // SetDisplayPowerInternal so requested_power_state_ is maintained.
  SetDisplayPowerInternal(chromeos::DISPLAY_POWER_ALL_OFF,
                          kSetDisplayPowerNoFlags, callback);

  // We need to make sure that the monitor configuration we just did actually
  // completes before we return.
  native_display_delegate_->SyncWithServer();
}

void DisplayConfigurator::ResumeDisplays() {
  if (!configure_display_ || display_externally_controlled_)
    return;

  displays_suspended_ = false;

  if (current_display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR ||
      current_display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED ||
      current_display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED) {
    // When waking up from suspend while being in a multi display mode, we
    // schedule a delayed forced configuration, which will make
    // SetDisplayPowerInternal() avoid performing the configuration immediately.
    // This gives a chance to wait for all displays to be added and detected
    // before configuration is performed, so we won't immediately resize the
    // desktops and the windows on it to fit on a single display.
    configure_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
                                          kResumeConfigureMultiDisplayDelayMs),
                           this, &DisplayConfigurator::ConfigureDisplays);
  }

  // If requested_power_state_ is ALL_OFF due to idle suspend, powerd will turn
  // the display power on when it enables the backlight.
  SetDisplayPower(requested_power_state_, kSetDisplayPowerNoFlags,
                  base::Bind(&DoNothing));
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
      requested_display_state_, pending_power_state_, pending_power_flags_, 0,
      force_configure_, base::Bind(&DisplayConfigurator::OnConfigured,
                                   weak_ptr_factory_.GetWeakPtr())));
  configuration_task_->SetVirtualDisplaySnapshots(virtual_display_snapshots_);

  // Reset the flags before running the task; otherwise it may end up scheduling
  // another configuration.
  force_configure_ = false;
  pending_power_flags_ = kSetDisplayPowerNoFlags;
  has_pending_power_state_ = false;
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
    chromeos::DisplayPowerState old_power_state = current_power_state_;
    current_display_state_ = new_display_state;
    current_power_state_ = new_power_state;

    // |framebuffer_size| is empty in software mirroring mode, headless mode,
    // or all displays are off.
    DCHECK(!framebuffer_size.IsEmpty() ||
           (mirroring_controller_ &&
            mirroring_controller_->SoftwareMirroringEnabled()) ||
           new_display_state == MULTIPLE_DISPLAY_STATE_HEADLESS ||
           new_power_state == chromeos::DISPLAY_POWER_ALL_OFF);

    if (!framebuffer_size.IsEmpty())
      framebuffer_size_ = framebuffer_size;

    // If the pending power state hasn't changed then make sure that value
    // gets updated as well since the last requested value may have been
    // dependent on certain conditions (ie: if only the internal monitor was
    // present).
    if (!has_pending_power_state_)
      pending_power_state_ = new_power_state;

    if (old_power_state != current_power_state_)
      NotifyPowerStateObservers();
  }

  configuration_task_.reset();
  NotifyDisplayStateObservers(success, new_display_state);
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
  if (has_pending_power_state_)
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

void DisplayConfigurator::NotifyDisplayStateObservers(
    bool success,
    MultipleDisplayState attempted_state) {
  if (success) {
    for (Observer& observer : observers_)
      observer.OnDisplayModeChanged(cached_displays_);
  } else {
    for (Observer& observer : observers_)
      observer.OnDisplayModeChangeFailed(cached_displays_, attempted_state);
  }
}

void DisplayConfigurator::NotifyPowerStateObservers() {
  for (Observer& observer : observers_)
    observer.OnPowerStateChanged(current_power_state_);
}

int64_t DisplayConfigurator::AddVirtualDisplay(const gfx::Size& display_size) {
  if (last_virtual_display_id_ == 0xff) {
    LOG(WARNING) << "Exceeded virtual display id limit";
    return kInvalidDisplayId;
  }

  int64_t display_id = GenerateDisplayID(kReservedManufacturerID, 0x0,
                                         ++last_virtual_display_id_);
  virtual_display_snapshots_.push_back(
      base::MakeUnique<DisplaySnapshotVirtual>(display_id, display_size));
  ConfigureDisplays();

  return display_id;
}

bool DisplayConfigurator::RemoveVirtualDisplay(int64_t display_id) {
  bool display_found = false;
  for (auto it = virtual_display_snapshots_.begin();
       it != virtual_display_snapshots_.end(); ++it) {
    if ((*it)->display_id() == display_id) {
      virtual_display_snapshots_.erase(it);
      ConfigureDisplays();
      display_found = true;
      break;
    }
  }

  if (!display_found)
    return false;

  int64_t max_display_id = 0;
  for (const auto& display : virtual_display_snapshots_)
    max_display_id = std::max(max_display_id, display->display_id());
  last_virtual_display_id_ = max_display_id & 0xff;

  return true;
}

bool DisplayConfigurator::IsDisplayOn() const {
  return current_power_state_ != chromeos::DISPLAY_POWER_ALL_OFF;
}

}  // namespace display
