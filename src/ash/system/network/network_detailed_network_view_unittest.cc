// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_network_view_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using chromeos::network_config::CrosNetworkConfigTestHelper;

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

const char kStubCellularDevicePath[] = "/device/stub_cellular_device";
const char kStubCellularDeviceName[] = "stub_cellular_device";

class FakeNetworkDetailedNetworkViewDelegate
    : public NetworkDetailedNetworkView::Delegate {
 public:
  FakeNetworkDetailedNetworkViewDelegate() = default;
  ~FakeNetworkDetailedNetworkViewDelegate() override = default;

  bool last_mobile_toggle_state() { return last_mobile_toggle_state_; }

  size_t on_mobile_toggle_clicked_count() {
    return on_mobile_toggle_clicked_count_;
  }

  bool last_wifi_toggle_state() { return last_wifi_toggle_state_; }

  size_t on_wifi_toggle_clicked_count() {
    return on_wifi_toggle_clicked_count_;
  }

  size_t network_list_item_selected_count() const {
    return network_list_item_selected_count_;
  }

 private:
  // NetworkDetailedView::Delegate:
  void OnNetworkListItemSelected(
      const NetworkStatePropertiesPtr& network) override {
    network_list_item_selected_count_++;
    last_network_list_item_selected_ = mojo::Clone(network);
  };

  // NetworkDetailedNetworkView::Delegate:
  void OnWifiToggleClicked(bool new_state) override {
    on_wifi_toggle_clicked_count_++;
    last_wifi_toggle_state_ = new_state;
  };

  void OnMobileToggleClicked(bool new_state) override {
    on_mobile_toggle_clicked_count_++;
    last_mobile_toggle_state_ = new_state;
  };

  const NetworkStatePropertiesPtr& last_network_list_item_selected() const {
    return last_network_list_item_selected_;
  }

  bool last_mobile_toggle_state_ = false;
  size_t on_mobile_toggle_clicked_count_ = 0;

  bool last_wifi_toggle_state_ = false;
  size_t on_wifi_toggle_clicked_count_ = 0;

  size_t network_list_item_selected_count_ = 0;
  NetworkStatePropertiesPtr last_network_list_item_selected_;
};
}  // namespace

class NetworkDetailedNetworkViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    network_state_helper()->ClearDevices();

    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);

    network_state_helper()->device_test()->AddDevice(
        kStubCellularDevicePath, shill::kTypeCellular, kStubCellularDeviceName);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();

    detailed_view_delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);

    std::unique_ptr<NetworkDetailedNetworkViewImpl>
        network_detailed_network_view =
            std::make_unique<NetworkDetailedNetworkViewImpl>(
                detailed_view_delegate_.get(),
                &fake_network_detailed_network_delagte_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_detailed_network_view_ =
        widget_->SetContentsView(std::move(network_detailed_network_view));
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  NetworkListNetworkItemView* AddNetworkListItem() {
    return network_detailed_network_view()->AddNetworkListItem();
  }

  void NotifyNetworkListChanged() {
    network_detailed_network_view()->NotifyNetworkListChanged();
  }

  NetworkListNetworkHeaderView* AddWifiSectionHeader() {
    return network_detailed_network_view()->AddWifiSectionHeader();
  }

  NetworkListNetworkHeaderView* AddMobileSectionHeader() {
    return network_detailed_network_view()->AddMobileSectionHeader();
  }

  FakeNetworkDetailedNetworkViewDelegate* delegate() {
    return &fake_network_detailed_network_delagte_;
  }

  void SimulateMobileToggleClicked(bool new_state) {
    network_detailed_network_view()->OnMobileToggleClicked(new_state);
  }

  void SimulateWifiToggleClicked(bool new_state) {
    network_detailed_network_view()->OnWifiToggleClicked(new_state);
  }

 private:
  NetworkDetailedNetworkViewImpl* network_detailed_network_view() {
    return network_detailed_network_view_;
  }

  chromeos::NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkDetailedNetworkViewDelegate fake_network_detailed_network_delagte_;
  std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
  NetworkDetailedNetworkViewImpl* network_detailed_network_view_;
};

TEST_F(NetworkDetailedNetworkViewTest, ViewsAreCreated) {
  NetworkListNetworkItemView* network_list_item = AddNetworkListItem();
  ASSERT_NE(nullptr, network_list_item);
  EXPECT_STREQ("NetworkListNetworkItemView", network_list_item->GetClassName());

  NetworkListNetworkHeaderView* wifi_section = AddWifiSectionHeader();
  ASSERT_NE(nullptr, wifi_section);
  EXPECT_STREQ("NetworkListWifiHeaderView", wifi_section->GetClassName());

  NetworkListNetworkHeaderView* mobile_section = AddMobileSectionHeader();
  ASSERT_NE(nullptr, mobile_section);
  EXPECT_STREQ("NetworkListMobileHeaderView", mobile_section->GetClassName());
}

TEST_F(NetworkDetailedNetworkViewTest, ToggleInteractions) {
  EXPECT_EQ(0u, delegate()->on_mobile_toggle_clicked_count());
  EXPECT_FALSE(delegate()->last_mobile_toggle_state());

  SimulateMobileToggleClicked(/*new_state=*/true);
  EXPECT_EQ(1u, delegate()->on_mobile_toggle_clicked_count());
  EXPECT_TRUE(delegate()->last_mobile_toggle_state());

  EXPECT_EQ(0u, delegate()->on_wifi_toggle_clicked_count());
  EXPECT_FALSE(delegate()->last_wifi_toggle_state());

  SimulateWifiToggleClicked(/*new_state=*/true);
  EXPECT_EQ(1u, delegate()->on_wifi_toggle_clicked_count());
  EXPECT_TRUE(delegate()->last_wifi_toggle_state());
}

TEST_F(NetworkDetailedNetworkViewTest, ListItemClicked) {
  EXPECT_EQ(0u, delegate()->network_list_item_selected_count());
  NetworkListNetworkItemView* network_list_item = AddNetworkListItem();
  ASSERT_NE(nullptr, network_list_item);
  NotifyNetworkListChanged();
  LeftClickOn(network_list_item);
  EXPECT_EQ(1u, delegate()->network_list_item_selected_count());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(network_list_item);
  EXPECT_EQ(1u, delegate()->network_list_item_selected_count());
}

}  // namespace ash