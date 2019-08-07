// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_

#include <string>

#include "ash/login_status.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace views {
class Button;
}

namespace ash {

class TrayNetworkStateModel;

namespace tray {

// Exported for tests.
class ASH_EXPORT NetworkStateListDetailedView : public TrayDetailedView {
 public:
  ~NetworkStateListDetailedView() override;

  void Init();

  // Called when the contents of the network list have changed or when any
  // Manager properties (e.g. technology state) have changed.
  void Update();

  void ToggleInfoBubbleForTesting();

  // views::View:
  const char* GetClassName() const override;

 protected:
  enum ListType { LIST_TYPE_NETWORK, LIST_TYPE_VPN };

  NetworkStateListDetailedView(DetailedViewDelegate* delegate,
                               ListType list_type,
                               LoginStatus login);

  // Refreshes the network list.
  virtual void UpdateNetworkList() = 0;

  // Checks whether |view| represents a network in the list. If yes, sets
  // |guid| to the network's guid and returns |true|. Otherwise,
  // leaves |guid| unchanged and returns |false|.
  virtual bool IsNetworkEntry(views::View* view, std::string* guid) const = 0;

 private:
  class InfoBubble;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;

  // Implementation of 'HandleViewClicked' once networks are received.
  void HandleViewClickedImpl(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network);

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Update info and settings buttons in header.
  void UpdateHeaderButtons();

  // Update scanning progress bar.
  void UpdateScanningBar();

  // Create and manage the network info bubble.
  void ToggleInfoBubble();
  bool ResetInfoBubble();
  void OnInfoBubbleDestroyed();
  views::View* CreateNetworkInfoView();

  // Scan and start timer to periodically request a network scan.
  void ScanAndStartTimer();

  // Request a network scan.
  void CallRequestScan();

  bool IsWifiEnabled();

  // Type of list (all networks or vpn)
  ListType list_type_;

  // Track login state.
  LoginStatus login_;

  TrayNetworkStateModel* model_;

  views::Button* info_button_;
  views::Button* settings_button_;

  // A small bubble for displaying network info.
  InfoBubble* info_bubble_;

  // Timer for starting and stopping network scans.
  base::RepeatingTimer network_scan_repeating_timer_;

  base::WeakPtrFactory<NetworkStateListDetailedView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkStateListDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_
