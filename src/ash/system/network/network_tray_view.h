// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_

#include <memory>

#include "ash/session/session_observer.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace chromeos {
class NetworkState;
}  // namespace chromeos

namespace ash {
namespace tray {

class NetworkTrayIconStrategy;

// Returns the connected, non-virtual (aka VPN), network.
const chromeos::NetworkState* GetConnectedNetwork();

class NetworkTrayView : public TrayItemView,
                        public network_icon::AnimationObserver,
                        public SessionObserver,
                        public TrayNetworkStateObserver::Delegate {
 public:
  ~NetworkTrayView() override;

  // Creates a NetworkTrayView that shows non-mobile network state.
  static NetworkTrayView* CreateForDefault(Shelf* shelf);
  // Creates a NetworkTrayView that only shows Mobile network state.
  static NetworkTrayView* CreateForMobile(Shelf* shelf);
  // Creates a NetworkTrayView that shows all networks state.
  static NetworkTrayView* CreateForSingleIcon(Shelf* shelf);

  const char* GetClassName() const override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TrayNetworkStateObserver::Delegate:
  void NetworkStateChanged(bool notify_a11y) override;

 private:
  NetworkTrayView(Shelf* shelf,
                  std::unique_ptr<NetworkTrayIconStrategy> network_icon_type);

  void UpdateIcon(bool tray_icon_visible, const gfx::ImageSkia& image);

  void UpdateNetworkStateHandlerIcon();

  // Updates connection status and notifies accessibility event when necessary.
  void UpdateConnectionStatus(const chromeos::NetworkState* connected_network,
                              bool notify_a11y);

  base::string16 connection_status_string_;
  base::string16 connection_status_tooltip_;

  std::unique_ptr<TrayNetworkStateObserver> network_state_observer_;
  std::unique_ptr<NetworkTrayIconStrategy> network_tray_icon_strategy_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_
