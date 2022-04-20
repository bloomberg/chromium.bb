// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view.h"

#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

NetworkDetailedView::NetworkDetailedView(
    DetailedViewDelegate* detailed_view_delegate,
    Delegate* delegate,
    ListType list_type)
    : TrayDetailedView(detailed_view_delegate),
      list_type_(list_type),
      login_(Shell::Get()->session_controller()->login_status()),
      model_(Shell::Get()->system_tray_model()->network_state_model()),
      delegate_(delegate) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());

  CreateTitleRow(list_type_ == ListType::LIST_TYPE_NETWORK
                     ? IDS_ASH_STATUS_TRAY_NETWORK
                     : IDS_ASH_STATUS_TRAY_VPN);
  CreateTitleRowButtons();
  CreateScrollableList();
  // TODO(b/207089013): add metrics for UI surface displayed.
}

NetworkDetailedView::~NetworkDetailedView() = default;

void NetworkDetailedView::NotifyNetworkListChanged() {
  scroll_content()->InvalidateLayout();
  Layout();
}

void NetworkDetailedView::HandleViewClicked(views::View* view) {
  if (login_ == LoginStatus::LOCKED)
    return;
  // TODO(b/207089013): Call OnNetworkListItemSelected() on delegate() and pass
  // in a cast of either NetworkListNetworkItemView or NetworkListVPNItemView
  // when available, also add test for this.
}

views::View* NetworkDetailedView::network_list() {
  return scroll_content();
}

void NetworkDetailedView::CreateTitleRowButtons() {
  DCHECK(!info_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, true);

  std::unique_ptr<views::Button> info = base::WrapUnique(
      CreateInfoButton(base::BindRepeating(&NetworkDetailedView::OnInfoClicked,
                                           weak_ptr_factory_.GetWeakPtr()),
                       IDS_ASH_STATUS_TRAY_NETWORK_INFO));
  info->SetID(static_cast<int>(NetworkDetailedViewChildId::kInfoButton));
  info_button_ = info.get();
  tri_view()->AddView(TriView::Container::END, info.release());

  DCHECK(!settings_button_);

  std::unique_ptr<views::Button> settings =
      base::WrapUnique(CreateSettingsButton(
          base::BindRepeating(&NetworkDetailedView::OnSettingsClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
  settings->SetID(
      static_cast<int>(NetworkDetailedViewChildId::kSettingsButton));
  settings_button_ = settings.get();
  tri_view()->AddView(TriView::Container::END, settings.release());
}

bool NetworkDetailedView::ShouldIncludeDeviceAddresses() {
  return list_type_ == LIST_TYPE_NETWORK;
}

void NetworkDetailedView::OnInfoBubbleDestroyed() {
  info_bubble_ = nullptr;

  // Widget of info bubble is activated while info bubble is shown. To move
  // focus back to the widget of this view, activate it again here.
  GetWidget()->Activate();
}

void NetworkDetailedView::OnInfoClicked() {
  if (CloseInfoBubble())
    return;

  info_bubble_ =
      new NetworkInfoBubble(weak_ptr_factory_.GetWeakPtr(), tri_view());
  views::BubbleDialogDelegateView::CreateBubble(info_bubble_)->Show();
  info_bubble_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, false);
}

bool NetworkDetailedView::CloseInfoBubble() {
  if (!info_bubble_)
    return false;

  info_bubble_->GetWidget()->Close();
  return true;
}

void NetworkDetailedView::OnSettingsClicked() {
  // TODO(b/207089013): Record user action metrics here

  const std::string guid = model_->default_network()
                               ? model_->default_network()->guid
                               : std::string();

  CloseBubble();  // Deletes |this|.

  SystemTrayClient* system_tray_client =
      Shell::Get()->system_tray_model()->client();
  if (system_tray_client)
    system_tray_client->ShowNetworkSettings(guid);
}

}  // namespace ash
