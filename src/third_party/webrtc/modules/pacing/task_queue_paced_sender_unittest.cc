/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/task_queue_paced_sender.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "modules/pacing/packet_router.h"
#include "modules/utility/include/mock/mock_process_thread.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace webrtc {
namespace {
constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr size_t kDefaultPacketSize = 1234;

class MockPacketRouter : public PacketRouter {
 public:
  MOCK_METHOD2(SendPacket,
               void(std::unique_ptr<RtpPacketToSend> packet,
                    const PacedPacketInfo& cluster_info));
  MOCK_METHOD1(
      GeneratePadding,
      std::vector<std::unique_ptr<RtpPacketToSend>>(size_t target_size_bytes));
};

class TaskQueuePacedSenderForTest : public TaskQueuePacedSender {
 public:
  TaskQueuePacedSenderForTest(Clock* clock,
                              PacketRouter* packet_router,
                              RtcEventLog* event_log,
                              const WebRtcKeyValueConfig* field_trials,
                              TaskQueueFactory* task_queue_factory)
      : TaskQueuePacedSender(clock,
                             packet_router,
                             event_log,
                             field_trials,
                             task_queue_factory) {}

  void OnStatsUpdated(const Stats& stats) override {
    ++num_stats_updates_;
    TaskQueuePacedSender::OnStatsUpdated(stats);
  }

  size_t num_stats_updates_ = 0;
};

std::unique_ptr<RtpPacketToSend> BuildRtpPacket(RtpPacketMediaType type) {
  auto packet = std::make_unique<RtpPacketToSend>(nullptr);
  packet->set_packet_type(type);
  switch (type) {
    case RtpPacketMediaType::kAudio:
      packet->SetSsrc(kAudioSsrc);
      break;
    case RtpPacketMediaType::kVideo:
      packet->SetSsrc(kVideoSsrc);
      break;
    case RtpPacketMediaType::kRetransmission:
    case RtpPacketMediaType::kPadding:
      packet->SetSsrc(kVideoRtxSsrc);
      break;
    case RtpPacketMediaType::kForwardErrorCorrection:
      packet->SetSsrc(kFlexFecSsrc);
      break;
  }

  packet->SetPayloadSize(kDefaultPacketSize);
  return packet;
}

std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePackets(
    RtpPacketMediaType type,
    size_t num_packets) {
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  for (size_t i = 0; i < num_packets; ++i) {
    packets.push_back(BuildRtpPacket(type));
  }
  return packets;
}
}  // namespace

namespace test {

class TaskQueuePacedSenderTest : public ::testing::Test {
 public:
  TaskQueuePacedSenderTest()
      : time_controller_(Timestamp::Millis(1234)),
        pacer_(time_controller_.GetClock(),
               &packet_router_,
               /*event_log=*/nullptr,
               /*field_trials=*/nullptr,
               time_controller_.GetTaskQueueFactory()) {}

 protected:
  Timestamp CurrentTime() { return time_controller_.GetClock()->CurrentTime(); }

  GlobalSimulatedTimeController time_controller_;
  MockPacketRouter packet_router_;
  TaskQueuePacedSender pacer_;
};

TEST_F(TaskQueuePacedSenderTest, PacesPackets) {
  // Insert a number of packets, covering one second.
  static constexpr size_t kPacketsToSend = 42;
  pacer_.SetPacingRates(
      DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketsToSend),
      DataRate::Zero());
  pacer_.EnqueuePackets(
      GeneratePackets(RtpPacketMediaType::kVideo, kPacketsToSend));

  // Expect all of them to be sent.
  size_t packets_sent = 0;
  Timestamp end_time = Timestamp::PlusInfinity();
  EXPECT_CALL(packet_router_, SendPacket)
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& cluster_info) {
        ++packets_sent;
        if (packets_sent == kPacketsToSend) {
          end_time = time_controller_.GetClock()->CurrentTime();
        }
      });

  const Timestamp start_time = time_controller_.GetClock()->CurrentTime();

  // Packets should be sent over a period of close to 1s. Expect a little lower
  // than this since initial probing is a bit quicker.
  time_controller_.AdvanceTime(TimeDelta::Seconds(1));
  EXPECT_EQ(packets_sent, kPacketsToSend);
  ASSERT_TRUE(end_time.IsFinite());
  EXPECT_NEAR((end_time - start_time).ms<double>(), 1000.0, 50.0);
}

TEST_F(TaskQueuePacedSenderTest, ReschedulesProcessOnRateChange) {
  // Insert a number of packets to be sent 200ms apart.
  const size_t kPacketsPerSecond = 5;
  const DataRate kPacingRate =
      DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketsPerSecond);
  pacer_.SetPacingRates(kPacingRate, DataRate::Zero());

  // Send some initial packets to be rid of any probes.
  EXPECT_CALL(packet_router_, SendPacket).Times(kPacketsPerSecond);
  pacer_.EnqueuePackets(
      GeneratePackets(RtpPacketMediaType::kVideo, kPacketsPerSecond));
  time_controller_.AdvanceTime(TimeDelta::Seconds(1));

  // Insert three packets, and record send time of each of them.
  // After the second packet is sent, double the send rate so we can
  // check the third packets is sent after half the wait time.
  Timestamp first_packet_time = Timestamp::MinusInfinity();
  Timestamp second_packet_time = Timestamp::MinusInfinity();
  Timestamp third_packet_time = Timestamp::MinusInfinity();

  EXPECT_CALL(packet_router_, SendPacket)
      .Times(3)
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& cluster_info) {
        if (first_packet_time.IsInfinite()) {
          first_packet_time = CurrentTime();
        } else if (second_packet_time.IsInfinite()) {
          second_packet_time = CurrentTime();
          pacer_.SetPacingRates(2 * kPacingRate, DataRate::Zero());
        } else {
          third_packet_time = CurrentTime();
        }
      });

  pacer_.EnqueuePackets(GeneratePackets(RtpPacketMediaType::kVideo, 3));
  time_controller_.AdvanceTime(TimeDelta::Millis(500));
  ASSERT_TRUE(third_packet_time.IsFinite());
  EXPECT_NEAR((second_packet_time - first_packet_time).ms<double>(), 200.0,
              1.0);
  EXPECT_NEAR((third_packet_time - second_packet_time).ms<double>(), 100.0,
              1.0);
}

TEST_F(TaskQueuePacedSenderTest, SendsAudioImmediately) {
  const DataRate kPacingDataRate = DataRate::KilobitsPerSec(125);
  const DataSize kPacketSize = DataSize::Bytes(kDefaultPacketSize);
  const TimeDelta kPacketPacingTime = kPacketSize / kPacingDataRate;

  pacer_.SetPacingRates(kPacingDataRate, DataRate::Zero());

  // Add some initial video packets, only one should be sent.
  EXPECT_CALL(packet_router_, SendPacket);
  pacer_.EnqueuePackets(GeneratePackets(RtpPacketMediaType::kVideo, 10));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  ::testing::Mock::VerifyAndClearExpectations(&packet_router_);

  // Advance time, but still before next packet should be sent.
  time_controller_.AdvanceTime(kPacketPacingTime / 2);

  // Insert an audio packet, it should be sent immediately.
  EXPECT_CALL(packet_router_, SendPacket);
  pacer_.EnqueuePackets(GeneratePackets(RtpPacketMediaType::kAudio, 1));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  ::testing::Mock::VerifyAndClearExpectations(&packet_router_);
}

TEST(TaskQueuePacedSenderTestNew, RespectedMinTimeBetweenStatsUpdates) {
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(1234));
  MockPacketRouter packet_router;
  TaskQueuePacedSenderForTest pacer(time_controller.GetClock(), &packet_router,
                                    /*event_log=*/nullptr,
                                    /*field_trials=*/nullptr,
                                    time_controller.GetTaskQueueFactory());
  const DataRate kPacingDataRate = DataRate::KilobitsPerSec(300);
  pacer.SetPacingRates(kPacingDataRate, DataRate::Zero());

  const TimeDelta kMinTimeBetweenStatsUpdates = TimeDelta::Millis(1);

  // Nothing inserted, no stats updates yet.
  EXPECT_EQ(pacer.num_stats_updates_, 0u);

  // Insert one packet, stats should be updated.
  pacer.EnqueuePackets(GeneratePackets(RtpPacketMediaType::kVideo, 1));
  time_controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(pacer.num_stats_updates_, 1u);

  // Advance time half of the min stats update interval, and trigger a
  // refresh - stats should not be updated yet.
  time_controller.AdvanceTime(kMinTimeBetweenStatsUpdates / 2);
  pacer.EnqueuePackets({});
  time_controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(pacer.num_stats_updates_, 1u);

  // Advance time the next half, now stats update is triggered.
  time_controller.AdvanceTime(kMinTimeBetweenStatsUpdates / 2);
  pacer.EnqueuePackets({});
  time_controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(pacer.num_stats_updates_, 2u);
}

TEST(TaskQueuePacedSenderTestNew, ThrottlesStatsUpdates) {
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(1234));
  MockPacketRouter packet_router;
  TaskQueuePacedSenderForTest pacer(time_controller.GetClock(), &packet_router,
                                    /*event_log=*/nullptr,
                                    /*field_trials=*/nullptr,
                                    time_controller.GetTaskQueueFactory());

  // Set rates so one packet adds 10ms of buffer level.
  const DataSize kPacketSize = DataSize::Bytes(kDefaultPacketSize);
  const TimeDelta kPacketPacingTime = TimeDelta::Millis(10);
  const DataRate kPacingDataRate = kPacketSize / kPacketPacingTime;
  const TimeDelta kMinTimeBetweenStatsUpdates = TimeDelta::Millis(1);
  const TimeDelta kMaxTimeBetweenStatsUpdates = TimeDelta::Millis(33);

  // Nothing inserted, no stats updates yet.
  size_t num_expected_stats_updates = 0;
  EXPECT_EQ(pacer.num_stats_updates_, num_expected_stats_updates);
  pacer.SetPacingRates(kPacingDataRate, DataRate::Zero());
  time_controller.AdvanceTime(kMinTimeBetweenStatsUpdates);
  // Updating pacing rates refreshes stats.
  EXPECT_EQ(pacer.num_stats_updates_, ++num_expected_stats_updates);

  // Record time when we insert first packet, this triggers the scheduled
  // stats updating.
  Clock* const clock = time_controller.GetClock();
  const Timestamp start_time = clock->CurrentTime();

  while (clock->CurrentTime() - start_time <=
         kMaxTimeBetweenStatsUpdates - kPacketPacingTime) {
    // Enqueue packet, expect stats update.
    pacer.EnqueuePackets(GeneratePackets(RtpPacketMediaType::kVideo, 1));
    time_controller.AdvanceTime(TimeDelta::Zero());
    EXPECT_EQ(pacer.num_stats_updates_, ++num_expected_stats_updates);

    // Advance time to halfway through pacing time, expect another stats
    // update.
    time_controller.AdvanceTime(kPacketPacingTime / 2);
    pacer.EnqueuePackets({});
    time_controller.AdvanceTime(TimeDelta::Zero());
    EXPECT_EQ(pacer.num_stats_updates_, ++num_expected_stats_updates);

    // Advance time the rest of the way.
    time_controller.AdvanceTime(kPacketPacingTime / 2);
  }

  // At this point, the pace queue is drained so there is no more intersting
  // update to be made - but there is still as schduled task that should run
  // |kMaxTimeBetweenStatsUpdates| after the first update.
  time_controller.AdvanceTime(start_time + kMaxTimeBetweenStatsUpdates -
                              clock->CurrentTime());
  EXPECT_EQ(pacer.num_stats_updates_, ++num_expected_stats_updates);

  // Advance time a significant time - don't expect any more calls as stats
  // updating does not happen when queue is drained.
  time_controller.AdvanceTime(TimeDelta::Millis(400));
  EXPECT_EQ(pacer.num_stats_updates_, num_expected_stats_updates);
}

}  // namespace test
}  // namespace webrtc
