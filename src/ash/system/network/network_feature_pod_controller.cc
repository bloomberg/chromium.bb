// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::CellularStateProperties;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

namespace {

std::u16string GetSubLabelForConnectedNetwork(
    const NetworkStateProperties* network) {
  DCHECK(network &&
         chromeos::network_config::StateIsConnected(network->connection_state));

  if (!chromeos::network_config::NetworkStateMatchesType(
          network, NetworkType::kWireless)) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
  }

  if (network->type == NetworkType::kCellular) {
    CellularStateProperties* cellular =
        network->type_state->get_cellular().get();
    if (cellular->network_technology == onc::cellular::kTechnologyCdma1Xrtt) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_ONE_X);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyGsm) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GSM);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyGprs) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GPRS);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyEdge) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_EDGE);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyEvdo ||
        cellular->network_technology == onc::cellular::kTechnologyUmts) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_THREE_G);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyHspa) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyHspaPlus) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA_PLUS);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyLte) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyLteAdvanced) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE_PLUS);
    }

    // All connectivity types exposed by Shill should be covered above. However,
    // as a fail-safe, return the default "Connected" string here to protect
    // against Shill providing an unexpected value.
    NOTREACHED();
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
  }

  int signal_strength =
      chromeos::network_config::GetWirelessSignalStrength(network);

  switch (network_icon::GetSignalStrength(signal_strength)) {
    case network_icon::SignalStrength::NONE:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
    case network_icon::SignalStrength::WEAK:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK_SUBLABEL);
    case network_icon::SignalStrength::MEDIUM:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_MEDIUM_SUBLABEL);
    case network_icon::SignalStrength::STRONG:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG_SUBLABEL);
  }
  NOTREACHED();
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
}

// Returns |true| if the network type can be toggled to state |enabled|.
bool NetworkTypeCanBeToggled(const NetworkStateProperties* network,
                             bool enabled) {
  // The default behavior is to change the enabled state of WiFi.
  if (!network || network->type == NetworkType::kWiFi)
    return true;

  // Cellular and tether networks can only be disabled from clicking the feature
  // pod toggle, not enabled.
  if (!enabled && (network->type == NetworkType::kCellular ||
                   network->type == NetworkType::kTether)) {
    return true;
  }
  return false;
}

// Returns |true| if the network type is actually toggled.
bool SetNetworkTypeEnabled(bool enabled) {
  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();
  const NetworkStateProperties* default_network = model->default_network();

  if (!NetworkTypeCanBeToggled(default_network, enabled))
    return false;

  model->SetNetworkTypeEnabledState(
      default_network ? default_network->type : NetworkType::kWiFi, enabled);
  return true;
}

}  // namespace

NetworkFeaturePodController::NetworkFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
}

NetworkFeaturePodController::~NetworkFeaturePodController() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

void NetworkFeaturePodController::NetworkIconChanged() {
  UpdateButtonStateIfExists();
}

FeaturePodButton* NetworkFeaturePodController::CreateButton() {
  DCHECK(!button_);
  std::unique_ptr<NetworkFeaturePodButton> button =
      std::make_unique<NetworkFeaturePodButton>(/*controller=*/this,
                                                /*delegate=*/this);
  button_ = button.get();
  button_->ShowDetailedViewArrow();
  UpdateButtonStateIfExists();
  return button.release();
}

void NetworkFeaturePodController::OnIconPressed() {
  bool was_enabled = button_->IsToggled();
  bool can_toggle = SetNetworkTypeEnabled(!was_enabled);

  // The detailed view should be shown when we enable a network technology as
  // well as when the network technology cannot be toggled, e.g. ethernet.
  if (!was_enabled || !can_toggle)
    tray_controller_->ShowNetworkDetailedView(/*force=*/!can_toggle);
}

void NetworkFeaturePodController::OnLabelPressed() {
  SetNetworkTypeEnabled(true);
  tray_controller_->ShowNetworkDetailedView(/*force=*/true);
}

SystemTrayItemUmaType NetworkFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_NETWORK;
}

void NetworkFeaturePodController::OnFeaturePodButtonThemeChanged() {
  // All of the network icons will need to be redrawn.
  Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->PurgeNetworkIconCache();

  UpdateButtonStateIfExists();
}

void NetworkFeaturePodController::ActiveNetworkStateChanged() {
  UpdateButtonStateIfExists();
}

void NetworkFeaturePodController::UpdateButtonStateIfExists() {
  // Check |button_| here so that calling functions don't need to.
  if (!button_)
    return;

  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();
  const chromeos::network_config::mojom::NetworkStateProperties*
      default_network = model->default_network();

  const bool toggled =
      default_network ||
      model->GetDeviceState(NetworkType::kWiFi) == DeviceStateType::kEnabled;
  const network_icon::IconType icon_type =
      toggled ? network_icon::ICON_TYPE_FEATURE_POD_TOGGLED
              : network_icon::ICON_TYPE_FEATURE_POD;

  button_->SetToggled(toggled);

  bool image_animating = false;
  ActiveNetworkIcon* active_network_icon =
      Shell::Get()->system_tray_model()->active_network_icon();

  button_->icon_button()->SetImage(
      views::Button::STATE_NORMAL,
      active_network_icon->GetImage(ActiveNetworkIcon::Type::kSingle, icon_type,
                                    &image_animating));
  button_->icon_button()->SetImage(
      views::Button::STATE_DISABLED,
      active_network_icon->GetImage(
          ActiveNetworkIcon::Type::kSingle,
          network_icon::ICON_TYPE_FEATURE_POD_DISABLED, &image_animating));

  if (image_animating) {
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  } else {
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  // Modifying network state is not allowed when the screen is locked.
  button_->SetEnabled(!Shell::Get()->session_controller()->IsScreenLocked());

  std::u16string tooltip;
  Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetConnectionStatusStrings(ActiveNetworkIcon::Type::kSingle,
                                   /*a11y_name=*/nullptr,
                                   /*a11y_desc=*/nullptr, &tooltip);

  button_->SetLabel(ComputeButtonLabel(default_network));
  button_->SetSubLabel(ComputeButtonSubLabel(default_network));

  if (!button_->GetEnabled()) {
    button_->SetIconAndLabelTooltips(tooltip);
    return;
  }

  // Make sure the tooltip only indicates the network type will be toggled by
  // pressing the icon only when it can be toggled (e.g. not ethernet).
  if (!NetworkTypeCanBeToggled(default_network, !toggled)) {
    button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP, tooltip));
    return;
  }

  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP, tooltip));

  // Make sure the tooltip indicates the network type will be toggled by
  // pressing the label if the network type is disabled.
  button_->SetLabelTooltip(l10n_util::GetStringFUTF16(
      toggled ? IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP
              : IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP,
      tooltip));
}

std::u16string NetworkFeaturePodController::ComputeButtonLabel(
    const chromeos::network_config::mojom::NetworkStateProperties* network)
    const {
  if (!network) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_LABEL);
  }
  return network->type == NetworkType::kEthernet
             ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET)
             : base::UTF8ToUTF16(network->name);
}

std::u16string NetworkFeaturePodController::ComputeButtonSubLabel(
    const chromeos::network_config::mojom::NetworkStateProperties* network)
    const {
  if (!network ||
      network->connection_state == ConnectionStateType::kNotConnected) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL);
  }

  // Check whether the network is currently activating first since activating
  // networks are still considered connected.
  if (network->type == NetworkType::kCellular &&
      network->type_state->get_cellular()->activation_state ==
          ActivationStateType::kActivating) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING_SUBLABEL);
  }

  if (chromeos::network_config::StateIsConnected(network->connection_state)) {
    return GetSubLabelForConnectedNetwork(network);
  }

  DCHECK(network->connection_state == ConnectionStateType::kConnecting);

  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL);
}

}  // namespace ash
