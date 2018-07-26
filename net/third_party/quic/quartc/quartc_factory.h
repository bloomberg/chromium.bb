// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_FACTORY_H_
#define NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_FACTORY_H_

#include "net/third_party/quic/core/quic_alarm_factory.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"
#include "net/third_party/quic/quartc/quartc_session.h"

namespace quic {

// Options that control the BBR algorithm.
enum class QuartcBbrOptions {
  kSlowerStartup,    // Once a loss is encountered in STARTUP,
                     // switches startup to a 1.5x pacing gain.
  kFullyDrainQueue,  // Fully drains the queue once per cycle.
  kReduceProbeRtt,   // Probe RTT reduces CWND to 0.75 * BDP instead of 4
                     // packets.
  kSkipProbeRtt,     // Skip Probe RTT and extend the existing min_rtt if a
                     // recent min_rtt is within 12.5% of the current min_rtt.
  kSkipProbeRttAggressively,  //  Skip ProbeRTT and extend the existing min_rtt
                              //  as long as you've been app limited at least
                              //  once.
  kFillUpLinkDuringProbing,   // Sends probing retransmissions whenever we
                              // become application limited.
  kInitialWindow3,            // Use a 3-packet initial congestion window.
  kInitialWindow10,           // Use a 10-packet initial congestion window.
  kInitialWindow20,           // Use a 20-packet initial congestion window.
  kInitialWindow50,           // Use a 50-packet initial congestion window.
  kStartup1RTT,               // Stay in STARTUP for 1 RTT.
  kStartup2RTT,               // Stay in STARTUP for 2 RTTs.
};

// The configuration for creating a QuartcFactory.
struct QuartcFactoryConfig {
  // Factory for |QuicAlarm|s. Implemented by the Quartc user with different
  // mechanisms. For example in WebRTC, it is implemented with rtc::Thread.
  // Owned by the user, and needs to stay alive for as long as the QuartcFactory
  // exists.
  QuicAlarmFactory* alarm_factory = nullptr;
  // The clock used by |QuicAlarm|s. Implemented by the Quartc user. Owned by
  // the user, and needs to stay alive for as long as the QuartcFactory exists.
  QuicClock* clock = nullptr;
};

struct QuartcSessionConfig {
  // When using Quartc, there are two endpoints. The QuartcSession on one
  // endpoint must act as a server and the one on the other side must act as a
  // client.
  Perspective perspective = Perspective::IS_CLIENT;
  // This is only needed when is_server = false.  It must be unique
  // for each endpoint the local endpoint may communicate with. For example,
  // a WebRTC client could use the remote endpoint's crypto fingerprint
  QuicString unique_remote_server_id;
  // The way the QuicConnection will send and receive packets, like a virtual
  // UDP socket. For WebRTC, this will typically be an IceTransport.
  QuartcPacketTransport* packet_transport = nullptr;
  // The maximum size of the packet can be written with the packet writer.
  // 1200 bytes by default.
  QuicPacketLength max_packet_size = 1200;
  // Options to control the BBR algorithm. In case the congestion control is
  // set to anything but BBR, these options are ignored.
  std::vector<QuartcBbrOptions> bbr_options;
  // Timeouts for the crypto handshake. Set them to higher values to
  // prevent closing the session before it started on a slow network.
  // Zero entries are ignored and QUIC defaults are used in that case.
  QuicTime::Delta max_idle_time_before_crypto_handshake =
      QuicTime::Delta::Zero();
  QuicTime::Delta max_time_before_crypto_handshake = QuicTime::Delta::Zero();
  QuicTime::Delta idle_network_timeout = QuicTime::Delta::Zero();
};

// Factory that creates instances of QuartcSession.  Implements the
// QuicConnectionHelperInterface used by the QuicConnections. Only one
// QuartcFactory is expected to be created.
class QUIC_EXPORT_PRIVATE QuartcFactory : public QuicConnectionHelperInterface {
 public:
  explicit QuartcFactory(const QuartcFactoryConfig& factory_config);
  ~QuartcFactory() override;

  // Creates a new QuartcSession using the given configuration.
  std::unique_ptr<QuartcSession> CreateQuartcSession(
      const QuartcSessionConfig& quartc_session_config);

  // QuicConnectionHelperInterface overrides.
  const QuicClock* GetClock() const override;

  QuicRandom* GetRandomGenerator() override;

  QuicBufferAllocator* GetStreamSendBufferAllocator() override;

 private:
  std::unique_ptr<QuicConnection> CreateQuicConnection(
      Perspective perspective,
      QuartcPacketWriter* packet_writer);

  // Used to implement QuicAlarmFactory.  Owned by the user and must outlive
  // QuartcFactory.
  QuicAlarmFactory* alarm_factory_;
  // Used to implement the QuicConnectionHelperInterface.  Owned by the user and
  // must outlive QuartcFactory.
  QuicClock* clock_;
  SimpleBufferAllocator buffer_allocator_;
};

// Creates a new instance of QuartcFactory.
std::unique_ptr<QuartcFactory> CreateQuartcFactory(
    const QuartcFactoryConfig& factory_config);

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_FACTORY_H_
