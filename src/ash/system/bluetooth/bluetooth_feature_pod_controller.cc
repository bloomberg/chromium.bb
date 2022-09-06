// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {
using chromeos::bluetooth_config::GetPairedDeviceName;
using chromeos::bluetooth_config::mojom::BluetoothModificationState;
using chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr;
using chromeos::bluetooth_config::mojom::BluetoothSystemState;
using chromeos::bluetooth_config::mojom::DeviceConnectionState;
using chromeos::bluetooth_config::mojom::PairedBluetoothDeviceProperties;
}  // namespace

BluetoothFeaturePodController::BluetoothFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

BluetoothFeaturePodController::~BluetoothFeaturePodController() = default;

FeaturePodButton* BluetoothFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->ShowDetailedViewArrow();
  UpdateButtonStateIfExists();
  return button_;
}

void BluetoothFeaturePodController::OnIconPressed() {
  if (!button_->GetEnabled())
    return;
  const bool is_toggled = button_->IsToggled();
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(!is_toggled);
  if (!is_toggled)
    tray_controller_->ShowBluetoothDetailedView();
}

void BluetoothFeaturePodController::OnLabelPressed() {
  if (!button_->GetEnabled())
    return;
  if (!button_->IsToggled())
    remote_cros_bluetooth_config_->SetBluetoothEnabledState(true);
  tray_controller_->ShowBluetoothDetailedView();
}

SystemTrayItemUmaType BluetoothFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_BLUETOOTH;
}

BluetoothFeaturePodController::BluetoothDeviceNameAndBatteryInfo::
    BluetoothDeviceNameAndBatteryInfo(
        const std::u16string& device_name,
        chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr battery_info)
    : device_name(device_name), battery_info(std::move(battery_info)) {}

BluetoothFeaturePodController::BluetoothDeviceNameAndBatteryInfo::
    ~BluetoothDeviceNameAndBatteryInfo() = default;

bool BluetoothFeaturePodController::DoesFirstConnectedDeviceHaveBatteryInfo()
    const {
  return first_connected_device_.has_value() &&
         first_connected_device_.value().battery_info &&
         (first_connected_device_.value().battery_info->default_properties ||
          first_connected_device_.value().battery_info->left_bud_info ||
          first_connected_device_.value().battery_info->right_bud_info ||
          first_connected_device_.value().battery_info->case_info);
}

const gfx::VectorIcon& BluetoothFeaturePodController::ComputeButtonIcon()
    const {
  if (!button_->IsToggled())
    return kUnifiedMenuBluetoothDisabledIcon;

  if (first_connected_device_.has_value())
    return kUnifiedMenuBluetoothConnectedIcon;

  return kUnifiedMenuBluetoothIcon;
}

std::u16string BluetoothFeaturePodController::ComputeButtonLabel() const {
  if (button_->IsToggled() && first_connected_device_.has_value() &&
      connected_device_count_ == 1)
    return first_connected_device_.value().device_name;

  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH);
}

int BluetoothFeaturePodController::
    GetFirstConnectedDeviceBatteryLevelForDisplay() const {
  // If there are any multiple battery details, we should prioritize showing
  // them over the default battery in order to match the Quick Settings
  // Bluetooth sub-page battery details shown. Android only shows the left bud
  // if there are multiple batteries, so we match that here, and if it doesn't
  // exist, we prioritize the right bud battery, then the case battery, if they
  // exist over the default battery in order to match any detailed battery
  // shown on the sub-page.
  if (first_connected_device_.value().battery_info->left_bud_info)
    return first_connected_device_.value()
        .battery_info->left_bud_info->battery_percentage;

  if (first_connected_device_.value().battery_info->right_bud_info)
    return first_connected_device_.value()
        .battery_info->right_bud_info->battery_percentage;

  if (first_connected_device_.value().battery_info->case_info)
    return first_connected_device_.value()
        .battery_info->case_info->battery_percentage;

  return first_connected_device_.value()
      .battery_info->default_properties->battery_percentage;
}

std::u16string BluetoothFeaturePodController::ComputeButtonSubLabel() const {
  if (!button_->IsToggled())
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_SHORT);

  if (!first_connected_device_.has_value())
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_SHORT);

  if (connected_device_count_ == 1) {
    if (DoesFirstConnectedDeviceHaveBatteryInfo()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
          base::NumberToString16(
              GetFirstConnectedDeviceBatteryLevelForDisplay()));
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_LABEL);
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_LABEL,
      base::FormatNumber(connected_device_count_));
}

std::u16string BluetoothFeaturePodController::ComputeTooltip() const {
  if (!first_connected_device_.has_value())
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP);

  if (connected_device_count_ == 1) {
    if (DoesFirstConnectedDeviceHaveBatteryInfo()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_WITH_BATTERY_TOOLTIP,
          first_connected_device_.value().device_name,
          base::NumberToString16(
              GetFirstConnectedDeviceBatteryLevelForDisplay()));
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP,
        first_connected_device_.value().device_name);
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
      base::FormatNumber(connected_device_count_));
}

void BluetoothFeaturePodController::UpdateButtonStateIfExists() {
  // Check |button_| here so that calling functions don't need to.
  if (!button_)
    return;
  if (system_state_ == BluetoothSystemState::kUnavailable) {
    button_->SetVisible(false);
    button_->SetEnabled(false);
    return;
  }

  button_->SetEnabled(modification_state_ ==
                      BluetoothModificationState::kCanModifyBluetooth);
  button_->SetToggled(
      ::chromeos::bluetooth_config::IsBluetoothEnabledOrEnabling(
          system_state_));
  button_->SetVisible(true);

  button_->SetVectorIcon(ComputeButtonIcon());
  button_->SetLabel(ComputeButtonLabel());
  button_->SetSubLabel(ComputeButtonSubLabel());

  if (!button_->IsToggled()) {
    button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)));
  } else {
    button_->SetIconTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, ComputeTooltip()));
    button_->SetLabelTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP, ComputeTooltip()));
  }
}

void BluetoothFeaturePodController::OnPropertiesUpdated(
    BluetoothSystemPropertiesPtr properties) {
  connected_device_count_ = 0;
  first_connected_device_.reset();

  // Counts the number of connected devices and caches the name and battery
  // information of the first connected device found.
  for (const auto& paired_device : properties->paired_devices) {
    if (paired_device->device_properties->connection_state !=
        DeviceConnectionState::kConnected) {
      continue;
    }
    ++connected_device_count_;
    if (first_connected_device_.has_value())
      continue;
    first_connected_device_.emplace(
        GetPairedDeviceName(paired_device),
        mojo::Clone(paired_device->device_properties->battery_info));
  }
  modification_state_ = properties->modification_state;
  system_state_ = properties->system_state;
  UpdateButtonStateIfExists();
}

}  // namespace ash
