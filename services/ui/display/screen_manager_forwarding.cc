// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/screen_manager_forwarding.h"

#include <utility>

#include "base/bind.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/display/screen_base.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot_mojo.h"
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

ScreenManagerForwarding::ScreenManagerForwarding()
    : screen_(base::MakeUnique<display::ScreenBase>()), binding_(this) {}

ScreenManagerForwarding::~ScreenManagerForwarding() {
  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);
}

void ScreenManagerForwarding::AddInterfaces(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface<mojom::NativeDisplayDelegate>(this);
}

void ScreenManagerForwarding::Init(ScreenManagerDelegate* delegate) {
  // Done in NativeDisplayDelegate::Initialize() instead.
}

void ScreenManagerForwarding::RequestCloseDisplay(int64_t display_id) {}

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
    mojom::NativeDisplayObserverPtr observer) {
  DCHECK(!native_display_delegate_);
  observer_ = std::move(observer);

  native_display_delegate_ =
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
  native_display_delegate_->AddObserver(this);
  native_display_delegate_->Initialize();
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
    std::unique_ptr<display::DisplayMode> mode,
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
      GetCorrespondingMode(*snapshot, mode.get());
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

void ScreenManagerForwarding::Create(
    const service_manager::Identity& remote_identity,
    mojom::NativeDisplayDelegateRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

void ScreenManagerForwarding::ForwardGetDisplays(
    const mojom::NativeDisplayDelegate::GetDisplaysCallback& callback,
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
