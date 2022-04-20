// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

namespace media_router {

namespace {
MediaRoute CreateRouteForTesting(const MediaSinkInternal& sink) {
  std::string sink_id = sink.id();
  std::string route_id =
      "urn:x-org.chromium:media:route:1/" + sink_id + "/http://foo.com";
  return MediaRoute(route_id, MediaSource("access_code"), sink_id,
                    "access_sink", true);
}
}  // namespace

using SinkSource = CastDeviceCountMetrics::SinkSource;
using MockBoolCallback = base::MockCallback<base::OnceCallback<void(bool)>>;
using MockAddSinkResultCallback = base::MockCallback<
    media_router::AccessCodeCastSinkService::AddSinkResultCallback>;
using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

class AccessCodeCastSinkServiceTest : public testing::Test {
 public:
  AccessCodeCastSinkServiceTest()
      : mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_cast_socket_service_(
            std::make_unique<cast_channel::MockCastSocketService>(
                (mock_time_task_runner_))),
        message_handler_(mock_cast_socket_service_.get()),
        cast_media_sink_service_impl_(
            std::make_unique<MockCastMediaSinkServiceImpl>(
                OnSinksDiscoveredCallback(),
                mock_cast_socket_service_.get(),
                discovery_network_monitor_.get(),
                &dual_media_sink_service_)) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
    feature_list_.InitWithFeatures({features::kAccessCodeCastRememberDevices},
                                   {});
  }
  AccessCodeCastSinkServiceTest(AccessCodeCastSinkServiceTest&) = delete;
  AccessCodeCastSinkServiceTest& operator=(AccessCodeCastSinkServiceTest&) =
      delete;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterAccessCodeProfilePrefs(pref_service_->registry());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager()->CreateTestingProfile("foo_email");

    router_ = std::make_unique<NiceMock<media_router::MockMediaRouter>>();
    logger_ = std::make_unique<LoggerImpl>();
    ON_CALL(*router_, GetLogger()).WillByDefault(Return(logger_.get()));

    access_code_cast_sink_service_ =
        base::WrapUnique(new AccessCodeCastSinkService(
            profile_, router_.get(), cast_media_sink_service_impl_.get(),
            discovery_network_monitor_.get(), pref_service_.get()));
    access_code_cast_sink_service_->SetTaskRunnerForTest(
        mock_time_task_runner_);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    access_code_cast_sink_service_.reset();
    router_.reset();
    pref_service_.reset();
    task_environment_.RunUntilIdle();
    content::RunAllTasksUntilIdle();
  }

  void FastForwardUiAndIoTasks() {
    mock_time_task_runner()->FastForwardUntilNoTasksRemain();
    task_environment_.RunUntilIdle();
  }

  MockCastMediaSinkServiceImpl* mock_cast_media_sink_service_impl() {
    return cast_media_sink_service_impl_.get();
  }

  base::TestMockTimeTaskRunner* mock_time_task_runner() {
    return mock_time_task_runner_.get();
  }
  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  void ChangeConnectionType(network::mojom::ConnectionType connection_type) {
    discovery_network_monitor_->OnConnectionChanged(connection_type);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<media_router::MockMediaRouter> router_;
  std::unique_ptr<LoggerImpl> logger_;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  static std::vector<DiscoveryNetworkInfo> fake_network_info_;

  static const std::vector<DiscoveryNetworkInfo> fake_ethernet_info_;
  static const std::vector<DiscoveryNetworkInfo> fake_wifi_info_;
  static const std::vector<DiscoveryNetworkInfo> fake_unknown_info_;

  static std::vector<DiscoveryNetworkInfo> FakeGetNetworkInfo() {
    return fake_network_info_;
  }

  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&FakeGetNetworkInfo);

  raw_ptr<TestingProfile> profile_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;

  TestMediaSinkService dual_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  raw_ptr<base::MockOneShotTimer> mock_timer_;
  testing::NiceMock<cast_channel::MockCastMessageHandler> message_handler_;
  std::unique_ptr<MockCastMediaSinkServiceImpl> cast_media_sink_service_impl_;
  std::unique_ptr<AccessCodeCastSinkService> access_code_cast_sink_service_;
};

// static
const std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_ethernet_info_ = {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
// static
const std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_wifi_info_ = {
        DiscoveryNetworkInfo{std::string("wlp3s0"), std::string("wifi1")},
        DiscoveryNetworkInfo{std::string("wlp3s1"), std::string("wifi2")}};
// static
const std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_unknown_info_ = {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string()}};

// static
std::vector<DiscoveryNetworkInfo>
    AccessCodeCastSinkServiceTest::fake_network_info_ =
        AccessCodeCastSinkServiceTest::fake_ethernet_info_;

TEST_F(AccessCodeCastSinkServiceTest,
       AccessCodeCastDeviceRemovedAfterRouteEnds) {
  // Test to see that an AccessCode cast sink will be removed after the session
  // is ended.
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Add a non-access code cast sink to media router and route list.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1);
  std::vector<MediaRoute> route_list = {media_route_cast};

  // Expect that the removed_route_id_ member variable has not changes since no
  // route was removed.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_TRUE(access_code_cast_sink_service_->media_routes_observer_
                  ->removed_route_id_.empty());

  // Add a cast sink discovered by access code to the list of routes.
  MediaSinkInternal access_code_sink2 = CreateCastSink(2);
  access_code_sink2.cast_data().discovered_by_access_code = true;
  MediaRoute media_route_access = CreateRouteForTesting(access_code_sink2);

  route_list.push_back(media_route_access);

  // Expect that the removed_route_id_ member variable has not changes since no
  // route was removed.
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_TRUE(access_code_cast_sink_service_->media_routes_observer_
                  ->removed_route_id_.empty());

  // Remove the non-access code sink from the list of routes.
  route_list.erase(
      std::remove(route_list.begin(), route_list.end(), media_route_cast),
      route_list.end());
  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(
      access_code_cast_sink_service_->media_routes_observer_->removed_route_id_,
      media_route_cast.media_route_id());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  // Expect cast sink is NOT removed from the media router since it
  // is not an access code sink.
  access_code_cast_sink_service_->HandleMediaRouteDiscoveredByAccessCode(
      &cast_sink1);
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(cast_sink1))
      .Times(0);

  // Remove the access code sink from the list of routes.
  route_list.erase(
      std::remove(route_list.begin(), route_list.end(), media_route_access),
      route_list.end());

  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      route_list);
  EXPECT_EQ(
      access_code_cast_sink_service_->media_routes_observer_->removed_route_id_,
      media_route_access.media_route_id());
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  access_code_cast_sink_service_->HandleMediaRouteDiscoveredByAccessCode(
      &access_code_sink2);
  // Expect that there is a pending attempt to examine the sink to see if it
  // should be expired.
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              DisconnectAndRemoveSink(access_code_sink2));
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddExistingSinkToMediaRouter) {
  // Ensure that the call to OpenChannel is NOT made since the cast sink already
  // exists in the media router.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _))
      .Times(0);
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, Eq(cast_sink1.id())));
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddExistingSinkToMediaRouterWithRoute) {
  // When an existing sink has an existing route, ensure that that route is
  // terminated before the caller is alerted to the successful discovery.
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  cast_sink1.cast_data().discovered_by_access_code = true;

  MediaRoute media_route_cast = CreateRouteForTesting(cast_sink1);

  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated(
      {media_route_cast});
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*router_, GetCurrentRoutes())
      .WillOnce(Return(std::vector<MediaRoute>{media_route_cast}));
  EXPECT_CALL(*router_, TerminateRoute(media_route_cast.media_route_id()));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _))
      .Times(0);

  MockAddSinkResultCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, _));

  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), true);

  access_code_cast_sink_service_->media_routes_observer_->OnRoutesUpdated({});
  // Since a route has been removed, there should be a pending task to examine
  // whether the route's sink is an access code sink.
  EXPECT_EQ(
      access_code_cast_sink_service_->media_routes_observer_->removed_route_id_,
      media_route_cast.media_route_id());

  access_code_cast_sink_service_->HandleMediaRouteDiscoveredByAccessCode(
      &cast_sink1);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, AddNewSinkToMediaRouter) {
  // Make sure that the sink is added to the media router if it does not already
  // exist.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(cast_sink1, _, SinkSource::kAccessCode, _, _));
  EXPECT_CALL(mock_callback, Run(_, _)).Times(0);
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), false);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, DiscoveryDeviceMissingWithOk) {
  // Test to ensure that the add_sink_callback returns an EMPTY_RESPONSE if the
  // the device is missing.
  MockAddSinkResultCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::EMPTY_RESPONSE, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), absl::nullopt, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastSinkServiceTest, ValidDiscoveryDeviceAndCode) {
  // If discovery device is present, formatted correctly, and code is OK, no
  // callback should be run during OnAccessCodeValidated. Instead when the
  // channel opens successfully the callback should run with OK.
  MockAddSinkResultCallback mock_callback;
  MediaSinkInternal cast_sink1 = CreateCastSink(1);

  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto();
  discovery_device_proto.set_id("id1");

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, _));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(_));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannel(_, _, SinkSource::kAccessCode, _, _));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), discovery_device_proto, AddSinkResultCode::OK);

  // Assume sink is not present in the Media Router so a call to OpenChannel is
  // made.
  access_code_cast_sink_service_->OpenChannelIfNecessary(
      cast_sink1, mock_callback.Get(), false);

  // Channel successfully opens.
  access_code_cast_sink_service_->OnChannelOpenedResult(mock_callback.Get(),
                                                        "123456", true);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, InvalidDiscoveryDevice) {
  // If discovery device is present, but formatted incorrectly, and code is OK,
  // then callback should be SINK_CREATION_ERROR.
  MockAddSinkResultCallback mock_callback;

  // Create discovery_device with an invalid port
  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto("foo_display_name", "1234",
                                              "```````23489:1238:1239");

  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::SINK_CREATION_ERROR, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), discovery_device_proto, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastSinkServiceTest, NonOKResultCode) {
  // Check to see that any result code that isn't OK will return that error.
  MockAddSinkResultCallback mock_callback;

  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::AUTH_ERROR, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnAccessCodeValidated(
      mock_callback.Get(), absl::nullopt, AddSinkResultCode::AUTH_ERROR);
}

TEST_F(AccessCodeCastSinkServiceTest, OnChannelOpenedSuccess) {
  // Validate callback calls for OnChannelOpened for success.
  MockAddSinkResultCallback mock_callback;

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK, Eq("123456")));
  access_code_cast_sink_service_->OnChannelOpenedResult(mock_callback.Get(),
                                                        "123456", true);
}

TEST_F(AccessCodeCastSinkServiceTest, OnChannelOpenedFailure) {
  // Validate callback calls for OnChannelOpened for failure.
  MockAddSinkResultCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(AddSinkResultCode::CHANNEL_OPEN_ERROR, Eq(absl::nullopt)));
  access_code_cast_sink_service_->OnChannelOpenedResult(mock_callback.Get(),
                                                        "123456", false);
}

TEST_F(AccessCodeCastSinkServiceTest, SinkDoesntExistForPrefs) {
  // Ensure that the StoreSinkInPrefs() function returns if no sink exists in
  // the media router and prefs are changed.
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
  access_code_cast_sink_service_->StoreSinkInPrefs(nullptr);
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDiscoveredNetworksDict()
          ->GetDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAdditionTimeDict()
          ->GetDict()
          .empty());
  EXPECT_TRUE(access_code_cast_sink_service_->pref_updater_->GetDevicesDict()
                  ->GetDict()
                  .empty());
}

TEST_F(AccessCodeCastSinkServiceTest, TestFetchAndAddStoredDevices) {
  // Test that ensures OpenChannels is called after valid sinks are fetched from
  // the internal pref service.
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks_ethernet;
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink2.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink3.id())
          .value());

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks_ethernet, SinkSource::kAccessCode))
      .Times(1);

  FastForwardUiAndIoTasks();

  mock_time_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &DiscoveryNetworkMonitor::GetNetworkId,
          base::Unretained(discovery_network_monitor_.get()),
          base::BindOnce(&AccessCodeCastSinkService::FetchAndAddStoredDevices,
                         access_code_cast_sink_service_->GetWeakPtr())));

  // GetNetworkId() is run on the IO thread, so we must run RunUntilIdle and
  // RunAllTasks for that task to finish before we can continue with the
  // mock_task_runner.
  FastForwardUiAndIoTasks();
  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest, TestChangeNetworks) {
  // Test that ensures sinks are stored on a network and then reopened when we
  // connect back to that network.
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  const MediaSinkInternal cast_sink2 = CreateCastSink(2);
  const MediaSinkInternal cast_sink3 = CreateCastSink(3);

  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink2);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink3);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks_ethernet;
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink2.id())
          .value());
  cast_sinks_ethernet.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink3.id())
          .value());

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks_ethernet, SinkSource::kAccessCode))
      .Times(1);

  FastForwardUiAndIoTasks();

  mock_time_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &DiscoveryNetworkMonitor::GetNetworkId,
          base::Unretained(discovery_network_monitor_.get()),
          base::BindOnce(&AccessCodeCastSinkService::FetchAndAddStoredDevices,
                         access_code_cast_sink_service_->GetWeakPtr())));

  FastForwardUiAndIoTasks();

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  fake_network_info_ = fake_wifi_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  const MediaSinkInternal cast_sink4 = CreateCastSink(4);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink4);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks_wifi;
  cast_sinks_wifi.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink4.id())
          .value());

  FastForwardUiAndIoTasks();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks_wifi, SinkSource::kAccessCode))
      .Times(1);

  mock_time_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &DiscoveryNetworkMonitor::GetNetworkId,
          base::Unretained(discovery_network_monitor_.get()),
          base::BindOnce(&AccessCodeCastSinkService::FetchAndAddStoredDevices,
                         access_code_cast_sink_service_->GetWeakPtr())));

  FastForwardUiAndIoTasks();

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks_ethernet, SinkSource::kAccessCode))
      .Times(1);

  fake_network_info_ = fake_ethernet_info_;
  ChangeConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AccessCodeCastSinkServiceTest,
       TestAddInvalidDevicesNoMediaSinkInternal) {
  // Test that to check that if a sink is not stored in each pref, it will be
  // removed from the pref service and no call to open channels is made.
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks;
  cast_sinks.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());

  FastForwardUiAndIoTasks();

  // Remove the cast sink from the devices dict -- now the cast sink is
  // incompletely stored since it only exists in 2/3 of the prefs.
  access_code_cast_sink_service_->pref_updater_->RemoveSinkIdFromDevicesDict(
      cast_sink1.id());

  mock_time_task_runner()->FastForwardUntilNoTasksRemain();

  base::Value::List sink_ids;
  sink_ids.Append(cast_sink1.id());

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks, SinkSource::kAccessCode))
      .Times(0);

  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDiscoveredNetworksDict()
          ->GetDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAdditionTimeDict()
          ->GetDict()
          .empty());
  EXPECT_TRUE(access_code_cast_sink_service_->pref_updater_->GetDevicesDict()
                  ->GetDict()
                  .empty());

  // Expect that the sink id is removed from all instance in the pref service.
  access_code_cast_sink_service_->AddStoredDevicesToMediaRouter(sink_ids);
  mock_time_task_runner()->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDiscoveredNetworksDict()
          ->GetDict()
          .empty());
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAdditionTimeDict()
          ->GetDict()
          .empty());
  EXPECT_TRUE(access_code_cast_sink_service_->pref_updater_->GetDevicesDict()
                  ->GetDict()
                  .empty());

  FastForwardUiAndIoTasks();
}

TEST_F(AccessCodeCastSinkServiceTest, TestFetchAndAddStoredDevicesNoNetwork) {
  // Test that to check that if a sink is not stored on the network, it won't
  // attempted to be added. In this case no sink_ids should be removed.
  FastForwardUiAndIoTasks();

  const MediaSinkInternal cast_sink1 = CreateCastSink(1);
  access_code_cast_sink_service_->StoreSinkInPrefs(&cast_sink1);

  FastForwardUiAndIoTasks();

  std::vector<MediaSinkInternal> cast_sinks;
  cast_sinks.push_back(
      access_code_cast_sink_service_->ValidateDeviceFromSinkId(cast_sink1.id())
          .value());

  FastForwardUiAndIoTasks();

  // Remove the cast sink from the networks dict -- now the cast sink is
  // incompletely stored since it only exists in 2/3 of the prefs.
  access_code_cast_sink_service_->pref_updater_
      ->RemoveSinkIdFromDiscoveredNetworksDict(cast_sink1.id());

  FastForwardUiAndIoTasks();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(),
              OpenChannels(cast_sinks, SinkSource::kAccessCode))
      .Times(0);
  EXPECT_TRUE(
      access_code_cast_sink_service_->pref_updater_->GetDiscoveredNetworksDict()
          ->GetDict()
          .empty());
  EXPECT_FALSE(
      access_code_cast_sink_service_->pref_updater_->GetDeviceAdditionTimeDict()
          ->GetDict()
          .empty());
  EXPECT_FALSE(access_code_cast_sink_service_->pref_updater_->GetDevicesDict()
                   ->GetDict()
                   .empty());

  mock_time_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &DiscoveryNetworkMonitor::GetNetworkId,
          base::Unretained(discovery_network_monitor_.get()),
          base::BindOnce(&AccessCodeCastSinkService::FetchAndAddStoredDevices,
                         access_code_cast_sink_service_->GetWeakPtr())));

  FastForwardUiAndIoTasks();
}

}  // namespace media_router
