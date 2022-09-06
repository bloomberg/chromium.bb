// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class UnifiedSystemTrayController;

// This class encapsulates the logic to update the detailed Bluetooth device
// page within the quick settings and translate user interaction with the
// detailed view into Bluetooth state changes.
class ASH_EXPORT BluetoothDetailedViewController
    : public DetailedViewController,
      public chromeos::bluetooth_config::mojom::SystemPropertiesObserver,
      public BluetoothDetailedView::Delegate {
 public:
  explicit BluetoothDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  BluetoothDetailedViewController(const BluetoothDetailedViewController&) =
      delete;
  BluetoothDetailedViewController& operator=(
      const BluetoothDetailedViewController&) = delete;
  ~BluetoothDetailedViewController() override;

 protected:
  using PairedBluetoothDevicePropertiesPtrs = std::vector<
      chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>;

 private:
  // DetailedViewControllerBase:
  views::View* CreateView() override;
  std::u16string GetAccessibleName() const override;

  // chromeos::bluetooth_config::mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(
      chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr
          properties) override;

  // BluetoothDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override;
  void OnPairNewDeviceRequested() override;
  void OnDeviceListItemSelected(
      const chromeos::bluetooth_config::mojom::
          PairedBluetoothDevicePropertiesPtr& device) override;

  // Used to update |view_| and |device_list_controller_| when the cached
  // Bluetooth state has changed.
  void BluetoothEnabledStateChanged();

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  mojo::Remote<chromeos::bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<chromeos::bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  chromeos::bluetooth_config::mojom::BluetoothSystemState system_state_ =
      chromeos::bluetooth_config::mojom::BluetoothSystemState::kUnavailable;
  BluetoothDetailedView* view_ = nullptr;
  std::unique_ptr<BluetoothDeviceListController> device_list_controller_;
  PairedBluetoothDevicePropertiesPtrs connected_devices_;
  PairedBluetoothDevicePropertiesPtrs previously_connected_devices_;
  UnifiedSystemTrayController* tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
