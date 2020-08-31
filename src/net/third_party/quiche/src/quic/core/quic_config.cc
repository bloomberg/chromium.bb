// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_config.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake_message.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_socket_address_coder.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_macros.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_uint128.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace quic {

// Reads the value corresponding to |name_| from |msg| into |out|. If the
// |name_| is absent in |msg| and |presence| is set to OPTIONAL |out| is set
// to |default_value|.
QuicErrorCode ReadUint32(const CryptoHandshakeMessage& msg,
                         QuicTag tag,
                         QuicConfigPresence presence,
                         uint32_t default_value,
                         uint32_t* out,
                         std::string* error_details) {
  DCHECK(error_details != nullptr);
  QuicErrorCode error = msg.GetUint32(tag, out);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence == PRESENCE_REQUIRED) {
        *error_details = "Missing " + QuicTagToString(tag);
        break;
      }
      error = QUIC_NO_ERROR;
      *out = default_value;
      break;
    case QUIC_NO_ERROR:
      break;
    default:
      *error_details = "Bad " + QuicTagToString(tag);
      break;
  }
  return error;
}

QuicConfigValue::QuicConfigValue(QuicTag tag, QuicConfigPresence presence)
    : tag_(tag), presence_(presence) {}
QuicConfigValue::~QuicConfigValue() {}

QuicFixedUint32::QuicFixedUint32(QuicTag tag, QuicConfigPresence presence)
    : QuicConfigValue(tag, presence),
      has_send_value_(false),
      has_receive_value_(false) {}
QuicFixedUint32::~QuicFixedUint32() {}

bool QuicFixedUint32::HasSendValue() const {
  return has_send_value_;
}

uint32_t QuicFixedUint32::GetSendValue() const {
  QUIC_BUG_IF(!has_send_value_)
      << "No send value to get for tag:" << QuicTagToString(tag_);
  return send_value_;
}

void QuicFixedUint32::SetSendValue(uint32_t value) {
  has_send_value_ = true;
  send_value_ = value;
}

bool QuicFixedUint32::HasReceivedValue() const {
  return has_receive_value_;
}

uint32_t QuicFixedUint32::GetReceivedValue() const {
  QUIC_BUG_IF(!has_receive_value_)
      << "No receive value to get for tag:" << QuicTagToString(tag_);
  return receive_value_;
}

void QuicFixedUint32::SetReceivedValue(uint32_t value) {
  has_receive_value_ = true;
  receive_value_ = value;
}

void QuicFixedUint32::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (tag_ == 0) {
    QUIC_BUG
        << "This parameter does not support writing to CryptoHandshakeMessage";
    return;
  }
  if (has_send_value_) {
    out->SetValue(tag_, send_value_);
  }
}

QuicErrorCode QuicFixedUint32::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType /*hello_type*/,
    std::string* error_details) {
  DCHECK(error_details != nullptr);
  if (tag_ == 0) {
    *error_details =
        "This parameter does not support reading from CryptoHandshakeMessage";
    QUIC_BUG << *error_details;
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }
  QuicErrorCode error = peer_hello.GetUint32(tag_, &receive_value_);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicTagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      has_receive_value_ = true;
      break;
    default:
      *error_details = "Bad " + QuicTagToString(tag_);
      break;
  }
  return error;
}

QuicFixedUint62::QuicFixedUint62(QuicTag name, QuicConfigPresence presence)
    : QuicConfigValue(name, presence),
      has_send_value_(false),
      has_receive_value_(false) {}

QuicFixedUint62::~QuicFixedUint62() {}

bool QuicFixedUint62::HasSendValue() const {
  return has_send_value_;
}

uint64_t QuicFixedUint62::GetSendValue() const {
  if (!has_send_value_) {
    QUIC_BUG << "No send value to get for tag:" << QuicTagToString(tag_);
    return 0;
  }
  return send_value_;
}

void QuicFixedUint62::SetSendValue(uint64_t value) {
  if (value > kVarInt62MaxValue) {
    QUIC_BUG << "QuicFixedUint62 invalid value " << value;
    value = kVarInt62MaxValue;
  }
  has_send_value_ = true;
  send_value_ = value;
}

bool QuicFixedUint62::HasReceivedValue() const {
  return has_receive_value_;
}

uint64_t QuicFixedUint62::GetReceivedValue() const {
  if (!has_receive_value_) {
    QUIC_BUG << "No receive value to get for tag:" << QuicTagToString(tag_);
    return 0;
  }
  return receive_value_;
}

void QuicFixedUint62::SetReceivedValue(uint64_t value) {
  has_receive_value_ = true;
  receive_value_ = value;
}

void QuicFixedUint62::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (!has_send_value_) {
    return;
  }
  uint32_t send_value32;
  if (send_value_ > std::numeric_limits<uint32_t>::max()) {
    QUIC_BUG << "Attempting to send " << send_value_
             << " for tag:" << QuicTagToString(tag_);
    send_value32 = std::numeric_limits<uint32_t>::max();
  } else {
    send_value32 = static_cast<uint32_t>(send_value_);
  }
  out->SetValue(tag_, send_value32);
}

QuicErrorCode QuicFixedUint62::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType /*hello_type*/,
    std::string* error_details) {
  DCHECK(error_details != nullptr);
  uint32_t receive_value32;
  QuicErrorCode error = peer_hello.GetUint32(tag_, &receive_value32);
  // GetUint32 is guaranteed to always initialize receive_value32.
  receive_value_ = receive_value32;
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicTagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      has_receive_value_ = true;
      break;
    default:
      *error_details = "Bad " + QuicTagToString(tag_);
      break;
  }
  return error;
}

QuicFixedUint128::QuicFixedUint128(QuicTag tag, QuicConfigPresence presence)
    : QuicConfigValue(tag, presence),
      has_send_value_(false),
      has_receive_value_(false) {}
QuicFixedUint128::~QuicFixedUint128() {}

bool QuicFixedUint128::HasSendValue() const {
  return has_send_value_;
}

QuicUint128 QuicFixedUint128::GetSendValue() const {
  QUIC_BUG_IF(!has_send_value_)
      << "No send value to get for tag:" << QuicTagToString(tag_);
  return send_value_;
}

void QuicFixedUint128::SetSendValue(QuicUint128 value) {
  has_send_value_ = true;
  send_value_ = value;
}

bool QuicFixedUint128::HasReceivedValue() const {
  return has_receive_value_;
}

QuicUint128 QuicFixedUint128::GetReceivedValue() const {
  QUIC_BUG_IF(!has_receive_value_)
      << "No receive value to get for tag:" << QuicTagToString(tag_);
  return receive_value_;
}

void QuicFixedUint128::SetReceivedValue(QuicUint128 value) {
  has_receive_value_ = true;
  receive_value_ = value;
}

void QuicFixedUint128::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (has_send_value_) {
    out->SetValue(tag_, send_value_);
  }
}

QuicErrorCode QuicFixedUint128::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType /*hello_type*/,
    std::string* error_details) {
  DCHECK(error_details != nullptr);
  QuicErrorCode error = peer_hello.GetUint128(tag_, &receive_value_);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicTagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      has_receive_value_ = true;
      break;
    default:
      *error_details = "Bad " + QuicTagToString(tag_);
      break;
  }
  return error;
}

QuicFixedTagVector::QuicFixedTagVector(QuicTag name,
                                       QuicConfigPresence presence)
    : QuicConfigValue(name, presence),
      has_send_values_(false),
      has_receive_values_(false) {}

QuicFixedTagVector::QuicFixedTagVector(const QuicFixedTagVector& other) =
    default;

QuicFixedTagVector::~QuicFixedTagVector() {}

bool QuicFixedTagVector::HasSendValues() const {
  return has_send_values_;
}

const QuicTagVector& QuicFixedTagVector::GetSendValues() const {
  QUIC_BUG_IF(!has_send_values_)
      << "No send values to get for tag:" << QuicTagToString(tag_);
  return send_values_;
}

void QuicFixedTagVector::SetSendValues(const QuicTagVector& values) {
  has_send_values_ = true;
  send_values_ = values;
}

bool QuicFixedTagVector::HasReceivedValues() const {
  return has_receive_values_;
}

const QuicTagVector& QuicFixedTagVector::GetReceivedValues() const {
  QUIC_BUG_IF(!has_receive_values_)
      << "No receive value to get for tag:" << QuicTagToString(tag_);
  return receive_values_;
}

void QuicFixedTagVector::SetReceivedValues(const QuicTagVector& values) {
  has_receive_values_ = true;
  receive_values_ = values;
}

void QuicFixedTagVector::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (has_send_values_) {
    out->SetVector(tag_, send_values_);
  }
}

QuicErrorCode QuicFixedTagVector::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType /*hello_type*/,
    std::string* error_details) {
  DCHECK(error_details != nullptr);
  QuicTagVector values;
  QuicErrorCode error = peer_hello.GetTaglist(tag_, &values);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicTagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      QUIC_DVLOG(1) << "Received Connection Option tags from receiver.";
      has_receive_values_ = true;
      receive_values_.insert(receive_values_.end(), values.begin(),
                             values.end());
      break;
    default:
      *error_details = "Bad " + QuicTagToString(tag_);
      break;
  }
  return error;
}

QuicFixedSocketAddress::QuicFixedSocketAddress(QuicTag tag,
                                               QuicConfigPresence presence)
    : QuicConfigValue(tag, presence),
      has_send_value_(false),
      has_receive_value_(false) {}

QuicFixedSocketAddress::~QuicFixedSocketAddress() {}

bool QuicFixedSocketAddress::HasSendValue() const {
  return has_send_value_;
}

const QuicSocketAddress& QuicFixedSocketAddress::GetSendValue() const {
  QUIC_BUG_IF(!has_send_value_)
      << "No send value to get for tag:" << QuicTagToString(tag_);
  return send_value_;
}

void QuicFixedSocketAddress::SetSendValue(const QuicSocketAddress& value) {
  has_send_value_ = true;
  send_value_ = value;
}

bool QuicFixedSocketAddress::HasReceivedValue() const {
  return has_receive_value_;
}

const QuicSocketAddress& QuicFixedSocketAddress::GetReceivedValue() const {
  QUIC_BUG_IF(!has_receive_value_)
      << "No receive value to get for tag:" << QuicTagToString(tag_);
  return receive_value_;
}

void QuicFixedSocketAddress::SetReceivedValue(const QuicSocketAddress& value) {
  has_receive_value_ = true;
  receive_value_ = value;
}

void QuicFixedSocketAddress::ToHandshakeMessage(
    CryptoHandshakeMessage* out) const {
  if (has_send_value_) {
    QuicSocketAddressCoder address_coder(send_value_);
    out->SetStringPiece(tag_, address_coder.Encode());
  }
}

QuicErrorCode QuicFixedSocketAddress::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType /*hello_type*/,
    std::string* error_details) {
  quiche::QuicheStringPiece address;
  if (!peer_hello.GetStringPiece(tag_, &address)) {
    if (presence_ == PRESENCE_REQUIRED) {
      *error_details = "Missing " + QuicTagToString(tag_);
      return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
    }
  } else {
    QuicSocketAddressCoder address_coder;
    if (address_coder.Decode(address.data(), address.length())) {
      SetReceivedValue(
          QuicSocketAddress(address_coder.ip(), address_coder.port()));
    }
  }
  return QUIC_NO_ERROR;
}

QuicConfig::QuicConfig()
    : negotiated_(false),
      max_time_before_crypto_handshake_(QuicTime::Delta::Zero()),
      max_idle_time_before_crypto_handshake_(QuicTime::Delta::Zero()),
      max_undecryptable_packets_(0),
      connection_options_(kCOPT, PRESENCE_OPTIONAL),
      client_connection_options_(kCLOP, PRESENCE_OPTIONAL),
      idle_timeout_to_send_(QuicTime::Delta::Infinite()),
      max_bidirectional_streams_(kMIBS, PRESENCE_REQUIRED),
      max_unidirectional_streams_(kMIUS, PRESENCE_OPTIONAL),
      bytes_for_connection_id_(kTCID, PRESENCE_OPTIONAL),
      initial_round_trip_time_us_(kIRTT, PRESENCE_OPTIONAL),
      initial_max_stream_data_bytes_incoming_bidirectional_(0,
                                                            PRESENCE_OPTIONAL),
      initial_max_stream_data_bytes_outgoing_bidirectional_(0,
                                                            PRESENCE_OPTIONAL),
      initial_max_stream_data_bytes_unidirectional_(0, PRESENCE_OPTIONAL),
      initial_stream_flow_control_window_bytes_(kSFCW, PRESENCE_OPTIONAL),
      initial_session_flow_control_window_bytes_(kCFCW, PRESENCE_OPTIONAL),
      connection_migration_disabled_(kNCMR, PRESENCE_OPTIONAL),
      alternate_server_address_ipv6_(kASAD, PRESENCE_OPTIONAL),
      alternate_server_address_ipv4_(kASAD, PRESENCE_OPTIONAL),
      stateless_reset_token_(kSRST, PRESENCE_OPTIONAL),
      max_ack_delay_ms_(kMAD, PRESENCE_OPTIONAL),
      ack_delay_exponent_(kADE, PRESENCE_OPTIONAL),
      max_packet_size_(0, PRESENCE_OPTIONAL),
      max_datagram_frame_size_(0, PRESENCE_OPTIONAL),
      active_connection_id_limit_(0, PRESENCE_OPTIONAL) {
  SetDefaults();
}

QuicConfig::QuicConfig(const QuicConfig& other) = default;

QuicConfig::~QuicConfig() {}

bool QuicConfig::SetInitialReceivedConnectionOptions(
    const QuicTagVector& tags) {
  if (HasReceivedConnectionOptions()) {
    // If we have already received connection options (via handshake or due to
    // a previous call), don't re-initialize.
    return false;
  }
  connection_options_.SetReceivedValues(tags);
  return true;
}

void QuicConfig::SetConnectionOptionsToSend(
    const QuicTagVector& connection_options) {
  connection_options_.SetSendValues(connection_options);
}

bool QuicConfig::HasReceivedConnectionOptions() const {
  return connection_options_.HasReceivedValues();
}

const QuicTagVector& QuicConfig::ReceivedConnectionOptions() const {
  return connection_options_.GetReceivedValues();
}

bool QuicConfig::HasSendConnectionOptions() const {
  return connection_options_.HasSendValues();
}

const QuicTagVector& QuicConfig::SendConnectionOptions() const {
  return connection_options_.GetSendValues();
}

bool QuicConfig::HasClientSentConnectionOption(QuicTag tag,
                                               Perspective perspective) const {
  if (perspective == Perspective::IS_SERVER) {
    if (HasReceivedConnectionOptions() &&
        ContainsQuicTag(ReceivedConnectionOptions(), tag)) {
      return true;
    }
  } else if (HasSendConnectionOptions() &&
             ContainsQuicTag(SendConnectionOptions(), tag)) {
    return true;
  }
  return false;
}

void QuicConfig::SetClientConnectionOptions(
    const QuicTagVector& client_connection_options) {
  client_connection_options_.SetSendValues(client_connection_options);
}

bool QuicConfig::HasClientRequestedIndependentOption(
    QuicTag tag,
    Perspective perspective) const {
  if (perspective == Perspective::IS_SERVER) {
    return (HasReceivedConnectionOptions() &&
            ContainsQuicTag(ReceivedConnectionOptions(), tag));
  }

  return (client_connection_options_.HasSendValues() &&
          ContainsQuicTag(client_connection_options_.GetSendValues(), tag));
}

const QuicTagVector& QuicConfig::ClientRequestedIndependentOptions(
    Perspective perspective) const {
  static const QuicTagVector* no_options = new QuicTagVector;
  if (perspective == Perspective::IS_SERVER) {
    return HasReceivedConnectionOptions() ? ReceivedConnectionOptions()
                                          : *no_options;
  }

  return client_connection_options_.HasSendValues()
             ? client_connection_options_.GetSendValues()
             : *no_options;
}

void QuicConfig::SetIdleNetworkTimeout(QuicTime::Delta idle_network_timeout) {
  if (idle_network_timeout.ToMicroseconds() <= 0) {
    QUIC_BUG << "Invalid idle network timeout " << idle_network_timeout;
    return;
  }
  idle_timeout_to_send_ = idle_network_timeout;
}

QuicTime::Delta QuicConfig::IdleNetworkTimeout() const {
  // TODO(b/152032210) add a QUIC_BUG to ensure that is not called before we've
  // received the peer's values. This is true in production code but not in all
  // of our tests that use a fake QuicConfig.
  if (!received_idle_timeout_.has_value()) {
    return idle_timeout_to_send_;
  }
  return received_idle_timeout_.value();
}

void QuicConfig::SetMaxBidirectionalStreamsToSend(uint32_t max_streams) {
  max_bidirectional_streams_.SetSendValue(max_streams);
}

uint32_t QuicConfig::GetMaxBidirectionalStreamsToSend() const {
  return max_bidirectional_streams_.GetSendValue();
}

bool QuicConfig::HasReceivedMaxBidirectionalStreams() const {
  return max_bidirectional_streams_.HasReceivedValue();
}

uint32_t QuicConfig::ReceivedMaxBidirectionalStreams() const {
  return max_bidirectional_streams_.GetReceivedValue();
}

void QuicConfig::SetMaxUnidirectionalStreamsToSend(uint32_t max_streams) {
  max_unidirectional_streams_.SetSendValue(max_streams);
}

uint32_t QuicConfig::GetMaxUnidirectionalStreamsToSend() const {
  return max_unidirectional_streams_.GetSendValue();
}

bool QuicConfig::HasReceivedMaxUnidirectionalStreams() const {
  return max_unidirectional_streams_.HasReceivedValue();
}

uint32_t QuicConfig::ReceivedMaxUnidirectionalStreams() const {
  return max_unidirectional_streams_.GetReceivedValue();
}

void QuicConfig::SetMaxAckDelayToSendMs(uint32_t max_ack_delay_ms) {
  return max_ack_delay_ms_.SetSendValue(max_ack_delay_ms);
}

uint32_t QuicConfig::GetMaxAckDelayToToSendMs() const {
  return max_ack_delay_ms_.GetSendValue();
}

bool QuicConfig::HasReceivedMaxAckDelayMs() const {
  return max_ack_delay_ms_.HasReceivedValue();
}

uint32_t QuicConfig::ReceivedMaxAckDelayMs() const {
  return max_ack_delay_ms_.GetReceivedValue();
}

void QuicConfig::SetAckDelayExponentToSend(uint32_t exponent) {
  ack_delay_exponent_.SetSendValue(exponent);
}

uint32_t QuicConfig::GetAckDelayExponentToSend() const {
  return ack_delay_exponent_.GetSendValue();
}

bool QuicConfig::HasReceivedAckDelayExponent() const {
  return ack_delay_exponent_.HasReceivedValue();
}

uint32_t QuicConfig::ReceivedAckDelayExponent() const {
  return ack_delay_exponent_.GetReceivedValue();
}

void QuicConfig::SetMaxPacketSizeToSend(uint64_t max_packet_size) {
  max_packet_size_.SetSendValue(max_packet_size);
}

uint64_t QuicConfig::GetMaxPacketSizeToSend() const {
  return max_packet_size_.GetSendValue();
}

bool QuicConfig::HasReceivedMaxPacketSize() const {
  return max_packet_size_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedMaxPacketSize() const {
  return max_packet_size_.GetReceivedValue();
}

void QuicConfig::SetMaxDatagramFrameSizeToSend(
    uint64_t max_datagram_frame_size) {
  max_datagram_frame_size_.SetSendValue(max_datagram_frame_size);
}

uint64_t QuicConfig::GetMaxDatagramFrameSizeToSend() const {
  return max_datagram_frame_size_.GetSendValue();
}

bool QuicConfig::HasReceivedMaxDatagramFrameSize() const {
  return max_datagram_frame_size_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedMaxDatagramFrameSize() const {
  return max_datagram_frame_size_.GetReceivedValue();
}

void QuicConfig::SetActiveConnectionIdLimitToSend(
    uint64_t active_connection_id_limit) {
  active_connection_id_limit_.SetSendValue(active_connection_id_limit);
}

uint64_t QuicConfig::GetActiveConnectionIdLimitToSend() const {
  return active_connection_id_limit_.GetSendValue();
}

bool QuicConfig::HasReceivedActiveConnectionIdLimit() const {
  return active_connection_id_limit_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedActiveConnectionIdLimit() const {
  return active_connection_id_limit_.GetReceivedValue();
}

bool QuicConfig::HasSetBytesForConnectionIdToSend() const {
  return bytes_for_connection_id_.HasSendValue();
}

void QuicConfig::SetBytesForConnectionIdToSend(uint32_t bytes) {
  bytes_for_connection_id_.SetSendValue(bytes);
}

bool QuicConfig::HasReceivedBytesForConnectionId() const {
  return bytes_for_connection_id_.HasReceivedValue();
}

uint32_t QuicConfig::ReceivedBytesForConnectionId() const {
  return bytes_for_connection_id_.GetReceivedValue();
}

void QuicConfig::SetInitialRoundTripTimeUsToSend(uint64_t rtt) {
  initial_round_trip_time_us_.SetSendValue(rtt);
}

bool QuicConfig::HasReceivedInitialRoundTripTimeUs() const {
  return initial_round_trip_time_us_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedInitialRoundTripTimeUs() const {
  return initial_round_trip_time_us_.GetReceivedValue();
}

bool QuicConfig::HasInitialRoundTripTimeUsToSend() const {
  return initial_round_trip_time_us_.HasSendValue();
}

uint64_t QuicConfig::GetInitialRoundTripTimeUsToSend() const {
  return initial_round_trip_time_us_.GetSendValue();
}

void QuicConfig::SetInitialStreamFlowControlWindowToSend(
    uint64_t window_bytes) {
  if (window_bytes < kMinimumFlowControlSendWindow) {
    QUIC_BUG << "Initial stream flow control receive window (" << window_bytes
             << ") cannot be set lower than minimum ("
             << kMinimumFlowControlSendWindow << ").";
    window_bytes = kMinimumFlowControlSendWindow;
  }
  initial_stream_flow_control_window_bytes_.SetSendValue(window_bytes);
}

uint64_t QuicConfig::GetInitialStreamFlowControlWindowToSend() const {
  return initial_stream_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialStreamFlowControlWindowBytes() const {
  return initial_stream_flow_control_window_bytes_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedInitialStreamFlowControlWindowBytes() const {
  return initial_stream_flow_control_window_bytes_.GetReceivedValue();
}

void QuicConfig::SetInitialMaxStreamDataBytesIncomingBidirectionalToSend(
    uint64_t window_bytes) {
  initial_max_stream_data_bytes_incoming_bidirectional_.SetSendValue(
      window_bytes);
}

uint64_t QuicConfig::GetInitialMaxStreamDataBytesIncomingBidirectionalToSend()
    const {
  if (initial_max_stream_data_bytes_incoming_bidirectional_.HasSendValue()) {
    return initial_max_stream_data_bytes_incoming_bidirectional_.GetSendValue();
  }
  return initial_stream_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialMaxStreamDataBytesIncomingBidirectional()
    const {
  return initial_max_stream_data_bytes_incoming_bidirectional_
      .HasReceivedValue();
}

uint64_t QuicConfig::ReceivedInitialMaxStreamDataBytesIncomingBidirectional()
    const {
  return initial_max_stream_data_bytes_incoming_bidirectional_
      .GetReceivedValue();
}

void QuicConfig::SetInitialMaxStreamDataBytesOutgoingBidirectionalToSend(
    uint64_t window_bytes) {
  initial_max_stream_data_bytes_outgoing_bidirectional_.SetSendValue(
      window_bytes);
}

uint64_t QuicConfig::GetInitialMaxStreamDataBytesOutgoingBidirectionalToSend()
    const {
  if (initial_max_stream_data_bytes_outgoing_bidirectional_.HasSendValue()) {
    return initial_max_stream_data_bytes_outgoing_bidirectional_.GetSendValue();
  }
  return initial_stream_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialMaxStreamDataBytesOutgoingBidirectional()
    const {
  return initial_max_stream_data_bytes_outgoing_bidirectional_
      .HasReceivedValue();
}

uint64_t QuicConfig::ReceivedInitialMaxStreamDataBytesOutgoingBidirectional()
    const {
  return initial_max_stream_data_bytes_outgoing_bidirectional_
      .GetReceivedValue();
}

void QuicConfig::SetInitialMaxStreamDataBytesUnidirectionalToSend(
    uint64_t window_bytes) {
  initial_max_stream_data_bytes_unidirectional_.SetSendValue(window_bytes);
}

uint64_t QuicConfig::GetInitialMaxStreamDataBytesUnidirectionalToSend() const {
  if (initial_max_stream_data_bytes_unidirectional_.HasSendValue()) {
    return initial_max_stream_data_bytes_unidirectional_.GetSendValue();
  }
  return initial_stream_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialMaxStreamDataBytesUnidirectional() const {
  return initial_max_stream_data_bytes_unidirectional_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedInitialMaxStreamDataBytesUnidirectional() const {
  return initial_max_stream_data_bytes_unidirectional_.GetReceivedValue();
}

void QuicConfig::SetInitialSessionFlowControlWindowToSend(
    uint64_t window_bytes) {
  if (window_bytes < kMinimumFlowControlSendWindow) {
    QUIC_BUG << "Initial session flow control receive window (" << window_bytes
             << ") cannot be set lower than default ("
             << kMinimumFlowControlSendWindow << ").";
    window_bytes = kMinimumFlowControlSendWindow;
  }
  initial_session_flow_control_window_bytes_.SetSendValue(window_bytes);
}

uint64_t QuicConfig::GetInitialSessionFlowControlWindowToSend() const {
  return initial_session_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialSessionFlowControlWindowBytes() const {
  return initial_session_flow_control_window_bytes_.HasReceivedValue();
}

uint64_t QuicConfig::ReceivedInitialSessionFlowControlWindowBytes() const {
  return initial_session_flow_control_window_bytes_.GetReceivedValue();
}

void QuicConfig::SetDisableConnectionMigration() {
  connection_migration_disabled_.SetSendValue(1);
}

bool QuicConfig::DisableConnectionMigration() const {
  return connection_migration_disabled_.HasReceivedValue();
}

void QuicConfig::SetIPv6AlternateServerAddressToSend(
    const QuicSocketAddress& alternate_server_address_ipv6) {
  if (!alternate_server_address_ipv6.host().IsIPv6()) {
    QUIC_BUG << "Cannot use SetIPv6AlternateServerAddressToSend with "
             << alternate_server_address_ipv6;
    return;
  }
  alternate_server_address_ipv6_.SetSendValue(alternate_server_address_ipv6);
}

bool QuicConfig::HasReceivedIPv6AlternateServerAddress() const {
  return alternate_server_address_ipv6_.HasReceivedValue();
}

const QuicSocketAddress& QuicConfig::ReceivedIPv6AlternateServerAddress()
    const {
  return alternate_server_address_ipv6_.GetReceivedValue();
}

void QuicConfig::SetIPv4AlternateServerAddressToSend(
    const QuicSocketAddress& alternate_server_address_ipv4) {
  if (!alternate_server_address_ipv4.host().IsIPv4()) {
    QUIC_BUG << "Cannot use SetIPv4AlternateServerAddressToSend with "
             << alternate_server_address_ipv4;
    return;
  }
  alternate_server_address_ipv4_.SetSendValue(alternate_server_address_ipv4);
}

bool QuicConfig::HasReceivedIPv4AlternateServerAddress() const {
  return alternate_server_address_ipv4_.HasReceivedValue();
}

const QuicSocketAddress& QuicConfig::ReceivedIPv4AlternateServerAddress()
    const {
  return alternate_server_address_ipv4_.GetReceivedValue();
}

void QuicConfig::SetOriginalConnectionIdToSend(
    const QuicConnectionId& original_connection_id) {
  original_connection_id_to_send_ = original_connection_id;
}

bool QuicConfig::HasReceivedOriginalConnectionId() const {
  return received_original_connection_id_.has_value();
}

QuicConnectionId QuicConfig::ReceivedOriginalConnectionId() const {
  if (!HasReceivedOriginalConnectionId()) {
    QUIC_BUG << "No received original connection ID";
    return EmptyQuicConnectionId();
  }
  return received_original_connection_id_.value();
}

void QuicConfig::SetStatelessResetTokenToSend(
    QuicUint128 stateless_reset_token) {
  stateless_reset_token_.SetSendValue(stateless_reset_token);
}

bool QuicConfig::HasReceivedStatelessResetToken() const {
  return stateless_reset_token_.HasReceivedValue();
}

QuicUint128 QuicConfig::ReceivedStatelessResetToken() const {
  return stateless_reset_token_.GetReceivedValue();
}

bool QuicConfig::negotiated() const {
  return negotiated_;
}

void QuicConfig::SetCreateSessionTagIndicators(QuicTagVector tags) {
  create_session_tag_indicators_ = std::move(tags);
}

const QuicTagVector& QuicConfig::create_session_tag_indicators() const {
  return create_session_tag_indicators_;
}

void QuicConfig::SetDefaults() {
  SetIdleNetworkTimeout(QuicTime::Delta::FromSeconds(kMaximumIdleTimeoutSecs));
  SetMaxBidirectionalStreamsToSend(kDefaultMaxStreamsPerConnection);
  SetMaxUnidirectionalStreamsToSend(kDefaultMaxStreamsPerConnection);
  max_time_before_crypto_handshake_ =
      QuicTime::Delta::FromSeconds(kMaxTimeForCryptoHandshakeSecs);
  max_idle_time_before_crypto_handshake_ =
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs);
  max_undecryptable_packets_ = kDefaultMaxUndecryptablePackets;

  SetInitialStreamFlowControlWindowToSend(kMinimumFlowControlSendWindow);
  SetInitialSessionFlowControlWindowToSend(kMinimumFlowControlSendWindow);
  SetMaxAckDelayToSendMs(kDefaultDelayedAckTimeMs);
  SetAckDelayExponentToSend(kDefaultAckDelayExponent);
  SetMaxPacketSizeToSend(kMaxIncomingPacketSize);
  SetMaxDatagramFrameSizeToSend(kMaxAcceptedDatagramFrameSize);
}

void QuicConfig::ToHandshakeMessage(
    CryptoHandshakeMessage* out,
    QuicTransportVersion transport_version) const {
  // Idle timeout has custom rules that are different from other values.
  // We configure ourselves with the minumum value between the one sent and
  // the one received. Additionally, when QUIC_CRYPTO is used, the server
  // MUST send an idle timeout no greater than the idle timeout it received
  // from the client. We therefore send the received value if it is lower.
  QuicFixedUint32 idle_timeout_seconds(kICSL, PRESENCE_REQUIRED);
  uint32_t idle_timeout_to_send_seconds = idle_timeout_to_send_.ToSeconds();
  if (received_idle_timeout_.has_value() &&
      received_idle_timeout_->ToSeconds() < idle_timeout_to_send_seconds) {
    idle_timeout_to_send_seconds = received_idle_timeout_->ToSeconds();
  }
  idle_timeout_seconds.SetSendValue(idle_timeout_to_send_seconds);
  idle_timeout_seconds.ToHandshakeMessage(out);

  // Do not need a version check here, max...bi... will encode
  // as "MIDS" -- the max initial dynamic streams tag -- if
  // doing some version other than IETF QUIC.
  max_bidirectional_streams_.ToHandshakeMessage(out);
  if (VersionHasIetfQuicFrames(transport_version)) {
    max_unidirectional_streams_.ToHandshakeMessage(out);
    ack_delay_exponent_.ToHandshakeMessage(out);
  }
  if (GetQuicReloadableFlag(quic_negotiate_ack_delay_time)) {
    QUIC_RELOADABLE_FLAG_COUNT_N(quic_negotiate_ack_delay_time, 1, 4);
    max_ack_delay_ms_.ToHandshakeMessage(out);
  }
  bytes_for_connection_id_.ToHandshakeMessage(out);
  initial_round_trip_time_us_.ToHandshakeMessage(out);
  initial_stream_flow_control_window_bytes_.ToHandshakeMessage(out);
  initial_session_flow_control_window_bytes_.ToHandshakeMessage(out);
  connection_migration_disabled_.ToHandshakeMessage(out);
  connection_options_.ToHandshakeMessage(out);
  if (alternate_server_address_ipv6_.HasSendValue()) {
    alternate_server_address_ipv6_.ToHandshakeMessage(out);
  } else {
    alternate_server_address_ipv4_.ToHandshakeMessage(out);
  }
  stateless_reset_token_.ToHandshakeMessage(out);
}

QuicErrorCode QuicConfig::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    std::string* error_details) {
  DCHECK(error_details != nullptr);

  QuicErrorCode error = QUIC_NO_ERROR;
  if (error == QUIC_NO_ERROR) {
    // Idle timeout has custom rules that are different from other values.
    // We configure ourselves with the minumum value between the one sent and
    // the one received. Additionally, when QUIC_CRYPTO is used, the server
    // MUST send an idle timeout no greater than the idle timeout it received
    // from the client.
    QuicFixedUint32 idle_timeout_seconds(kICSL, PRESENCE_REQUIRED);
    error = idle_timeout_seconds.ProcessPeerHello(peer_hello, hello_type,
                                                  error_details);
    if (error == QUIC_NO_ERROR) {
      if (idle_timeout_seconds.GetReceivedValue() >
          idle_timeout_to_send_.ToSeconds()) {
        // The received value is higher than ours, ignore it if from the client
        // and raise an error if from the server.
        if (hello_type == SERVER) {
          error = QUIC_INVALID_NEGOTIATED_VALUE;
          *error_details =
              "Invalid value received for " + QuicTagToString(kICSL);
        }
      } else {
        received_idle_timeout_ = QuicTime::Delta::FromSeconds(
            idle_timeout_seconds.GetReceivedValue());
      }
    }
  }
  if (error == QUIC_NO_ERROR) {
    error = max_bidirectional_streams_.ProcessPeerHello(peer_hello, hello_type,
                                                        error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = max_unidirectional_streams_.ProcessPeerHello(peer_hello, hello_type,
                                                         error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = bytes_for_connection_id_.ProcessPeerHello(peer_hello, hello_type,
                                                      error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_round_trip_time_us_.ProcessPeerHello(peer_hello, hello_type,
                                                         error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_stream_flow_control_window_bytes_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_session_flow_control_window_bytes_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = connection_migration_disabled_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = connection_options_.ProcessPeerHello(peer_hello, hello_type,
                                                 error_details);
  }
  if (error == QUIC_NO_ERROR) {
    QuicFixedSocketAddress alternate_server_address(kASAD, PRESENCE_OPTIONAL);
    error = alternate_server_address.ProcessPeerHello(peer_hello, hello_type,
                                                      error_details);
    if (error == QUIC_NO_ERROR && alternate_server_address.HasReceivedValue()) {
      const QuicSocketAddress& received_address =
          alternate_server_address.GetReceivedValue();
      if (received_address.host().IsIPv6()) {
        alternate_server_address_ipv6_.SetReceivedValue(received_address);
      } else if (received_address.host().IsIPv4()) {
        alternate_server_address_ipv4_.SetReceivedValue(received_address);
      }
    }
  }
  if (error == QUIC_NO_ERROR) {
    error = stateless_reset_token_.ProcessPeerHello(peer_hello, hello_type,
                                                    error_details);
  }

  if (GetQuicReloadableFlag(quic_negotiate_ack_delay_time) &&
      error == QUIC_NO_ERROR) {
    QUIC_RELOADABLE_FLAG_COUNT_N(quic_negotiate_ack_delay_time, 2, 4);
    error = max_ack_delay_ms_.ProcessPeerHello(peer_hello, hello_type,
                                               error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = ack_delay_exponent_.ProcessPeerHello(peer_hello, hello_type,
                                                 error_details);
  }
  if (error == QUIC_NO_ERROR) {
    negotiated_ = true;
  }
  return error;
}

bool QuicConfig::FillTransportParameters(TransportParameters* params) const {
  if (original_connection_id_to_send_.has_value()) {
    params->original_connection_id = original_connection_id_to_send_.value();
  }

  params->idle_timeout_milliseconds.set_value(
      idle_timeout_to_send_.ToMilliseconds());

  if (stateless_reset_token_.HasSendValue()) {
    QuicUint128 stateless_reset_token = stateless_reset_token_.GetSendValue();
    params->stateless_reset_token.assign(
        reinterpret_cast<const char*>(&stateless_reset_token),
        reinterpret_cast<const char*>(&stateless_reset_token) +
            sizeof(stateless_reset_token));
  }

  params->max_packet_size.set_value(GetMaxPacketSizeToSend());
  params->max_datagram_frame_size.set_value(GetMaxDatagramFrameSizeToSend());
  params->initial_max_data.set_value(
      GetInitialSessionFlowControlWindowToSend());
  // The max stream data bidirectional transport parameters can be either local
  // or remote. A stream is local iff it is initiated by the endpoint that sent
  // the transport parameter (see the Transport Parameter Definitions section of
  // draft-ietf-quic-transport). In this function we are sending transport
  // parameters, so a local stream is one we initiated, which means an outgoing
  // stream.
  params->initial_max_stream_data_bidi_local.set_value(
      GetInitialMaxStreamDataBytesOutgoingBidirectionalToSend());
  params->initial_max_stream_data_bidi_remote.set_value(
      GetInitialMaxStreamDataBytesIncomingBidirectionalToSend());
  params->initial_max_stream_data_uni.set_value(
      GetInitialMaxStreamDataBytesUnidirectionalToSend());
  params->initial_max_streams_bidi.set_value(
      GetMaxBidirectionalStreamsToSend());
  params->initial_max_streams_uni.set_value(
      GetMaxUnidirectionalStreamsToSend());
  if (GetQuicReloadableFlag(quic_negotiate_ack_delay_time)) {
    QUIC_RELOADABLE_FLAG_COUNT_N(quic_negotiate_ack_delay_time, 3, 4);
    params->max_ack_delay.set_value(kDefaultDelayedAckTimeMs);
  }
  params->ack_delay_exponent.set_value(GetAckDelayExponentToSend());
  params->disable_migration =
      connection_migration_disabled_.HasSendValue() &&
      connection_migration_disabled_.GetSendValue() != 0;

  if (alternate_server_address_ipv6_.HasSendValue() ||
      alternate_server_address_ipv4_.HasSendValue()) {
    TransportParameters::PreferredAddress preferred_address;
    if (alternate_server_address_ipv6_.HasSendValue()) {
      preferred_address.ipv6_socket_address =
          alternate_server_address_ipv6_.GetSendValue();
    }
    if (alternate_server_address_ipv4_.HasSendValue()) {
      preferred_address.ipv4_socket_address =
          alternate_server_address_ipv4_.GetSendValue();
    }
    params->preferred_address =
        std::make_unique<TransportParameters::PreferredAddress>(
            preferred_address);
  }

  if (active_connection_id_limit_.HasSendValue()) {
    params->active_connection_id_limit.set_value(
        active_connection_id_limit_.GetSendValue());
  }

  if (GetQuicRestartFlag(quic_google_transport_param_send_new)) {
    QUIC_RESTART_FLAG_COUNT_N(quic_google_transport_param_send_new, 1, 3);
    if (initial_round_trip_time_us_.HasSendValue()) {
      params->initial_round_trip_time_us.set_value(
          initial_round_trip_time_us_.GetSendValue());
    }
    if (connection_options_.HasSendValues() &&
        !connection_options_.GetSendValues().empty()) {
      params->google_connection_options = connection_options_.GetSendValues();
    }
  }

  if (!GetQuicRestartFlag(quic_google_transport_param_omit_old)) {
    if (!params->google_quic_params) {
      params->google_quic_params = std::make_unique<CryptoHandshakeMessage>();
    }
    initial_round_trip_time_us_.ToHandshakeMessage(
        params->google_quic_params.get());
    connection_options_.ToHandshakeMessage(params->google_quic_params.get());
  } else {
    QUIC_RESTART_FLAG_COUNT_N(quic_google_transport_param_omit_old, 1, 3);
  }

  params->custom_parameters = custom_transport_parameters_to_send_;

  return true;
}

QuicErrorCode QuicConfig::ProcessTransportParameters(
    const TransportParameters& params,
    HelloType hello_type,
    bool is_resumption,
    std::string* error_details) {
  if (!is_resumption && params.original_connection_id.has_value()) {
    received_original_connection_id_ = params.original_connection_id.value();
  }

  if (params.idle_timeout_milliseconds.value() > 0 &&
      params.idle_timeout_milliseconds.value() <
          static_cast<uint64_t>(idle_timeout_to_send_.ToMilliseconds())) {
    // An idle timeout of zero indicates it is disabled.
    // We also ignore values higher than ours which will cause us to use the
    // smallest value between ours and our peer's.
    received_idle_timeout_ = QuicTime::Delta::FromMilliseconds(
        params.idle_timeout_milliseconds.value());
  }

  if (!is_resumption && !params.stateless_reset_token.empty()) {
    QuicUint128 stateless_reset_token;
    if (params.stateless_reset_token.size() != sizeof(stateless_reset_token)) {
      QUIC_BUG << "Bad stateless reset token length "
               << params.stateless_reset_token.size();
      *error_details = "Bad stateless reset token length";
      return QUIC_INTERNAL_ERROR;
    }
    memcpy(&stateless_reset_token, params.stateless_reset_token.data(),
           params.stateless_reset_token.size());
    stateless_reset_token_.SetReceivedValue(stateless_reset_token);
  }

  if (params.max_packet_size.IsValid()) {
    max_packet_size_.SetReceivedValue(params.max_packet_size.value());
  }

  if (params.max_datagram_frame_size.IsValid()) {
    max_datagram_frame_size_.SetReceivedValue(
        params.max_datagram_frame_size.value());
  }

  initial_session_flow_control_window_bytes_.SetReceivedValue(
      params.initial_max_data.value());

  // IETF QUIC specifies stream IDs and stream counts as 62-bit integers but
  // our implementation uses uint32_t to represent them to save memory.
  max_bidirectional_streams_.SetReceivedValue(
      std::min<uint64_t>(params.initial_max_streams_bidi.value(),
                         std::numeric_limits<uint32_t>::max()));
  max_unidirectional_streams_.SetReceivedValue(
      std::min<uint64_t>(params.initial_max_streams_uni.value(),
                         std::numeric_limits<uint32_t>::max()));

  // The max stream data bidirectional transport parameters can be either local
  // or remote. A stream is local iff it is initiated by the endpoint that sent
  // the transport parameter (see the Transport Parameter Definitions section of
  // draft-ietf-quic-transport). However in this function we are processing
  // received transport parameters, so a local stream is one initiated by our
  // peer, which means an incoming stream.
  initial_max_stream_data_bytes_incoming_bidirectional_.SetReceivedValue(
      params.initial_max_stream_data_bidi_local.value());
  initial_max_stream_data_bytes_outgoing_bidirectional_.SetReceivedValue(
      params.initial_max_stream_data_bidi_remote.value());
  initial_max_stream_data_bytes_unidirectional_.SetReceivedValue(
      params.initial_max_stream_data_uni.value());

  if (!is_resumption) {
    if (GetQuicReloadableFlag(quic_negotiate_ack_delay_time)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_negotiate_ack_delay_time, 4, 4);
      max_ack_delay_ms_.SetReceivedValue(params.max_ack_delay.value());
    }
    if (params.ack_delay_exponent.IsValid()) {
      ack_delay_exponent_.SetReceivedValue(params.ack_delay_exponent.value());
    }
    if (params.preferred_address != nullptr) {
      if (params.preferred_address->ipv6_socket_address.port() != 0) {
        alternate_server_address_ipv6_.SetReceivedValue(
            params.preferred_address->ipv6_socket_address);
      }
      if (params.preferred_address->ipv4_socket_address.port() != 0) {
        alternate_server_address_ipv4_.SetReceivedValue(
            params.preferred_address->ipv4_socket_address);
      }
    }
  }

  if (params.disable_migration) {
    connection_migration_disabled_.SetReceivedValue(1u);
  }

  active_connection_id_limit_.SetReceivedValue(
      params.active_connection_id_limit.value());

  bool google_params_already_parsed = false;
  if (GetQuicRestartFlag(quic_google_transport_param_send_new)) {
    QUIC_RESTART_FLAG_COUNT_N(quic_google_transport_param_send_new, 2, 3);
    if (params.initial_round_trip_time_us.value() > 0) {
      google_params_already_parsed = true;
      initial_round_trip_time_us_.SetReceivedValue(
          params.initial_round_trip_time_us.value());
    }
    if (params.google_connection_options.has_value()) {
      google_params_already_parsed = true;
      connection_options_.SetReceivedValues(
          params.google_connection_options.value());
    }
  }

  if (!GetQuicRestartFlag(quic_google_transport_param_omit_old)) {
    const CryptoHandshakeMessage* peer_params = params.google_quic_params.get();
    if (peer_params != nullptr && !google_params_already_parsed) {
      QuicErrorCode error = initial_round_trip_time_us_.ProcessPeerHello(
          *peer_params, hello_type, error_details);
      if (error != QUIC_NO_ERROR) {
        DCHECK(!error_details->empty());
        return error;
      }
      error = connection_options_.ProcessPeerHello(*peer_params, hello_type,
                                                   error_details);
      if (error != QUIC_NO_ERROR) {
        DCHECK(!error_details->empty());
        return error;
      }
    }
  } else {
    QUIC_RESTART_FLAG_COUNT_N(quic_google_transport_param_omit_old, 2, 3);
  }

  received_custom_transport_parameters_ = params.custom_parameters;

  if (!is_resumption) {
    negotiated_ = true;
  }
  *error_details = "";
  return QUIC_NO_ERROR;
}

}  // namespace quic
