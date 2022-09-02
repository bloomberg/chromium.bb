// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/network/network_row_title_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view.h"

namespace ash {

class TrayNetworkStateModel;

// This class is used for the headers of networks (Mobile and Wifi), and
// implements the functionality shared between the headers of Mobile and Wifi
// network headers.
class ASH_EXPORT NetworkListNetworkHeaderView : public NetworkListHeaderView {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnMobileToggleClicked(bool new_state) = 0;
    virtual void OnWifiToggleClicked(bool new_state) = 0;
  };

  NetworkListNetworkHeaderView(Delegate* delegate, int label_id);
  NetworkListNetworkHeaderView(const NetworkListNetworkHeaderView&) = delete;
  NetworkListNetworkHeaderView& operator=(const NetworkListNetworkHeaderView&) =
      delete;
  ~NetworkListNetworkHeaderView() override;

  virtual void SetToggleState(bool enabled, bool is_on);

  void SetToggleVisibility(bool visible);

 protected:
  virtual void AddExtraButtons();

  // Called when |toggle_| is clicked and toggled. Subclasses should override to
  // enabled/disable their respective technology.
  virtual void OnToggleToggled(bool is_on);

  Delegate* delegate() const { return delegate_; };

  TrayNetworkStateModel* model() { return model_; }

  // Used for testing.
  static constexpr int kToggleButtonId =
      NetworkListHeaderView::kTitleLabelViewId + 1;

 private:
  friend class NetworkListNetworkHeaderViewTest;
  friend class NetworkListMobileHeaderViewTest;
  friend class NetworkListWifiHeaderViewTest;
  friend class NetworkListViewControllerTest;

  void ToggleButtonPressed();

  TrayNetworkStateModel* model_;

  // ToggleButton to toggle section on or off.
  views::ToggleButton* toggle_ = nullptr;

  Delegate* delegate_ = nullptr;

  base::WeakPtrFactory<NetworkListNetworkHeaderView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_HEADER_VIEW_H_
