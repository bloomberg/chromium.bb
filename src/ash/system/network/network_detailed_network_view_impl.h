// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_IMPL_H_

#include "ash/ash_export.h"

#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

namespace ash {

class DetailedViewDelegate;

// This class is an implementation for NetworkDetailedNetworkView.
class ASH_EXPORT NetworkDetailedNetworkViewImpl
    : public NetworkDetailedView,
      public NetworkDetailedNetworkView,
      public NetworkListNetworkHeaderView::Delegate {
 public:
  METADATA_HEADER(NetworkDetailedNetworkViewImpl);

  NetworkDetailedNetworkViewImpl(
      DetailedViewDelegate* detailed_view_delegate,
      NetworkDetailedNetworkView::Delegate* delegate);
  NetworkDetailedNetworkViewImpl(const NetworkDetailedNetworkViewImpl&) =
      delete;
  NetworkDetailedNetworkViewImpl& operator=(
      const NetworkDetailedNetworkViewImpl&) = delete;
  ~NetworkDetailedNetworkViewImpl() override;

 private:
  friend class NetworkDetailedNetworkViewTest;

  // NetworkDetailedNetworkView:
  void NotifyNetworkListChanged() override;
  views::View* GetAsView() override;
  NetworkListNetworkItemView* AddNetworkListItem() override;
  NetworkListNetworkHeaderView* AddMobileSectionHeader() override;
  NetworkListNetworkHeaderView* AddWifiSectionHeader() override;
  views::View* network_list() override;

  // NetworkListNetworkHeaderView::Delegate:
  void OnMobileToggleClicked(bool new_state) override;
  void OnWifiToggleClicked(bool new_state) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
