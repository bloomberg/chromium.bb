// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_factory.h"

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/quartc/quartc_session.h"

namespace quic {

namespace {

// Implements the QuicAlarm with QuartcTaskRunnerInterface for the Quartc
//  users other than Chromium. For example, WebRTC will create QuartcAlarm with
// a QuartcTaskRunner implemented by WebRTC.
class QuartcAlarm : public QuicAlarm, public QuartcTaskRunnerInterface::Task {
 public:
  QuartcAlarm(const QuicClock* clock,
              QuartcTaskRunnerInterface* task_runner,
              QuicArenaScopedPtr<QuicAlarm::Delegate> delegate)
      : QuicAlarm(std::move(delegate)),
        clock_(clock),
        task_runner_(task_runner) {}

  ~QuartcAlarm() override {
    // Cancel the scheduled task before getting deleted.
    CancelImpl();
  }

  // QuicAlarm overrides.
  void SetImpl() override {
    DCHECK(deadline().IsInitialized());
    // Cancel it if already set.
    CancelImpl();

    int64_t delay_ms = (deadline() - (clock_->Now())).ToMilliseconds();
    if (delay_ms < 0) {
      delay_ms = 0;
    }

    DCHECK(task_runner_);
    DCHECK(!scheduled_task_);
    scheduled_task_ = task_runner_->Schedule(this, delay_ms);
  }

  void CancelImpl() override {
    if (scheduled_task_) {
      scheduled_task_->Cancel();
      scheduled_task_.reset();
    }
  }

  // QuartcTaskRunner::Task overrides.
  void Run() override {
    // The alarm may have been cancelled.
    if (!deadline().IsInitialized()) {
      return;
    }

    // The alarm may have been re-set to a later time.
    if (clock_->Now() < deadline()) {
      SetImpl();
      return;
    }

    Fire();
  }

 private:
  // Not owned by QuartcAlarm. Owned by the QuartcFactory.
  const QuicClock* clock_;
  // Not owned by QuartcAlarm. Owned by the QuartcFactory.
  QuartcTaskRunnerInterface* task_runner_;
  // Owned by QuartcAlarm.
  std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask> scheduled_task_;
};

// Adapts QuartcClockInterface (provided by the user) to QuicClock
// (expected by QUIC).
class QuartcClock : public QuicClock {
 public:
  explicit QuartcClock(QuartcClockInterface* clock) : clock_(clock) {}
  QuicTime ApproximateNow() const override { return Now(); }
  QuicTime Now() const override {
    return QuicTime::Zero() +
           QuicTime::Delta::FromMicroseconds(clock_->NowMicroseconds());
  }
  QuicWallTime WallNow() const override {
    return QuicWallTime::FromUNIXMicroseconds(clock_->NowMicroseconds());
  }

 private:
  QuartcClockInterface* clock_;
};

}  // namespace

QuartcFactory::QuartcFactory(const QuartcFactoryConfig& factory_config)
    : task_runner_(factory_config.task_runner),
      clock_(new QuartcClock(factory_config.clock)) {}

QuartcFactory::~QuartcFactory() {}

std::unique_ptr<QuartcSessionInterface> QuartcFactory::CreateQuartcSession(
    const QuartcSessionConfig& quartc_session_config) {
  DCHECK(quartc_session_config.packet_transport);

  Perspective perspective = quartc_session_config.is_server
                                ? Perspective::IS_SERVER
                                : Perspective::IS_CLIENT;

  // QuartcSession will eventually own both |writer| and |quic_connection|.
  auto writer =
      QuicMakeUnique<QuartcPacketWriter>(quartc_session_config.packet_transport,
                                         quartc_session_config.max_packet_size);

  // This setting fixes an issue with excessive ACKs being sent for reordered
  // packets, and instead bundles those ACKs better together. Combined with
  // ACK_DECIMATION_WITH_REORDERING we reduce the number of excessive ACKs
  // significantly. (Each one of the two features reduces ACKs by ~20%, but
  // together they reduce number of standalone acks by 25-30%).
  // This flag must be set before quic connection is created.
  SetQuicReloadableFlag(quic_deprecate_scoped_scheduler2, true);

  // ACK less aggressively when reordered packets are present.
  // Must be set before the connection is created.
  SetQuicReloadableFlag(quic_ack_reordered_packets, true);

  std::unique_ptr<QuicConnection> quic_connection =
      CreateQuicConnection(perspective, writer.get());

  QuicTagVector copt;
  copt.push_back(kNSTP);

  // Enable ACK_DECIMATION_WITH_REORDERING. It requires ack_decimation to be
  // false.
  SetQuicReloadableFlag(quic_enable_ack_decimation, false);
  copt.push_back(kAKD2);

  if (quartc_session_config.congestion_control ==
      QuartcCongestionControl::kBBR) {
    copt.push_back(kTBBR);

    // Note: These settings have no effect for Exoblaze builds since
    // SetQuicReloadableFlag() gets stubbed out.
    SetQuicReloadableFlag(quic_bbr_less_probe_rtt, true);
    SetQuicReloadableFlag(quic_unified_iw_options, true);
    for (const auto option : quartc_session_config.bbr_options) {
      switch (option) {
        case (QuartcBbrOptions::kSlowerStartup):
          copt.push_back(kBBRS);
          break;
        case (QuartcBbrOptions::kFullyDrainQueue):
          copt.push_back(kBBR3);
          break;
        case (QuartcBbrOptions::kReduceProbeRtt):
          copt.push_back(kBBR6);
          break;
        case (QuartcBbrOptions::kSkipProbeRtt):
          copt.push_back(kBBR7);
          break;
        case (QuartcBbrOptions::kSkipProbeRttAggressively):
          copt.push_back(kBBR8);
          break;
        case (QuartcBbrOptions::kFillUpLinkDuringProbing):
          quic_connection->set_fill_up_link_during_probing(true);
          break;
        case (QuartcBbrOptions::kInitialWindow3):
          copt.push_back(kIW03);
          break;
        case (QuartcBbrOptions::kInitialWindow10):
          copt.push_back(kIW10);
          break;
        case (QuartcBbrOptions::kInitialWindow20):
          copt.push_back(kIW20);
          break;
        case (QuartcBbrOptions::kInitialWindow50):
          copt.push_back(kIW50);
          break;
        case (QuartcBbrOptions::kStartup1RTT):
          copt.push_back(k1RTT);
          break;
        case (QuartcBbrOptions::kStartup2RTT):
          copt.push_back(k2RTT);
          break;
      }
    }
  }
  QuicConfig quic_config;

  // Use the limits for the session & stream flow control. The default 16KB
  // limit leads to significantly undersending (not reaching BWE on the outgoing
  // bitrate) due to blocked frames, and it leads to high latency (and one-way
  // delay). Setting it to its limits is not going to cause issues (our streams
  // are small generally, and if we were to buffer 24MB it wouldn't be the end
  // of the world). We can consider setting different limits in future (e.g. 1MB
  // stream, 1.5MB session). It's worth noting that on 1mbps bitrate, limit of
  // 24MB can capture approx 4 minutes of the call, and the default increase in
  // size of the window (half of the window size) is approximately 2 minutes of
  // the call.
  quic_config.SetInitialSessionFlowControlWindowToSend(
      kSessionReceiveWindowLimit);
  quic_config.SetInitialStreamFlowControlWindowToSend(
      kStreamReceiveWindowLimit);
  quic_config.SetConnectionOptionsToSend(copt);
  quic_config.SetClientConnectionOptions(copt);
  if (quartc_session_config.max_time_before_crypto_handshake_secs > 0) {
    quic_config.set_max_time_before_crypto_handshake(
        QuicTime::Delta::FromSeconds(
            quartc_session_config.max_time_before_crypto_handshake_secs));
  }
  if (quartc_session_config.max_idle_time_before_crypto_handshake_secs > 0) {
    quic_config.set_max_idle_time_before_crypto_handshake(
        QuicTime::Delta::FromSeconds(
            quartc_session_config.max_idle_time_before_crypto_handshake_secs));
  }
  if (quartc_session_config.idle_network_timeout > QuicTime::Delta::Zero()) {
    quic_config.SetIdleNetworkTimeout(
        quartc_session_config.idle_network_timeout,
        quartc_session_config.idle_network_timeout);
  }

  // The ICE transport provides a unique 5-tuple for each connection. Save
  // overhead by omitting the connection id.
  quic_config.SetBytesForConnectionIdToSend(0);
  return QuicMakeUnique<QuartcSession>(
      std::move(quic_connection), quic_config,
      quartc_session_config.unique_remote_server_id, perspective,
      this /*QuicConnectionHelperInterface*/, clock_.get(), std::move(writer));
}

std::unique_ptr<QuicConnection> QuartcFactory::CreateQuicConnection(
    Perspective perspective,
    QuartcPacketWriter* packet_writer) {
  // dummy_id and dummy_address are used because Quartc network layer will not
  // use these two.
  QuicConnectionId dummy_id = 0;
  QuicSocketAddress dummy_address(QuicIpAddress::Any4(), 0 /*Port*/);
  return QuicMakeUnique<QuicConnection>(
      dummy_id, dummy_address, this, /*QuicConnectionHelperInterface*/
      this /*QuicAlarmFactory*/, packet_writer, /*owns_writer=*/false,
      perspective, CurrentSupportedVersions());
}

QuicAlarm* QuartcFactory::CreateAlarm(QuicAlarm::Delegate* delegate) {
  return new QuartcAlarm(GetClock(), task_runner_,
                         QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate));
}

QuicArenaScopedPtr<QuicAlarm> QuartcFactory::CreateAlarm(
    QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
    QuicConnectionArena* arena) {
  if (arena != nullptr) {
    return arena->New<QuartcAlarm>(GetClock(), task_runner_,
                                   std::move(delegate));
  }
  return QuicArenaScopedPtr<QuicAlarm>(
      new QuartcAlarm(GetClock(), task_runner_, std::move(delegate)));
}

const QuicClock* QuartcFactory::GetClock() const {
  return clock_.get();
}

QuicRandom* QuartcFactory::GetRandomGenerator() {
  return QuicRandom::GetInstance();
}

QuicBufferAllocator* QuartcFactory::GetStreamSendBufferAllocator() {
  return &buffer_allocator_;
}

std::unique_ptr<QuartcFactoryInterface> CreateQuartcFactory(
    const QuartcFactoryConfig& factory_config) {
  return std::unique_ptr<QuartcFactoryInterface>(
      new QuartcFactory(factory_config));
}

}  // namespace quic
