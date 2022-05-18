// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_mobile_header_view_impl.h"

#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::NetworkType;

int GetAddESimTooltipMessageId() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  if (!cellular_device)
    return 0;

  switch (cellular_device->inhibit_reason) {
    case chromeos::network_config::mojom::InhibitReason::kInstallingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_INSTALLING_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kRenamingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RENAMING_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kRemovingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REMOVING_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kConnectingToProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_CONNECTING_TO_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kRefreshingProfileList:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REFRESHING_PROFILE_LIST;
    case chromeos::network_config::mojom::InhibitReason::kNotInhibited:
      return IDS_ASH_STATUS_TRAY_ADD_CELLULAR_LABEL;
    case chromeos::network_config::mojom::InhibitReason::kResettingEuiccMemory:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RESETTING_ESIM;
    case chromeos::network_config::mojom::InhibitReason::kDisablingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_DISABLING_PROFILE;
  }
}

}  // namespace

NetworkListMobileHeaderViewImpl::NetworkListMobileHeaderViewImpl(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListMobileHeaderView(delegate) {
  AddExtraButtons();
}

NetworkListMobileHeaderViewImpl::~NetworkListMobileHeaderViewImpl() = default;

void NetworkListMobileHeaderViewImpl::AddExtraButtons() {
  // The button navigates to Settings, only add it if this can occur.
  if (!TrayPopupUtils::CanOpenWebUISettings())
    return;

  const gfx::VectorIcon& icon = base::i18n::IsRTL() ? kAddCellularNetworkRtlIcon
                                                    : kAddCellularNetworkIcon;
  std::unique_ptr<IconButton> add_esim_button = std::make_unique<IconButton>(
      base::BindRepeating(
          &NetworkListMobileHeaderViewImpl::AddESimButtonPressed,
          weak_factory_.GetWeakPtr()),
      IconButton::Type::kSmall, &icon, GetAddESimTooltipMessageId());
  add_esim_button.get()->SetID(kAddESimButtonId);
  add_esim_button_ = add_esim_button.get();
  container()->AddView(TriView::Container::END, add_esim_button.release());
};

void NetworkListMobileHeaderViewImpl::OnToggleToggled(bool is_on) {
  delegate()->OnMobileToggleClicked(is_on);
}

void NetworkListMobileHeaderViewImpl::AddESimButtonPressed() {
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kCellular);
}

void NetworkListMobileHeaderViewImpl::SetAddESimButtonState(bool enabled,
                                                            bool visible) {
  if (!add_esim_button_)
    return;

  add_esim_button_->SetVisible(visible);
  add_esim_button_->SetEnabled(enabled);
  add_esim_button_->SetTooltipText(
      l10n_util::GetStringUTF16(GetAddESimTooltipMessageId()));
}

}  // namespace ash