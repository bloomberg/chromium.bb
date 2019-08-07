/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media and datagram transports.

#ifndef API_DATAGRAM_TRANSPORT_INTERFACE_H_
#define API_DATAGRAM_TRANSPORT_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/congestion_control_interface.h"
#include "api/media_transport_interface.h"
#include "api/rtc_error.h"
#include "api/units/data_rate.h"
#include "api/units/timestamp.h"

namespace rtc {
class PacketTransportInternal;
}  // namespace rtc

namespace webrtc {

typedef int64_t DatagramId;

struct DatagramAck {
  // |datagram_id| is same as passed in
  // DatagramTransportInterface::SendDatagram.
  DatagramId datagram_id;

  // The timestamp at which the remote peer received the identified datagram,
  // according to that peer's clock.
  Timestamp receive_timestamp;
};

// All sink methods are called on network thread.
class DatagramSinkInterface {
 public:
  virtual ~DatagramSinkInterface() {}

  // Called when new packet is received.
  virtual void OnDatagramReceived(rtc::ArrayView<const uint8_t> data) = 0;

  // Called when datagram is actually sent (datragram can be delayed due
  // to congestion control or fusing). |datagram_id| is same as passed in
  // DatagramTransportInterface::SendDatagram.
  virtual void OnDatagramSent(DatagramId datagram_id) = 0;

  // Called when datagram is ACKed.
  // TODO(sukhanov): Make pure virtual.
  virtual void OnDatagramAcked(const DatagramAck& datagram_ack) {}
};

// Datagram transport allows to send and receive unreliable packets (datagrams)
// and receive feedback from congestion control (via
// CongestionControlInterface). The idea is to send RTP packets as datagrams and
// have underlying implementation of datagram transport to use QUIC datagram
// protocol.
class DatagramTransportInterface {
 public:
  virtual ~DatagramTransportInterface() = default;

  // Connect the datagram transport to the ICE transport.
  // The implementation must be able to ignore incoming packets that don't
  // belong to it.
  virtual void Connect(rtc::PacketTransportInternal* packet_transport) = 0;

  // Returns congestion control feedback interface or nullptr if datagram
  // transport does not implement congestion control.
  //
  // Note that right now datagram transport is used without congestion control,
  // but we plan to use it in the future.
  virtual CongestionControlInterface* congestion_control() = 0;

  // Sets a state observer callback. Before datagram transport is destroyed, the
  // callback must be unregistered by setting it to nullptr.
  // A newly registered callback will be called with the current state.
  // Datagram transport does not invoke this callback concurrently.
  virtual void SetTransportStateCallback(
      MediaTransportStateCallback* callback) = 0;

  // Start asynchronous send of datagram. The status returned by this method
  // only pertains to the synchronous operations (e.g. serialization /
  // packetization), not to the asynchronous operation.
  //
  // Datagrams larger than GetLargestDatagramSize() will fail and return error.
  //
  // Datagrams are sent in FIFO order.
  virtual RTCError SendDatagram(rtc::ArrayView<const uint8_t> data,
                                DatagramId datagram_id) = 0;

  // Returns maximum size of datagram message, does not change.
  // TODO(sukhanov): Because value may be undefined before connection setup
  // is complete, consider returning error when called before connection is
  // established. Currently returns hardcoded const, because integration
  // prototype may call before connection is established.
  virtual size_t GetLargestDatagramSize() const = 0;

  // Sets packet sink. Sink must be unset by calling
  // SetDataTransportSink(nullptr) before the data transport is destroyed or
  // before new sink is set.
  virtual void SetDatagramSink(DatagramSinkInterface* sink) = 0;

  // Retrieves callers config (i.e. media transport offer) that should be passed
  // to the callee, before the call is connected. Such config is opaque to SDP
  // (sdp just passes it through). The config is a binary blob, so SDP may
  // choose to use base64 to serialize it (or any other approach that guarantees
  // that the binary blob goes through). This should only be called for the
  // caller's perspective.
  //
  // TODO(sukhanov): Make pure virtual.
  virtual absl::optional<std::string> GetTransportParametersOffer() const {
    return absl::nullopt;
  }
};

}  // namespace webrtc

#endif  // API_DATAGRAM_TRANSPORT_INTERFACE_H_
