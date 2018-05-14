// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_factory.h"

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/quartc/quartc_session.h"

namespace net {

QuartcFactory::QuartcFactory(const QuartcFactoryConfig& factory_config)
    : alarm_factory_(factory_config.alarm_factory),
      clock_(factory_config.clock) {}

QuartcFactory::~QuartcFactory() {}

std::unique_ptr<QuartcSessionInterface> QuartcFactory::CreateQuartcSession(
    const QuartcSessionConfig& quartc_session_config) {
  DCHECK(quartc_session_config.packet_transport);
  SetQuicReloadableFlag(quic_is_write_blocked, true);

  Perspective perspective = quartc_session_config.is_server
                                ? Perspective::IS_SERVER
                                : Perspective::IS_CLIENT;

  // QuartcSession will eventually own both |writer| and |quic_connection|.
  auto writer =
      QuicMakeUnique<QuartcPacketWriter>(quartc_session_config.packet_transport,
                                         quartc_session_config.max_packet_size);
  std::unique_ptr<QuicConnection> quic_connection =
      CreateQuicConnection(perspective, writer.get());

  QuicTagVector copt;
  copt.push_back(kNSTP);
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
  return QuicMakeUnique<QuartcSession>(
      std::move(quic_connection), quic_config,
      quartc_session_config.unique_remote_server_id, perspective,
      this /*QuicConnectionHelperInterface*/, clock_, std::move(writer));
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
      alarm_factory_ /*QuicAlarmFactory*/, packet_writer, /*owns_writer=*/false,
      perspective, CurrentSupportedVersions());
}

const QuicClock* QuartcFactory::GetClock() const {
  return clock_;
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

}  // namespace net
