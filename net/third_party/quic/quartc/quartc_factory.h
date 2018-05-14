// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_FACTORY_H_
#define NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_FACTORY_H_

#include "net/third_party/quic/core/quic_alarm_factory.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/quartc/quartc_factory_interface.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"

namespace net {

// Implements the QuartcFactoryInterface to create the instances of
// QuartcSessionInterface. Implements the QuicAlarmFactory to create alarms
// using the QuartcTaskRunner. Implements the QuicConnectionHelperInterface used
// by the QuicConnections. Only one QuartcFactory is expected to be created.
class QUIC_EXPORT_PRIVATE QuartcFactory : public QuartcFactoryInterface,
                                          public QuicConnectionHelperInterface {
 public:
  explicit QuartcFactory(const QuartcFactoryConfig& factory_config);
  ~QuartcFactory() override;

  // QuartcFactoryInterface overrides.
  std::unique_ptr<QuartcSessionInterface> CreateQuartcSession(
      const QuartcSessionConfig& quartc_session_config) override;

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

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_FACTORY_H_
