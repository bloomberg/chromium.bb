// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/screen_manager_forwarding.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/system/devicemode.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/display/screen_base.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot_mojo.h"
#include "ui/display/types/fake_display_controller.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"

namespace display {
namespace {

// Finds the display mode in |snapshot| that corresponds to |mode_to_find|.
const DisplayMode* GetCorrespondingMode(const DisplaySnapshot& snapshot,
                                        const DisplayMode* mode_to_find) {
  if (!mode_to_find)
    return nullptr;

  for (auto& mode : snapshot.modes()) {
    if (mode->size() == mode_to_find->size() &&
        mode->is_interlaced() == mode_to_find->is_interlaced() &&
        mode->refresh_rate() == mode_to_find->refresh_rate()) {
      return mode.get();
    }
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

ScreenManagerForwarding::ScreenManagerForwarding(Mode mode)
    : is_in_process_(mode == Mode::IN_WM_PROCESS),
      screen_(base::MakeUnique<display::ScreenBase>()),
      binding_(this),
      test_controller_binding_(this) {
  if (!is_in_process_)
    Screen::SetScreenInstance(screen_.get());
}

ScreenManagerForwarding::~ScreenManagerForwarding() {
  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);
  if (!is_in_process_)
    Screen::SetScreenInstance(nullptr);
}

void ScreenManagerForwarding::AddInterfaces(
    service_manager::BinderRegistryWithArgs<
        const service_manager::BindSourceInfo&>* registry) {
  registry->AddInterface<mojom::NativeDisplayDelegate>(
      base::Bind(&ScreenManagerForwarding::BindNativeDisplayDelegateRequest,
                 base::Unretained(this)));
  registry->AddInterface<mojom::TestDisplayController>(
      base::Bind(&ScreenManagerForwarding::BindTestDisplayControllerRequest,
                 base::Unretained(this)));
}

void ScreenManagerForwarding::Init(ScreenManagerDelegate* delegate) {
  // Done in NativeDisplayDelegate::Initialize() instead.
}

void ScreenManagerForwarding::RequestCloseDisplay(int64_t display_id) {
  if (!fake_display_controller_)
    return;

  // Tell NativeDisplayDelegate to remove the display. This triggers an
  // OnConfigurationChanged() and the corresponding display snapshot will be
  // gone.
  fake_display_controller_->RemoveDisplay(display_id);
}

display::ScreenBase* ScreenManagerForwarding::GetScreen() {
  return screen_.get();
}

void ScreenManagerForwarding::OnConfigurationChanged() {
  if (observer_.is_bound())
    observer_->OnConfigurationChanged();
}

void ScreenManagerForwarding::OnDisplaySnapshotsInvalidated() {
  snapshot_map_.clear();
}

void ScreenManagerForwarding::Initialize(
    mojom::NativeDisplayObserverPtr observer,
    const InitializeCallback& callback) {
  DCHECK(!native_display_delegate_);
  observer_ = std::move(observer);

  native_display_delegate_ =
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
  native_display_delegate_->AddObserver(this);
  native_display_delegate_->Initialize();

  // FakeDisplayController is only applicable when not running on a CrOS device.
  if (!chromeos::IsRunningAsSystemCompositor()) {
    fake_display_controller_ =
        native_display_delegate_->GetFakeDisplayController();
  }

  // Provide the list of display snapshots initially. ForwardingDisplayDelegate
  // will wait synchronously for this.
  native_display_delegate_->GetDisplays(
      base::Bind(&ScreenManagerForwarding::ForwardGetDisplays,
                 base::Unretained(this), callback));

  // When ForwardingDisplayDelegate receives this it will start asynchronous
  // operation and redo any configuration that was skipped.
  observer_->OnConfigurationChanged();
}

void ScreenManagerForwarding::TakeDisplayControl(
    const TakeDisplayControlCallback& callback) {
  DCHECK(native_display_delegate_);
  native_display_delegate_->TakeDisplayControl(callback);
}

void ScreenManagerForwarding::RelinquishDisplayControl(
    const RelinquishDisplayControlCallback& callback) {
  DCHECK(native_display_delegate_);
  native_display_delegate_->RelinquishDisplayControl(callback);
}

void ScreenManagerForwarding::GetDisplays(const GetDisplaysCallback& callback) {
  DCHECK(native_display_delegate_);
  native_display_delegate_->GetDisplays(
      base::Bind(&ScreenManagerForwarding::ForwardGetDisplays,
                 base::Unretained(this), callback));
}

void ScreenManagerForwarding::Configure(
    int64_t display_id,
    base::Optional<std::unique_ptr<display::DisplayMode>> mode,
    const gfx::Point& origin,
    const ConfigureCallback& callback) {
  DCHECK(native_display_delegate_);
  DisplaySnapshot* snapshot = snapshot_map_[display_id];
  if (!snapshot) {
    callback.Run(false);
    return;
  }

  // We need a pointer to the mode in |snapshot|, not the equivalent mode we
  // received over Mojo.
  const DisplayMode* snapshot_mode =
      mode ? GetCorrespondingMode(*snapshot, mode->get()) : nullptr;
  native_display_delegate_->Configure(
      *snapshot, snapshot_mode, origin,
      base::Bind(&ScreenManagerForwarding::ForwardConfigure,
                 base::Unretained(this), snapshot, snapshot_mode, origin,
                 callback));
}

void ScreenManagerForwarding::GetHDCPState(
    int64_t display_id,
    const GetHDCPStateCallback& callback) {
  DCHECK(native_display_delegate_);
  const DisplaySnapshot* snapshot = snapshot_map_[display_id];
  if (!snapshot) {
    callback.Run(false, HDCP_STATE_UNDESIRED);
    return;
  }

  native_display_delegate_->GetHDCPState(*snapshot, callback);
}

void ScreenManagerForwarding::SetHDCPState(
    int64_t display_id,
    display::HDCPState state,
    const SetHDCPStateCallback& callback) {
  DCHECK(native_display_delegate_);
  const DisplaySnapshot* snapshot = snapshot_map_[display_id];
  if (!snapshot) {
    callback.Run(false);
    return;
  }

  native_display_delegate_->SetHDCPState(*snapshot, state, callback);
}

void ScreenManagerForwarding::SetColorCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DCHECK(native_display_delegate_);
  const DisplaySnapshot* snapshot = snapshot_map_[display_id];
  if (!snapshot)
    return;

  native_display_delegate_->SetColorCorrection(*snapshot, degamma_lut,
                                               gamma_lut, correction_matrix);
}

void ScreenManagerForwarding::ToggleAddRemoveDisplay() {
  if (!fake_display_controller_)
    return;

  int num_displays = screen_->GetNumDisplays();
  if (num_displays == 1) {
    // If we have one display, add a second display with the same size.
    Display primary_display = screen_->GetPrimaryDisplay();
    fake_display_controller_->AddDisplay(primary_display.GetSizeInPixel());
  } else if (num_displays > 1) {
    // If we have more than one display, remove the first display we find that
    // isn't the primary display.
    Display primary_display = screen_->GetPrimaryDisplay();
    for (auto& display : screen_->display_list().displays()) {
      if (display.id() != primary_display.id()) {
        fake_display_controller_->RemoveDisplay(display.id());
        break;
      }
    }
  } else {
    // If we have no displays, add one with a default size.
    fake_display_controller_->AddDisplay(gfx::Size(1024, 768));
  }
}

void ScreenManagerForwarding::BindNativeDisplayDelegateRequest(
    mojom::NativeDisplayDelegateRequest request,
    const service_manager::BindSourceInfo& source_info) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

void ScreenManagerForwarding::BindTestDisplayControllerRequest(
    mojom::TestDisplayControllerRequest request,
    const service_manager::BindSourceInfo& source_info) {
  DCHECK(!test_controller_binding_.is_bound());
  test_controller_binding_.Bind(std::move(request));
}

void ScreenManagerForwarding::ForwardGetDisplays(
    const GetDisplaysCallback& callback,
    const std::vector<DisplaySnapshot*>& snapshots) {
  snapshot_map_.clear();

  // Convert the DisplaySnapshots to MojoDisplaySnapshots to allow sending
  // over Mojo. Also caches the snapshots for lookup later.
  std::vector<std::unique_ptr<DisplaySnapshotMojo>> mojo_snapshots;
  for (auto* snapshot : snapshots) {
    snapshot_map_[snapshot->display_id()] = snapshot;
    mojo_snapshots.push_back(DisplaySnapshotMojo::CreateFrom(*snapshot));
  }

  callback.Run(std::move(mojo_snapshots));
}

void ScreenManagerForwarding::ForwardConfigure(
    DisplaySnapshot* snapshot,
    const DisplayMode* mode,
    const gfx::Point& origin,
    const mojom::NativeDisplayDelegate::ConfigureCallback& callback,
    bool status) {
  if (status) {
    // Modify display snapshot similar to how ConfigureDisplaysTask would. Ozone
    // DRM needs these to be changed and ConfigureDisplaysTasks can't do it.
    snapshot->set_current_mode(mode);
    snapshot->set_origin(origin);
  }
  callback.Run(status);
}

}  // namespace display
