// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_QUARTC_SIMULATED_PACKET_TRANSPORT_H_
#define NET_THIRD_PARTY_QUIC_QUARTC_SIMULATED_PACKET_TRANSPORT_H_

#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"
#include "net/third_party/quic/test_tools/simulator/port.h"
#include "net/third_party/quic/test_tools/simulator/queue.h"

namespace quic {
namespace simulator {

// Simulated implementation of QuartcPacketTransport.  This packet transport
// implementation connects Quartc to a QUIC simulator's network fabric.
// Assumes that its caller and delegate run on the same thread as the network
// simulation and therefore require no additional synchronization.
class SimulatedQuartcPacketTransport : public Endpoint,
                                       public QuartcPacketTransport,
                                       public UnconstrainedPortInterface,
                                       public Queue::ListenerInterface {
 public:
  SimulatedQuartcPacketTransport(Simulator* simulator,
                                 const QuicString& name,
                                 const QuicString& peer_name,
                                 QuicByteCount queue_capacity);

  // QuartcPacketTransport methods.
  int Write(const char* buffer,
            size_t buf_len,
            const PacketInfo& info) override;
  void SetDelegate(Delegate* delegate) override;

  // Simulation methods below.  These are implementation details.

  // Endpoint methods.  Called by the simulation to connect the transport.
  UnconstrainedPortInterface* GetRxPort() override;
  void SetTxPort(ConstrainedPortInterface* port) override;

  // UnconstrainedPortInterface method.  Called by the simulation to deliver a
  // packet to this transport.
  void AcceptPacket(std::unique_ptr<Packet> packet) override;

  // Queue::ListenerInterface method.  Called when the internal egress queue has
  // dispatched a packet and may have room for more.
  void OnPacketDequeued() override;

  // Actor method.  The transport schedules this to run when the delegate is set
  // in order to trigger an initial call to |Delegate::OnTransportCanWrite()|.
  // (The Quartc packet writer starts in a blocked state and needs an initial
  // callback to unblock it.)
  void Act() override;

  // Last packet number sent over this simulated transport.
  // TODO(b/112561077):  Reorganize tests so that this method can be deleted.
  // This exists purely for use by quartc_session_test.cc, to test that the
  // packet writer passes packet numbers to the transport.
  QuicPacketNumber last_packet_number() { return last_packet_number_; }

 private:
  QuicString peer_name_;
  Delegate* delegate_ = nullptr;
  Queue egress_queue_;
  QuicPacketNumber last_packet_number_;
};

}  // namespace simulator
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_QUARTC_SIMULATED_PACKET_TRANSPORT_H_
