// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_packet_transport_adapter.h"

#include "net/test/gtest_util.h"
#include "third_party/webrtc/p2p/base/mock_ice_transport.h"

namespace blink {

namespace {
using testing::StrEq;
}  // namespace

class MockWriteObserver : public P2PQuicPacketTransport::WriteObserver {
 public:
  MOCK_METHOD0(OnCanWrite, void());
};

class MockReceiveDelegate : public P2PQuicPacketTransport::ReceiveDelegate {
 public:
  MOCK_METHOD2(OnPacketDataReceived, void(const char*, size_t));
};

class QuicPacketTransportAdapterTest : public testing::Test {
 public:
  QuicPacketTransportAdapterTest()
      : quic_packet_transport_adapter_(&mock_ice_transport_) {}

  ~QuicPacketTransportAdapterTest() override {
    quic_packet_transport_adapter_.SetReceiveDelegate(nullptr);
    quic_packet_transport_adapter_.SetWriteObserver(nullptr);
  }

  cricket::MockIceTransport* mock_ice_transport() {
    return &mock_ice_transport_;
  }

  QuicPacketTransportAdapter* quic_packet_transport_adapter() {
    return &quic_packet_transport_adapter_;
  }

 private:
  cricket::MockIceTransport mock_ice_transport_;
  QuicPacketTransportAdapter quic_packet_transport_adapter_;
};

// Tests that when the underlying ICE transport is ready to send data that
// the QuicPacketTransportAdapter will tell the WriteObserver.
TEST_F(QuicPacketTransportAdapterTest, IceTransportReadyTriggersCanWrite) {
  MockWriteObserver mock_write_observer;
  quic_packet_transport_adapter()->SetWriteObserver(&mock_write_observer);

  EXPECT_CALL(mock_write_observer, OnCanWrite());

  mock_ice_transport()->SignalReadyToSend(mock_ice_transport());
}

// Tests that writing a packet to the QuicPacketTransportAdapter will write
// the data to the underlying ICE transport.
TEST_F(QuicPacketTransportAdapterTest, WritePacketWritesToIceTransport) {
  std::string packet("hola");

  EXPECT_CALL(*mock_ice_transport(),
              SendPacket(StrEq(packet), packet.size(), _, _));

  P2PQuicPacketTransport::QuicPacket quic_packet;
  quic_packet.buffer = packet.c_str();
  quic_packet.buf_len = packet.size();
  quic_packet_transport_adapter()->WritePacket(quic_packet);
}

// Tests that when the underlying ICE transport receives data it it
// is passed appropriately to the ReceiveDelegate.
TEST_F(QuicPacketTransportAdapterTest, ReadPacketGivenToReceiveDelegate) {
  MockReceiveDelegate mock_receive_delegate;
  quic_packet_transport_adapter()->SetReceiveDelegate(&mock_receive_delegate);

  std::string packet("hola");
  EXPECT_CALL(mock_receive_delegate,
              OnPacketDataReceived(StrEq(packet), packet.size()));

  mock_ice_transport()->SignalReadPacket(mock_ice_transport(), packet.c_str(),
                                         packet.size(), 0, 0);
}

// Tests that the most recent packet that was received before the
// ReceiveDelegate is hooked up will be given to the ReceiveDelegate once it is
// set. This is used as an optimization in the case that a QUIC CHLO is received
// early.
TEST_F(QuicPacketTransportAdapterTest,
       MostRecentCachedPacketGivenToReceiveDelegate) {
  std::string packet("hola");
  std::string latest_packet("bonjour");

  mock_ice_transport()->SignalReadPacket(mock_ice_transport(), packet.c_str(),
                                         packet.size(), 0, 0);
  mock_ice_transport()->SignalReadPacket(
      mock_ice_transport(), latest_packet.c_str(), latest_packet.size(), 0, 0);

  // The receive delegate is set after packets have been received.
  MockReceiveDelegate mock_receive_delegate;
  EXPECT_CALL(mock_receive_delegate,
              OnPacketDataReceived(StrEq(latest_packet), latest_packet.size()));

  quic_packet_transport_adapter()->SetReceiveDelegate(&mock_receive_delegate);
}

// Tests that the cached packet is not reused once it has been handed off to
// a set ReceiveDelegate.
TEST_F(QuicPacketTransportAdapterTest, CachedPacketIsNotReused) {
  std::string packet("hola");
  mock_ice_transport()->SignalReadPacket(mock_ice_transport(), packet.c_str(),
                                         packet.size(), 0, 0);

  MockReceiveDelegate mock_receive_delegate;
  EXPECT_CALL(mock_receive_delegate, OnPacketDataReceived(_, _));
  quic_packet_transport_adapter()->SetReceiveDelegate(&mock_receive_delegate);

  MockReceiveDelegate latest_receive_delegate;
  EXPECT_CALL(latest_receive_delegate, OnPacketDataReceived(_, _)).Times(0);
  quic_packet_transport_adapter()->SetReceiveDelegate(&latest_receive_delegate);
}

}  // namespace blink
