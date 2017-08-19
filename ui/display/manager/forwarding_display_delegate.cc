// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/forwarding_display_delegate.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/display/types/display_snapshot_mojo.h"

namespace display {

ForwardingDisplayDelegate::ForwardingDisplayDelegate(
    mojom::NativeDisplayDelegatePtr delegate)
    : delegate_(std::move(delegate)), binding_(this) {}

ForwardingDisplayDelegate::~ForwardingDisplayDelegate() {}

void ForwardingDisplayDelegate::Initialize() {
  // TODO(kylechar/sky): Figure out how to make this not synchronous.
  mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_sync;

  // This is a synchronous call to Initialize() because ash depends on
  // NativeDisplayDelegate being synchronous during it's initialization. Calls
  // to GetDisplays() and Configure() will return early starting now using
  // whatever is in |snapshots_|.
  mojom::NativeDisplayObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  delegate_->Initialize(std::move(observer), &snapshots_);
}

void ForwardingDisplayDelegate::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  delegate_->TakeDisplayControl(callback);
}

void ForwardingDisplayDelegate::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  delegate_->TakeDisplayControl(callback);
}

void ForwardingDisplayDelegate::GetDisplays(
    const GetDisplaysCallback& callback) {
  if (!use_delegate_) {
    ForwardDisplays(callback);
    return;
  }

  delegate_->GetDisplays(
      base::Bind(&ForwardingDisplayDelegate::StoreAndForwardDisplays,
                 base::Unretained(this), callback));
}

void ForwardingDisplayDelegate::AddMode(const DisplaySnapshot& snapshot,
                                        const DisplayMode* mode) {}

void ForwardingDisplayDelegate::Configure(const DisplaySnapshot& snapshot,
                                          const DisplayMode* mode,
                                          const gfx::Point& origin,
                                          const ConfigureCallback& callback) {
  if (!use_delegate_) {
    // Pretend configuration succeeded. When the first OnConfigurationChanged()
    // is received this will run again and actually happen.
    callback.Run(true);
    return;
  }

  base::Optional<std::unique_ptr<DisplayMode>> transport_mode;
  if (mode)
    transport_mode = mode->Clone();
  delegate_->Configure(snapshot.display_id(), std::move(transport_mode), origin,
                       callback);
}

void ForwardingDisplayDelegate::CreateFrameBuffer(const gfx::Size& size) {}

void ForwardingDisplayDelegate::GetHDCPState(
    const DisplaySnapshot& snapshot,
    const GetHDCPStateCallback& callback) {
  delegate_->GetHDCPState(snapshot.display_id(), callback);
}

void ForwardingDisplayDelegate::SetHDCPState(
    const DisplaySnapshot& snapshot,
    HDCPState state,
    const SetHDCPStateCallback& callback) {
  delegate_->SetHDCPState(snapshot.display_id(), state, callback);
}

bool ForwardingDisplayDelegate::SetColorCorrection(
    const DisplaySnapshot& output,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  delegate_->SetColorCorrection(output.display_id(), degamma_lut, gamma_lut,
                                correction_matrix);
  // DrmNativeDisplayDelegate always returns true so this will too.
  return true;
}

void ForwardingDisplayDelegate::AddObserver(
    display::NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void ForwardingDisplayDelegate::RemoveObserver(
    display::NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

FakeDisplayController* ForwardingDisplayDelegate::GetFakeDisplayController() {
  return nullptr;
}

void ForwardingDisplayDelegate::OnConfigurationChanged() {
  // Start asynchronous operation once the first OnConfigurationChanged()
  // arrives. We know |delegate_| is usable at this point.
  use_delegate_ = true;

  // Forward OnConfigurationChanged received over Mojo to local observers.
  for (auto& observer : observers_)
    observer.OnConfigurationChanged();
}

void ForwardingDisplayDelegate::StoreAndForwardDisplays(
    const GetDisplaysCallback& callback,
    std::vector<std::unique_ptr<DisplaySnapshotMojo>> snapshots) {
  for (auto& observer : observers_)
    observer.OnDisplaySnapshotsInvalidated();
  snapshots_ = std::move(snapshots);

  ForwardDisplays(callback);
}

void ForwardingDisplayDelegate::ForwardDisplays(
    const GetDisplaysCallback& callback) {
  std::vector<DisplaySnapshot*> snapshot_ptrs;
  for (auto& snapshot : snapshots_)
    snapshot_ptrs.push_back(snapshot.get());
  callback.Run(snapshot_ptrs);
}

}  // namespace display
