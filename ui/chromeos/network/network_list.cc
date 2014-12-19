// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/network/network_list.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "grit/ui_chromeos_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/network/network_icon.h"
#include "ui/chromeos/network/network_icon_animation.h"
#include "ui/chromeos/network/network_info.h"
#include "ui/chromeos/network/network_list_delegate.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ui {

// NetworkListView:

NetworkListView::NetworkListView(NetworkListDelegate* delegate)
    : delegate_(delegate),
      content_(NULL),
      scanning_view_(NULL),
      no_wifi_networks_view_(NULL),
      no_cellular_networks_view_(NULL) {
  CHECK(delegate);
}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkList() {
  CHECK(content_);
  NetworkStateHandler::NetworkStateList network_list;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);
  UpdateNetworkListInternal();
}

bool NetworkListView::IsViewInList(views::View* view,
                                   std::string* service_path) const {
  std::map<views::View*, std::string>::const_iterator found =
      network_map_.find(view);
  if (found == network_map_.end())
    return false;
  *service_path = found->second;
  return true;
}

void NetworkListView::UpdateNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  network_list_.clear();
  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin();
       iter != networks.end();
       ++iter) {
    const chromeos::NetworkState* network = *iter;
    if (!pattern.MatchesType(network->type()))
      continue;
    NetworkInfo* info = new NetworkInfo(network->path());
    network_list_.push_back(info);
  }
}

void NetworkListView::UpdateNetworkListInternal() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // First, update state for all networks
  bool animating = false;
  for (size_t i = 0; i < network_list_.size(); ++i) {
    NetworkInfo* info = network_list_[i];
    const chromeos::NetworkState* network =
        handler->GetNetworkState(info->service_path);
    if (!network)
      continue;
    info->image =
        network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->label =
        network_icon::GetLabelForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->highlight =
        network->IsConnectedState() || network->IsConnectingState();
    info->disable =
        network->activation_state() == shill::kActivationStateActivating;
    if (!animating && network->IsConnectingState())
      animating = true;
  }

  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  // Get the updated list entries
  network_map_.clear();
  std::set<std::string> new_service_paths;
  bool needs_relayout = UpdateNetworkListEntries(&new_service_paths);

  // Remove old children
  std::set<std::string> remove_service_paths;
  for (ServicePathMap::const_iterator it = service_path_map_.begin();
       it != service_path_map_.end();
       ++it) {
    if (new_service_paths.find(it->first) == new_service_paths.end()) {
      remove_service_paths.insert(it->first);
      network_map_.erase(it->second);
      content_->RemoveChildView(it->second);
      needs_relayout = true;
    }
  }

  for (std::set<std::string>::const_iterator remove_it =
           remove_service_paths.begin();
       remove_it != remove_service_paths.end();
       ++remove_it) {
    service_path_map_.erase(*remove_it);
  }

  if (needs_relayout) {
    views::View* selected_view = NULL;
    for (ServicePathMap::const_iterator iter = service_path_map_.begin();
         iter != service_path_map_.end();
         ++iter) {
      if (delegate_->IsViewHovered(iter->second)) {
        selected_view = iter->second;
        break;
      }
    }
    content_->SizeToPreferredSize();
    delegate_->RelayoutScrollList();
    if (selected_view)
      content_->ScrollRectToVisible(selected_view->bounds());
  }
}

bool NetworkListView::UpdateNetworkListEntries(
    std::set<std::string>* new_service_paths) {
  bool needs_relayout = false;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // Insert child views
  int index = 0;

  // Highlighted networks
  for (size_t i = 0; i < network_list_.size(); ++i) {
    const NetworkInfo* info = network_list_[i];
    if (info->highlight) {
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }
  }

  const NetworkTypePattern pattern = delegate_->GetNetworkTypePattern();
  if (pattern.MatchesPattern(NetworkTypePattern::Cellular())) {
    // Cellular initializing
    int message_id = network_icon::GetCellularUninitializedMsg();
    if (!message_id &&
        handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()) &&
        !handler->FirstNetworkByType(NetworkTypePattern::Mobile())) {
      message_id = IDS_ASH_STATUS_TRAY_NO_CELLULAR_NETWORKS;
    }
    if (UpdateInfoLabel(message_id, index, &no_cellular_networks_view_))
      needs_relayout = true;
    if (message_id)
      ++index;
  }

  if (pattern.MatchesPattern(NetworkTypePattern::WiFi())) {
    // "Wifi Enabled / Disabled"
    int message_id = 0;
    if (network_list_.empty()) {
      message_id = handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())
                       ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                       : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
    }
    if (UpdateInfoLabel(message_id, index, &no_wifi_networks_view_))
      needs_relayout = true;
    if (message_id)
      ++index;

    // "Wifi Scanning"
    message_id = 0;
    if (handler->GetScanningByType(NetworkTypePattern::WiFi()))
      message_id = IDS_ASH_STATUS_TRAY_WIFI_SCANNING_MESSAGE;
    if (UpdateInfoLabel(message_id, index, &scanning_view_))
      needs_relayout = true;
    if (message_id)
      ++index;
  }

  // Un-highlighted networks
  for (size_t i = 0; i < network_list_.size(); ++i) {
    const NetworkInfo* info = network_list_[i];
    if (!info->highlight) {
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }
  }

  // No networks or other messages (fallback)
  if (index == 0) {
    int message_id = 0;
    if (pattern.Equals(NetworkTypePattern::VPN()))
      message_id = IDS_ASH_STATUS_TRAY_NETWORK_NO_VPN;
    else
      message_id = IDS_ASH_STATUS_TRAY_NO_NETWORKS;
    if (UpdateInfoLabel(message_id, index, &scanning_view_))
      needs_relayout = true;
  }

  return needs_relayout;
}

bool NetworkListView::UpdateNetworkChild(int index, const NetworkInfo* info) {
  bool needs_relayout = false;
  views::View* container = NULL;
  ServicePathMap::const_iterator found =
      service_path_map_.find(info->service_path);
  if (found == service_path_map_.end()) {
    container = delegate_->CreateViewForNetwork(*info);
    content_->AddChildViewAt(container, index);
    needs_relayout = true;
  } else {
    container = found->second;
    container->RemoveAllChildViews(true);
    delegate_->UpdateViewForNetwork(container, *info);
    container->Layout();
    container->SchedulePaint();
    needs_relayout = PlaceViewAtIndex(container, index);
  }
  if (info->disable)
    container->SetEnabled(false);
  network_map_[container] = info->service_path;
  service_path_map_[info->service_path] = container;
  return needs_relayout;
}

bool NetworkListView::PlaceViewAtIndex(views::View* view, int index) {
  if (content_->child_at(index) == view)
    return false;
  content_->ReorderChildView(view, index);
  return true;
}

bool NetworkListView::UpdateInfoLabel(int message_id,
                                      int index,
                                      views::Label** label) {
  CHECK(label);
  bool needs_relayout = false;
  if (message_id) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    base::string16 text = rb.GetLocalizedString(message_id);
    if (!*label) {
      *label = delegate_->CreateInfoLabel();
      (*label)->SetText(text);
      content_->AddChildViewAt(*label, index);
      needs_relayout = true;
    } else {
      (*label)->SetText(text);
      needs_relayout = PlaceViewAtIndex(*label, index);
    }
  } else if (*label) {
    content_->RemoveChildView(*label);
    delete *label;
    *label = NULL;
    needs_relayout = true;
  }
  return needs_relayout;
}

void NetworkListView::NetworkIconChanged() {
  UpdateNetworkList();
}

}  // namespace ui
