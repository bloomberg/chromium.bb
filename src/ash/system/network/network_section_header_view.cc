// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_section_header_view.h"

#include "ash/constants/ash_features.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/bind.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/onc/onc_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/controls/image_view.h"

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

namespace ash {
namespace tray {

namespace {

const int64_t kBluetoothTimeoutDelaySeconds = 2;

bool IsCellularSimLocked() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  return cellular_device &&
         !cellular_device->sim_lock_status->lock_type.empty();
}

void ShowCellularSettings() {
  Shell::Get()
      ->system_tray_model()
      ->network_state_model()
      ->cros_network_config()
      ->GetNetworkStateList(
          NetworkFilter::New(FilterType::kVisible, NetworkType::kCellular,
                             /*limit=*/1),
          base::BindOnce([](std::vector<NetworkStatePropertiesPtr> networks) {
            std::string guid;
            if (networks.size() > 0)
              guid = networks[0]->guid;
            Shell::Get()->system_tray_model()->client()->ShowNetworkSettings(
                guid);
          }));
}

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

bool IsESimSupported() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);

  if (!cellular_device || !cellular_device->sim_infos)
    return false;

  for (const auto& sim_info : *cellular_device->sim_infos) {
    if (!sim_info->eid.empty())
      return true;
  }
  return false;
}

}  // namespace

NetworkSectionHeaderView::NetworkSectionHeaderView(int title_id)
    : title_id_(title_id),
      model_(Shell::Get()->system_tray_model()->network_state_model()) {}

void NetworkSectionHeaderView::Init(bool enabled) {
  InitializeLayout();
  AddExtraButtons(enabled);
  AddToggleButton(enabled);
}

void NetworkSectionHeaderView::AddExtraButtons(bool enabled) {}

void NetworkSectionHeaderView::SetToggleVisibility(bool visible) {
  toggle_->SetVisible(visible);
}

void NetworkSectionHeaderView::SetToggleState(bool toggle_enabled, bool is_on) {
  toggle_->SetEnabled(toggle_enabled);
  toggle_->SetAcceptsEvents(toggle_enabled);
  toggle_->AnimateIsOn(is_on);
}

const char* NetworkSectionHeaderView::GetClassName() const {
  return "NetworkSectionHeaderView";
}

int NetworkSectionHeaderView::GetHeightForWidth(int width) const {
  // Make row height fixed avoiding layout manager adjustments.
  return GetPreferredSize().height();
}

void NetworkSectionHeaderView::InitializeLayout() {
  TrayPopupUtils::ConfigureAsStickyHeader(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = TrayPopupUtils::CreateSubHeaderRowView(true);
  container_->AddView(TriView::Container::START,
                      TrayPopupUtils::CreateMainImageView());
  AddChildView(container_);

  network_row_title_view_ = new NetworkRowTitleView(title_id_);
  container_->AddView(TriView::Container::CENTER, network_row_title_view_);
}

void NetworkSectionHeaderView::AddToggleButton(bool enabled) {
  toggle_ = TrayPopupUtils::CreateToggleButton(
      base::BindRepeating(&NetworkSectionHeaderView::ToggleButtonPressed,
                          base::Unretained(this)),
      title_id_);
  toggle_->SetIsOn(enabled);
  container_->AddView(TriView::Container::END, toggle_);
}

void NetworkSectionHeaderView::ToggleButtonPressed() {
  // In the event of frequent clicks, helps to prevent a toggle button state
  // from becoming inconsistent with the async operation of enabling /
  // disabling of mobile radio. The toggle will get unlocked in the next
  // call to NetworkListView::Update(). Note that we don't disable/enable
  // because that would clear focus.
  toggle_->SetAcceptsEvents(false);
  OnToggleToggled(toggle_->GetIsOn());
}

MobileSectionHeaderView::MobileSectionHeaderView()
    : NetworkSectionHeaderView(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE) {
  bool initially_enabled = model()->GetDeviceState(NetworkType::kCellular) ==
                               DeviceStateType::kEnabled ||
                           model()->GetDeviceState(NetworkType::kTether) ==
                               DeviceStateType::kEnabled;
  NetworkSectionHeaderView::Init(initially_enabled);
}

MobileSectionHeaderView::~MobileSectionHeaderView() {}

const char* MobileSectionHeaderView::GetClassName() const {
  return "MobileSectionHeaderView";
}

int MobileSectionHeaderView::UpdateToggleAndGetStatusMessage(
    bool mobile_has_networks,
    bool tether_has_networks) {
  DeviceStateType cellular_state =
      model()->GetDeviceState(NetworkType::kCellular);
  DeviceStateType tether_state = model()->GetDeviceState(NetworkType::kTether);

  bool default_toggle_enabled = !IsSecondaryUser();

  // If Cellular is available, toggle state and status message reflect Cellular.
  if (cellular_state != DeviceStateType::kUnavailable) {
    if (cellular_state == DeviceStateType::kUninitialized) {
      SetToggleVisibility(false);
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    }

    const DeviceStateProperties* cellular_device =
        model()->GetDevice(NetworkType::kCellular);
    if (cellular_device->sim_absent) {
      SetToggleVisibility(false);
      return IDS_ASH_STATUS_TRAY_SIM_CARD_MISSING;
    }
    bool toggle_enabled = default_toggle_enabled &&
                          (cellular_state == DeviceStateType::kEnabled ||
                           cellular_state == DeviceStateType::kDisabled);
    bool cellular_enabled = cellular_state == DeviceStateType::kEnabled;
    SetToggleVisibility(true);
    SetToggleState(toggle_enabled, cellular_enabled);
    if (cellular_state == DeviceStateType::kDisabling) {
      return IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLING;
    }

    if (cellular_device->sim_lock_status &&
        !cellular_device->sim_lock_status->lock_type.empty()) {
      return IDS_ASH_STATUS_TRAY_SIM_CARD_LOCKED;
    }
    if (cellular_device->scanning)
      return IDS_ASH_STATUS_TRAY_MOBILE_SCANNING;

    if (cellular_enabled && !mobile_has_networks) {
      // If no connectable Mobile network is available, show 'turn on
      // Bluetooth' if Tether is available but not initialized, otherwise
      // show 'no networks'.
      if (tether_state == DeviceStateType::kUninitialized)
        return IDS_ENABLE_BLUETOOTH;
      return IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
    }
    return 0;
  }

  // When Cellular is not available, always show the toggle.
  SetToggleVisibility(true);

  // Tether is also unavailable (edge case).
  if (tether_state == DeviceStateType::kUnavailable) {
    SetToggleState(false /* toggle_enabled */, false /* is_on */);
    return IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED;
  }

  // Otherwise, toggle state and status message reflect Tether.
  if (tether_state == DeviceStateType::kUninitialized) {
    if (waiting_for_tether_initialize_) {
      SetToggleState(false /* toggle_enabled */, true /* is_on */);
      // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    } else {
      SetToggleState(default_toggle_enabled, false /* is_on */);
      return IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH;
    }
  }

  bool tether_enabled = tether_state == DeviceStateType::kEnabled;

  if (waiting_for_tether_initialize_) {
    waiting_for_tether_initialize_ = false;
    enable_bluetooth_timer_.Stop();
    if (!tether_enabled) {
      // We enabled Bluetooth so Tether is now initialized, but it was not
      // enabled so enable it.
      model()->SetNetworkTypeEnabledState(NetworkType::kTether, true);
      SetToggleState(default_toggle_enabled, true /* is_on */);
      // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    }
  }

  // Ensure that the toggle state and status message match the tether state.
  SetToggleState(default_toggle_enabled, tether_enabled /* is_on */);
  if (tether_enabled && !tether_has_networks)
    return IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND;
  return 0;
}

void MobileSectionHeaderView::OnToggleToggled(bool is_on) {
  DeviceStateType cellular_state =
      model()->GetDeviceState(NetworkType::kCellular);

  // When Cellular is available, the toggle controls Cellular enabled state.
  // (Tether may be enabled by turning on Bluetooth and turning on
  // 'Get data connection' in the Settings > Mobile data subpage).
  if (cellular_state != DeviceStateType::kUnavailable) {
    if (is_on && IsCellularSimLocked()) {
      ShowCellularSettings();
      return;
    }
    model()->SetNetworkTypeEnabledState(NetworkType::kCellular, is_on);
    return;
  }

  DeviceStateType tether_state = model()->GetDeviceState(NetworkType::kTether);

  // Tether is also unavailable (edge case).
  if (tether_state == DeviceStateType::kUnavailable)
    return;

  // If Tether is available but uninitialized, we expect Bluetooth to be off.
  // Enable Bluetooth so that Tether will be initialized. Ignore edge cases
  // (e.g. Bluetooth was disabled from a different UI).
  if (tether_state == DeviceStateType::kUninitialized) {
    if (is_on && !waiting_for_tether_initialize_)
      EnableBluetooth();
    return;
  }

  // Otherwise the toggle controls the Tether enabled state.
  model()->SetNetworkTypeEnabledState(NetworkType::kTether, is_on);
}

void MobileSectionHeaderView::AddExtraButtons(bool enabled) {
  if (!chromeos::features::IsCellularActivationUiEnabled())
    return;

  if (IsESimSupported()) {
    PerformAddExtraButtons(enabled);
    return;
  }

  // Fetch the available networks, all of which should be PSIM networks.
  // If any are unactivated, PerformAddExtraButtons() should be called.
  Shell::Get()
      ->system_tray_model()
      ->network_state_model()
      ->cros_network_config()
      ->GetNetworkStateList(
          NetworkFilter::New(
              FilterType::kVisible, NetworkType::kCellular,
              /*limit=*/chromeos::network_config::mojom::kNoLimit),
          base::BindOnce(&MobileSectionHeaderView::OnCellularNetworksFetched,
                         weak_ptr_factory_.GetWeakPtr(), enabled));
}

void MobileSectionHeaderView::PerformAddExtraButtons(bool enabled) {
  TopShortcutButton* add_cellular_button = new TopShortcutButton(
      base::BindRepeating(&MobileSectionHeaderView::AddCellularButtonPressed,
                          base::Unretained(this)),
      vector_icons::kAddCellularNetworkIcon,
      IDS_ASH_STATUS_TRAY_ADD_CELLULAR_LABEL);
  add_cellular_button->SetEnabled(enabled);
  container()->AddView(TriView::Container::END, add_cellular_button);
}

void MobileSectionHeaderView::OnCellularNetworksFetched(
    bool enabled,
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  if (networks.empty())
    return;

  for (const auto& network : networks) {
    if (network->type_state->get_cellular()->activation_state !=
        chromeos::network_config::mojom::ActivationStateType::kActivated) {
      PerformAddExtraButtons(enabled);
      return;
    }
  }
}

void MobileSectionHeaderView::AddCellularButtonPressed() {
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kCellular);
}

void MobileSectionHeaderView::EnableBluetooth() {
  DCHECK(!waiting_for_tether_initialize_);

  Shell::Get()
      ->bluetooth_power_controller()
      ->SetPrimaryUserBluetoothPowerSetting(true /* enabled */);
  waiting_for_tether_initialize_ = true;
  enable_bluetooth_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kBluetoothTimeoutDelaySeconds),
      base::BindOnce(&MobileSectionHeaderView::OnEnableBluetoothTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MobileSectionHeaderView::OnEnableBluetoothTimeout() {
  DCHECK(waiting_for_tether_initialize_);
  waiting_for_tether_initialize_ = false;
  SetToggleState(true /* toggle_enabled */, false /* is_on */);

  LOG(ERROR) << "Error enabling Bluetooth. Cannot enable Mobile data.";
}

WifiSectionHeaderView::WifiSectionHeaderView()
    : NetworkSectionHeaderView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI) {
  bool enabled =
      model()->GetDeviceState(NetworkType::kWiFi) == DeviceStateType::kEnabled;
  NetworkSectionHeaderView::Init(enabled);
}

void WifiSectionHeaderView::SetToggleState(bool toggle_enabled, bool is_on) {
  join_button_->SetEnabled(toggle_enabled && is_on);
  NetworkSectionHeaderView::SetToggleState(toggle_enabled, is_on);
}

const char* WifiSectionHeaderView::GetClassName() const {
  return "WifiSectionHeaderView";
}

void WifiSectionHeaderView::OnToggleToggled(bool is_on) {
  model()->SetNetworkTypeEnabledState(NetworkType::kWiFi, is_on);
}

void WifiSectionHeaderView::AddExtraButtons(bool enabled) {
  auto* join_button = new TopShortcutButton(
      base::BindRepeating(&WifiSectionHeaderView::JoinButtonPressed,
                          base::Unretained(this)),
      vector_icons::kWifiAddIcon, IDS_ASH_STATUS_TRAY_OTHER_WIFI);
  join_button->SetEnabled(enabled);
  container()->AddView(TriView::Container::END, join_button);
  join_button_ = join_button;
}

void WifiSectionHeaderView::JoinButtonPressed() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kWiFi);
}

}  // namespace tray
}  // namespace ash
