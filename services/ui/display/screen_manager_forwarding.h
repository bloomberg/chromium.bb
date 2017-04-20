// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_SCREEN_MANAGER_FORWARDING_H_
#define SERVICES_UI_DISPLAY_SCREEN_MANAGER_FORWARDING_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/ui/display/screen_manager.h"
#include "ui/display/mojo/native_display_delegate.mojom.h"
#include "ui/display/types/native_display_observer.h"

namespace display {

class NativeDisplayDelegate;

// ScreenManager implementation that implements mojom::NativeDisplayDelegate.
// This will own a real NativeDisplayDelegate and forwards calls to and
// responses from it over Mojo.
class ScreenManagerForwarding
    : public ScreenManager,
      public NativeDisplayObserver,
      public mojom::NativeDisplayDelegate,
      service_manager::InterfaceFactory<mojom::NativeDisplayDelegate> {
 public:
  ScreenManagerForwarding();
  ~ScreenManagerForwarding() override;

  // ScreenManager:
  void AddInterfaces(service_manager::BinderRegistry* registry) override;
  void Init(ScreenManagerDelegate* delegate) override;
  void RequestCloseDisplay(int64_t display_id) override;
  display::ScreenBase* GetScreen() override;

  // NativeDisplayObserver:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

  // mojom::NativeDisplayDelegate:
  void Initialize(mojom::NativeDisplayObserverPtr observer) override;
  void TakeDisplayControl(const TakeDisplayControlCallback& callback) override;
  void RelinquishDisplayControl(
      const RelinquishDisplayControlCallback& callback) override;
  void GetDisplays(const GetDisplaysCallback& callback) override;
  void Configure(int64_t display_id,
                 std::unique_ptr<display::DisplayMode> mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback) override;
  void GetHDCPState(int64_t display_id,
                    const GetHDCPStateCallback& callback) override;
  void SetHDCPState(int64_t display_id,
                    display::HDCPState state,
                    const SetHDCPStateCallback& callback) override;
  void SetColorCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut,
      const std::vector<float>& correction_matrix) override;

  // service_manager::InterfaceFactory<mojom::NativeDisplayDelegate>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::NativeDisplayDelegateRequest request) override;

 private:
  // Forwards results from GetDisplays() back with |callback|.
  void ForwardGetDisplays(
      const mojom::NativeDisplayDelegate::GetDisplaysCallback& callback,
      const std::vector<DisplaySnapshot*>& displays);

  // Forwards results from call to Configure() back with |callback|.
  void ForwardConfigure(
      DisplaySnapshot* snapshot,
      const DisplayMode* mode,
      const gfx::Point& origin,
      const mojom::NativeDisplayDelegate::ConfigureCallback& callback,
      bool status);

  std::unique_ptr<display::ScreenBase> screen_;
  mojo::Binding<mojom::NativeDisplayDelegate> binding_;
  mojom::NativeDisplayObserverPtr observer_;

  std::unique_ptr<display::NativeDisplayDelegate> native_display_delegate_;

  // Cached pointers to snapshots owned by the |native_display_delegate_|.
  std::unordered_map<int64_t, DisplaySnapshot*> snapshot_map_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerForwarding);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_SCREEN_MANAGER_FORWARDING_H_
