/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/goog_cc_factory.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

using testing::Field;
using testing::Matcher;
using testing::NiceMock;
using testing::Property;
using testing::_;

namespace webrtc {
namespace test {
namespace {

const uint32_t kInitialBitrateKbps = 60;
const DataRate kInitialBitrate = DataRate::kbps(kInitialBitrateKbps);
const float kDefaultPacingRate = 2.5f;

void UpdatesTargetRateBasedOnLinkCapacity(std::string test_name = "") {
  Scenario s("googcc_unit/target_capacity" + test_name, false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(1500);
  config.transport.rates.start_rate = DataRate::kbps(300);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(500);
    c->simulation.delay = TimeDelta::ms(100);
    c->simulation.loss_rate = 0.0;
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  StatesPrinter* truth = s.CreatePrinter(
      "send.truth.txt", TimeDelta::PlusInfinity(), {send_net->ConfigPrinter()});
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(25));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate_kbps(), 450, 100);

  send_net->UpdateConfig([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(800);
    c->simulation.delay = TimeDelta::ms(100);
  });

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(20));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate_kbps(), 750, 150);

  send_net->UpdateConfig([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(100);
    c->simulation.delay = TimeDelta::ms(200);
  });
  ret_net->UpdateConfig(
      [](NetworkNodeConfig* c) { c->simulation.delay = TimeDelta::ms(200); });

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(30));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate_kbps(), 90, 20);
}
}  // namespace

class GoogCcNetworkControllerTest : public ::testing::Test {
 protected:
  GoogCcNetworkControllerTest()
      : current_time_(Timestamp::ms(123456)), factory_(&event_log_) {}
  ~GoogCcNetworkControllerTest() override {}

  void SetUp() override {
    controller_ = factory_.Create(InitialConfig());
    NetworkControlUpdate update =
        controller_->OnProcessInterval(DefaultInterval());
    EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate);
    EXPECT_EQ(update.pacer_config->data_rate(),
              kInitialBitrate * kDefaultPacingRate);

    EXPECT_EQ(update.probe_cluster_configs[0].target_data_rate,
              kInitialBitrate * 3);
    EXPECT_EQ(update.probe_cluster_configs[1].target_data_rate,
              kInitialBitrate * 5);
  }
  // Custom setup - use an observer that tracks the target bitrate, without
  // prescribing on which iterations it must change (like a mock would).
  void TargetBitrateTrackingSetup() {
    controller_ = factory_.Create(InitialConfig());
    controller_->OnProcessInterval(DefaultInterval());
  }

  NetworkControllerConfig InitialConfig(
      int starting_bandwidth_kbps = kInitialBitrateKbps,
      int min_data_rate_kbps = 0,
      int max_data_rate_kbps = 5 * kInitialBitrateKbps) {
    NetworkControllerConfig config;
    config.constraints.at_time = current_time_;
    config.constraints.min_data_rate = DataRate::kbps(min_data_rate_kbps);
    config.constraints.max_data_rate = DataRate::kbps(max_data_rate_kbps);
    config.constraints.starting_rate = DataRate::kbps(starting_bandwidth_kbps);
    return config;
  }
  ProcessInterval DefaultInterval() {
    ProcessInterval interval;
    interval.at_time = current_time_;
    return interval;
  }
  RemoteBitrateReport CreateBitrateReport(DataRate rate) {
    RemoteBitrateReport report;
    report.receive_time = current_time_;
    report.bandwidth = rate;
    return report;
  }
  PacketResult CreateResult(int64_t arrival_time_ms,
                            int64_t send_time_ms,
                            size_t payload_size,
                            PacedPacketInfo pacing_info) {
    PacketResult packet_result;
    packet_result.sent_packet = SentPacket();
    packet_result.sent_packet.send_time = Timestamp::ms(send_time_ms);
    packet_result.sent_packet.size = DataSize::bytes(payload_size);
    packet_result.sent_packet.pacing_info = pacing_info;
    packet_result.receive_time = Timestamp::ms(arrival_time_ms);
    return packet_result;
  }

  NetworkRouteChange CreateRouteChange(
      absl::optional<DataRate> start_rate = absl::nullopt,
      absl::optional<DataRate> min_rate = absl::nullopt,
      absl::optional<DataRate> max_rate = absl::nullopt) {
    NetworkRouteChange route_change;
    route_change.at_time = current_time_;
    route_change.constraints.at_time = current_time_;
    route_change.constraints.min_data_rate = min_rate;
    route_change.constraints.max_data_rate = max_rate;
    route_change.constraints.starting_rate = start_rate;
    return route_change;
  }

  void AdvanceTimeMilliseconds(int timedelta_ms) {
    current_time_ += TimeDelta::ms(timedelta_ms);
  }

  void OnUpdate(NetworkControlUpdate update) {
    if (update.target_rate)
      target_bitrate_ = update.target_rate->target_rate;
  }

  void PacketTransmissionAndFeedbackBlock(int64_t runtime_ms, int64_t delay) {
    int64_t delay_buildup = 0;
    int64_t start_time_ms = current_time_.ms();
    while (current_time_.ms() - start_time_ms < runtime_ms) {
      constexpr size_t kPayloadSize = 1000;
      PacketResult packet =
          CreateResult(current_time_.ms() + delay_buildup, current_time_.ms(),
                       kPayloadSize, PacedPacketInfo());
      delay_buildup += delay;
      controller_->OnSentPacket(packet.sent_packet);
      TransportPacketsFeedback feedback;
      feedback.feedback_time = packet.receive_time;
      feedback.packet_feedbacks.push_back(packet);
      OnUpdate(controller_->OnTransportPacketsFeedback(feedback));
      AdvanceTimeMilliseconds(50);
      OnUpdate(controller_->OnProcessInterval(DefaultInterval()));
    }
  }
  Timestamp current_time_;
  absl::optional<DataRate> target_bitrate_;
  NiceMock<MockRtcEventLog> event_log_;
  GoogCcNetworkControllerFactory factory_;
  std::unique_ptr<NetworkControllerInterface> controller_;
};

TEST_F(GoogCcNetworkControllerTest, ReactsToChangedNetworkConditions) {
  // Test no change.
  AdvanceTimeMilliseconds(25);
  controller_->OnProcessInterval(DefaultInterval());

  NetworkControlUpdate update;
  controller_->OnRemoteBitrateReport(CreateBitrateReport(kInitialBitrate * 2));
  AdvanceTimeMilliseconds(25);
  update = controller_->OnProcessInterval(DefaultInterval());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate * 2);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * 2 * kDefaultPacingRate);

  controller_->OnRemoteBitrateReport(CreateBitrateReport(kInitialBitrate));
  AdvanceTimeMilliseconds(25);
  update = controller_->OnProcessInterval(DefaultInterval());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * kDefaultPacingRate);
}

// Test congestion window pushback on network delay happens.
TEST_F(GoogCcNetworkControllerTest, CongestionWindowPushbackOnNetworkDelay) {
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindowPushback/Enabled/WebRTC-CwndExperiment/"
      "Enabled-800/");
  Scenario s("googcc_unit/cwnd_on_delay", false);
  auto send_net = s.CreateSimulationNode([=](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(1000);
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::kbps(300);
  config.transport.rates.max_rate = DataRate::kbps(2000);
  config.transport.rates.min_rate = DataRate::kbps(10);
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});

  s.RunFor(TimeDelta::seconds(10));
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(10));
  s.RunFor(TimeDelta::seconds(3));

  // After 3 seconds without feedback from any sent packets, we expect that the
  // target rate is reduced to the minimum pushback threshold
  // kDefaultMinPushbackTargetBitrateBps, which is defined as 30 kbps in
  // congestion_window_pushback_controller.
  EXPECT_LT(client->target_rate_kbps(), 40);
}

TEST_F(GoogCcNetworkControllerTest, OnNetworkRouteChanged) {
  NetworkControlUpdate update;
  DataRate new_bitrate = DataRate::bps(200000);
  update = controller_->OnNetworkRouteChange(CreateRouteChange(new_bitrate));
  EXPECT_EQ(update.target_rate->target_rate, new_bitrate);
  EXPECT_EQ(update.pacer_config->data_rate(), new_bitrate * kDefaultPacingRate);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate.
  const DataRate kDefaultMinBitrate = DataRate::kbps(5);
  update = controller_->OnNetworkRouteChange(CreateRouteChange());
  EXPECT_EQ(update.target_rate->target_rate, kDefaultMinBitrate);
  EXPECT_NEAR(update.pacer_config->data_rate().bps<double>(),
              kDefaultMinBitrate.bps<double>() * kDefaultPacingRate, 10);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);
}

TEST_F(GoogCcNetworkControllerTest, ProbeOnRouteChange) {
  NetworkControlUpdate update;
  update = controller_->OnNetworkRouteChange(CreateRouteChange(
      2 * kInitialBitrate, DataRate::Zero(), 20 * kInitialBitrate));

  EXPECT_TRUE(update.pacer_config.has_value());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate * 2);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);
  EXPECT_EQ(update.probe_cluster_configs[0].target_data_rate,
            kInitialBitrate * 6);
  EXPECT_EQ(update.probe_cluster_configs[1].target_data_rate,
            kInitialBitrate * 12);

  update = controller_->OnProcessInterval(DefaultInterval());
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST_F(GoogCcNetworkControllerTest, UpdatesDelayBasedEstimate) {
  TargetBitrateTrackingSetup();
  const int64_t kRunTimeMs = 6000;

  // The test must run and insert packets/feedback long enough that the
  // BWE computes a valid estimate. This is first done in an environment which
  // simulates no bandwidth limitation, and therefore not built-up delay.
  PacketTransmissionAndFeedbackBlock(kRunTimeMs, 0);
  ASSERT_TRUE(target_bitrate_.has_value());

  // Repeat, but this time with a building delay, and make sure that the
  // estimation is adjusted downwards.
  DataRate bitrate_before_delay = *target_bitrate_;
  PacketTransmissionAndFeedbackBlock(kRunTimeMs, 50);
  EXPECT_LT(*target_bitrate_, bitrate_before_delay);
}

TEST_F(GoogCcNetworkControllerTest,
       PaddingRateLimitedByCongestionWindowInTrial) {
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindowPushback/Enabled/WebRTC-CwndExperiment/"
      "Enabled-200/");

  Scenario s("googcc_unit/padding_limited", false);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([=](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(1000);
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCc;
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::kbps(1000);
  config.transport.rates.max_rate = DataRate::kbps(2000);
  config.transport.rates.max_padding_rate = config.transport.rates.max_rate;
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
  // Run for a few seconds to allow the controller to stabilize.
  s.RunFor(TimeDelta::seconds(10));

  // Check that padding rate matches target rate.
  EXPECT_NEAR(client->padding_rate().kbps(), client->target_rate_kbps(), 1);

  // Check this is also the case when congestion window pushback kicks in.
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(1));
  EXPECT_NEAR(client->padding_rate().kbps(), client->target_rate_kbps(), 1);
}

TEST_F(GoogCcNetworkControllerTest, LimitsToFloorIfRttIsHighInTrial) {
  // The field trial limits maximum RTT to 2 seconds, higher RTT means that the
  // controller backs off until it reaches the minimum configured bitrate. This
  // allows the RTT to recover faster than the regular control mechanism would
  // achieve.
  const DataRate kBandwidthFloor = DataRate::kbps(50);
  ScopedFieldTrials trial("WebRTC-Bwe-MaxRttLimit/limit:2s,floor:" +
                          std::to_string(kBandwidthFloor.kbps()) + "kbps/");
  // In the test case, we limit the capacity and add a cross traffic packet
  // burst that blocks media from being sent. This causes the RTT to quickly
  // increase above the threshold in the trial.
  const DataRate kLinkCapacity = DataRate::kbps(100);
  const TimeDelta kBufferBloatDuration = TimeDelta::seconds(10);
  Scenario s("googcc_unit/limit_trial", false);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([=](NetworkNodeConfig* c) {
    c->simulation.bandwidth = kLinkCapacity;
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCc;
  config.transport.rates.start_rate = kLinkCapacity;
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
  // Run for a few seconds to allow the controller to stabilize.
  s.RunFor(TimeDelta::seconds(10));
  const DataSize kBloatPacketSize = DataSize::bytes(1000);
  const int kBloatPacketCount =
      static_cast<int>(kBufferBloatDuration * kLinkCapacity / kBloatPacketSize);
  // This will cause the RTT to be large for a while.
  s.TriggerPacketBurst({send_net}, kBloatPacketCount, kBloatPacketSize.bytes());
  // Wait to allow the high RTT to be detected and acted upon.
  s.RunFor(TimeDelta::seconds(4));
  // By now the target rate should have dropped to the minimum configured rate.
  EXPECT_NEAR(client->target_rate_kbps(), kBandwidthFloor.kbps(), 1);
}

TEST_F(GoogCcNetworkControllerTest, UpdatesTargetRateBasedOnLinkCapacity) {
  UpdatesTargetRateBasedOnLinkCapacity();
}

TEST_F(GoogCcNetworkControllerTest, DefaultEstimateVariesInSteadyState) {
  ScopedFieldTrials trial("WebRTC-Bwe-StableBandwidthEstimate/Disabled/");
  Scenario s("googcc_unit/no_stable_varies", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  NetworkNodeConfig net_conf;
  net_conf.simulation.bandwidth = DataRate::kbps(500);
  net_conf.simulation.delay = TimeDelta::ms(100);
  net_conf.update_frequency = TimeDelta::ms(5);
  auto send_net = s.CreateSimulationNode(net_conf);
  auto ret_net = s.CreateSimulationNode(net_conf);
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
  // Run for a while to allow the estimate to stabilize.
  s.RunFor(TimeDelta::seconds(20));
  DataRate min_estimate = DataRate::PlusInfinity();
  DataRate max_estimate = DataRate::MinusInfinity();
  // Measure variation in steady state.
  for (int i = 0; i < 20; ++i) {
    min_estimate = std::min(min_estimate, client->link_capacity());
    max_estimate = std::max(max_estimate, client->link_capacity());
    s.RunFor(TimeDelta::seconds(1));
  }
  // We should expect drops by at least 15% (default backoff.)
  EXPECT_LT(min_estimate / max_estimate, 0.85);
}

TEST_F(GoogCcNetworkControllerTest, StableEstimateDoesNotVaryInSteadyState) {
  ScopedFieldTrials trial("WebRTC-Bwe-StableBandwidthEstimate/Enabled/");
  Scenario s("googcc_unit/stable_is_stable", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  NetworkNodeConfig net_conf;
  net_conf.simulation.bandwidth = DataRate::kbps(500);
  net_conf.simulation.delay = TimeDelta::ms(100);
  net_conf.update_frequency = TimeDelta::ms(5);
  auto send_net = s.CreateSimulationNode(net_conf);
  auto ret_net = s.CreateSimulationNode(net_conf);
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
  // Run for a while to allow the estimate to stabilize.
  s.RunFor(TimeDelta::seconds(30));
  DataRate min_estimate = DataRate::PlusInfinity();
  DataRate max_estimate = DataRate::MinusInfinity();
  // Measure variation in steady state.
  for (int i = 0; i < 20; ++i) {
    min_estimate = std::min(min_estimate, client->link_capacity());
    max_estimate = std::max(max_estimate, client->link_capacity());
    s.RunFor(TimeDelta::seconds(1));
  }
  // We expect no variation under the trial in steady state.
  EXPECT_GT(min_estimate / max_estimate, 0.95);
}

TEST_F(GoogCcNetworkControllerTest,
       LossBasedControlUpdatesTargetRateBasedOnLinkCapacity) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  // TODO(srte): Should the behavior be unaffected at low loss rates?
  UpdatesTargetRateBasedOnLinkCapacity("_loss_based");
}

TEST_F(GoogCcNetworkControllerTest,
       LossBasedControlDoesModestBackoffToHighLoss) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  Scenario s("googcc_unit/high_loss_channel", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(1500);
  config.transport.rates.start_rate = DataRate::kbps(300);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(2000);
    c->simulation.delay = TimeDelta::ms(200);
    c->simulation.loss_rate = 0.1;
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(200);
    c->update_frequency = TimeDelta::ms(5);
  });
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});

  s.RunFor(TimeDelta::seconds(120));
  // Without LossBasedControl trial, bandwidth drops to ~10 kbps.
  EXPECT_GT(client->target_rate_kbps(), 100);
}

TEST_F(GoogCcNetworkControllerTest, LossBasedEstimatorCapsRateAtModerateLoss) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  Scenario s("googcc_unit/moderate_loss_channel", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(5000);
  config.transport.rates.start_rate = DataRate::kbps(300);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(5000);
    c->simulation.delay = TimeDelta::ms(100);
    c->simulation.loss_rate = 0.02;
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});

  s.RunFor(TimeDelta::seconds(60));
  // Without LossBasedControl trial, bitrate reaches above 4 mbps.
  // Using LossBasedControl the bitrate should not go above 3 mbps for a 2% loss
  // rate.
  EXPECT_LT(client->target_rate_kbps(), 3000);
}

}  // namespace test
}  // namespace webrtc
