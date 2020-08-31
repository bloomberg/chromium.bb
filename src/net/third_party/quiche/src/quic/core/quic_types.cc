// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_types.h"

#include <cstdint>

#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_str_cat.h"

namespace quic {

std::ostream& operator<<(std::ostream& os, const QuicConsumedData& s) {
  os << "bytes_consumed: " << s.bytes_consumed
     << " fin_consumed: " << s.fin_consumed;
  return os;
}

std::string PerspectiveToString(Perspective perspective) {
  if (perspective == Perspective::IS_SERVER) {
    return "IS_SERVER";
  }
  if (perspective == Perspective::IS_CLIENT) {
    return "IS_CLIENT";
  }
  return quiche::QuicheStrCat("Unknown(", static_cast<int>(perspective), ")");
}

std::ostream& operator<<(std::ostream& os, const Perspective& perspective) {
  os << PerspectiveToString(perspective);
  return os;
}

std::string ConnectionCloseSourceToString(
    ConnectionCloseSource connection_close_source) {
  if (connection_close_source == ConnectionCloseSource::FROM_PEER) {
    return "FROM_PEER";
  }
  if (connection_close_source == ConnectionCloseSource::FROM_SELF) {
    return "FROM_SELF";
  }
  return quiche::QuicheStrCat("Unknown(",
                              static_cast<int>(connection_close_source), ")");
}

std::ostream& operator<<(std::ostream& os,
                         const ConnectionCloseSource& connection_close_source) {
  os << ConnectionCloseSourceToString(connection_close_source);
  return os;
}

std::string ConnectionCloseBehaviorToString(
    ConnectionCloseBehavior connection_close_behavior) {
  if (connection_close_behavior == ConnectionCloseBehavior::SILENT_CLOSE) {
    return "SILENT_CLOSE";
  }
  if (connection_close_behavior ==
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET) {
    return "SEND_CONNECTION_CLOSE_PACKET";
  }
  return quiche::QuicheStrCat("Unknown(",
                              static_cast<int>(connection_close_behavior), ")");
}

std::ostream& operator<<(
    std::ostream& os,
    const ConnectionCloseBehavior& connection_close_behavior) {
  os << ConnectionCloseBehaviorToString(connection_close_behavior);
  return os;
}

std::ostream& operator<<(std::ostream& os, const AckedPacket& acked_packet) {
  os << "{ packet_number: " << acked_packet.packet_number
     << ", bytes_acked: " << acked_packet.bytes_acked << ", receive_timestamp: "
     << acked_packet.receive_timestamp.ToDebuggingValue() << "} ";
  return os;
}

std::ostream& operator<<(std::ostream& os, const LostPacket& lost_packet) {
  os << "{ packet_number: " << lost_packet.packet_number
     << ", bytes_lost: " << lost_packet.bytes_lost << "} ";
  return os;
}

std::string HistogramEnumString(WriteStatus enum_value) {
  switch (enum_value) {
    case WRITE_STATUS_OK:
      return "OK";
    case WRITE_STATUS_BLOCKED:
      return "BLOCKED";
    case WRITE_STATUS_BLOCKED_DATA_BUFFERED:
      return "BLOCKED_DATA_BUFFERED";
    case WRITE_STATUS_ERROR:
      return "ERROR";
    case WRITE_STATUS_MSG_TOO_BIG:
      return "MSG_TOO_BIG";
    case WRITE_STATUS_FAILED_TO_COALESCE_PACKET:
      return "WRITE_STATUS_FAILED_TO_COALESCE_PACKET";
    case WRITE_STATUS_NUM_VALUES:
      return "NUM_VALUES";
  }
  QUIC_DLOG(ERROR) << "Invalid WriteStatus value: "
                   << static_cast<int16_t>(enum_value);
  return "<invalid>";
}

std::ostream& operator<<(std::ostream& os, const WriteStatus& status) {
  os << HistogramEnumString(status);
  return os;
}

std::ostream& operator<<(std::ostream& os, const WriteResult& s) {
  os << "{ status: " << s.status;
  if (s.status == WRITE_STATUS_OK) {
    os << ", bytes_written: " << s.bytes_written;
  } else {
    os << ", error_code: " << s.error_code;
  }
  os << " }";
  return os;
}

MessageResult::MessageResult(MessageStatus status, QuicMessageId message_id)
    : status(status), message_id(message_id) {}

#define RETURN_STRING_LITERAL(x) \
  case x:                        \
    return #x;

std::string QuicIetfFrameTypeString(QuicIetfFrameType t) {
  if (IS_IETF_STREAM_FRAME(t)) {
    return "IETF_STREAM";
  }

  switch (t) {
    RETURN_STRING_LITERAL(IETF_PADDING);
    RETURN_STRING_LITERAL(IETF_PING);
    RETURN_STRING_LITERAL(IETF_ACK);
    RETURN_STRING_LITERAL(IETF_ACK_ECN);
    RETURN_STRING_LITERAL(IETF_RST_STREAM);
    RETURN_STRING_LITERAL(IETF_STOP_SENDING);
    RETURN_STRING_LITERAL(IETF_CRYPTO);
    RETURN_STRING_LITERAL(IETF_NEW_TOKEN);
    RETURN_STRING_LITERAL(IETF_MAX_DATA);
    RETURN_STRING_LITERAL(IETF_MAX_STREAM_DATA);
    RETURN_STRING_LITERAL(IETF_MAX_STREAMS_BIDIRECTIONAL);
    RETURN_STRING_LITERAL(IETF_MAX_STREAMS_UNIDIRECTIONAL);
    RETURN_STRING_LITERAL(IETF_DATA_BLOCKED);
    RETURN_STRING_LITERAL(IETF_STREAM_DATA_BLOCKED);
    RETURN_STRING_LITERAL(IETF_STREAMS_BLOCKED_BIDIRECTIONAL);
    RETURN_STRING_LITERAL(IETF_STREAMS_BLOCKED_UNIDIRECTIONAL);
    RETURN_STRING_LITERAL(IETF_NEW_CONNECTION_ID);
    RETURN_STRING_LITERAL(IETF_RETIRE_CONNECTION_ID);
    RETURN_STRING_LITERAL(IETF_PATH_CHALLENGE);
    RETURN_STRING_LITERAL(IETF_PATH_RESPONSE);
    RETURN_STRING_LITERAL(IETF_CONNECTION_CLOSE);
    RETURN_STRING_LITERAL(IETF_APPLICATION_CLOSE);
    RETURN_STRING_LITERAL(IETF_EXTENSION_MESSAGE_NO_LENGTH);
    RETURN_STRING_LITERAL(IETF_EXTENSION_MESSAGE);
    RETURN_STRING_LITERAL(IETF_EXTENSION_MESSAGE_NO_LENGTH_V99);
    RETURN_STRING_LITERAL(IETF_EXTENSION_MESSAGE_V99);
    default:
      return quiche::QuicheStrCat("Private value (", t, ")");
  }
}
std::ostream& operator<<(std::ostream& os, const QuicIetfFrameType& c) {
  os << QuicIetfFrameTypeString(c);
  return os;
}

std::string TransmissionTypeToString(TransmissionType transmission_type) {
  switch (transmission_type) {
    RETURN_STRING_LITERAL(NOT_RETRANSMISSION);
    RETURN_STRING_LITERAL(HANDSHAKE_RETRANSMISSION);
    RETURN_STRING_LITERAL(ALL_UNACKED_RETRANSMISSION);
    RETURN_STRING_LITERAL(ALL_INITIAL_RETRANSMISSION);
    RETURN_STRING_LITERAL(LOSS_RETRANSMISSION);
    RETURN_STRING_LITERAL(RTO_RETRANSMISSION);
    RETURN_STRING_LITERAL(TLP_RETRANSMISSION);
    RETURN_STRING_LITERAL(PTO_RETRANSMISSION);
    RETURN_STRING_LITERAL(PROBING_RETRANSMISSION);
    default:
      // Some varz rely on this behavior for statistic collection.
      if (transmission_type == LAST_TRANSMISSION_TYPE + 1) {
        return "INVALID_TRANSMISSION_TYPE";
      }
      return quiche::QuicheStrCat("Unknown(",
                                  static_cast<int>(transmission_type), ")");
      break;
  }
}

std::string PacketHeaderFormatToString(PacketHeaderFormat format) {
  switch (format) {
    RETURN_STRING_LITERAL(IETF_QUIC_LONG_HEADER_PACKET);
    RETURN_STRING_LITERAL(IETF_QUIC_SHORT_HEADER_PACKET);
    RETURN_STRING_LITERAL(GOOGLE_QUIC_PACKET);
    default:
      return quiche::QuicheStrCat("Unknown (", static_cast<int>(format), ")");
  }
}

std::string QuicLongHeaderTypeToString(QuicLongHeaderType type) {
  switch (type) {
    RETURN_STRING_LITERAL(VERSION_NEGOTIATION);
    RETURN_STRING_LITERAL(INITIAL);
    RETURN_STRING_LITERAL(ZERO_RTT_PROTECTED);
    RETURN_STRING_LITERAL(HANDSHAKE);
    RETURN_STRING_LITERAL(RETRY);
    RETURN_STRING_LITERAL(INVALID_PACKET_TYPE);
    default:
      return quiche::QuicheStrCat("Unknown (", static_cast<int>(type), ")");
  }
}

std::string MessageStatusToString(MessageStatus message_status) {
  switch (message_status) {
    RETURN_STRING_LITERAL(MESSAGE_STATUS_SUCCESS);
    RETURN_STRING_LITERAL(MESSAGE_STATUS_ENCRYPTION_NOT_ESTABLISHED);
    RETURN_STRING_LITERAL(MESSAGE_STATUS_UNSUPPORTED);
    RETURN_STRING_LITERAL(MESSAGE_STATUS_BLOCKED);
    RETURN_STRING_LITERAL(MESSAGE_STATUS_TOO_LARGE);
    RETURN_STRING_LITERAL(MESSAGE_STATUS_INTERNAL_ERROR);
    default:
      return quiche::QuicheStrCat("Unknown(", static_cast<int>(message_status),
                                  ")");
      break;
  }
}

std::string MessageResultToString(MessageResult message_result) {
  if (message_result.status != MESSAGE_STATUS_SUCCESS) {
    return quiche::QuicheStrCat(
        "{", MessageStatusToString(message_result.status), "}");
  }
  return quiche::QuicheStrCat(
      "{MESSAGE_STATUS_SUCCESS,id=", message_result.message_id, "}");
}

std::ostream& operator<<(std::ostream& os, const MessageResult& mr) {
  os << MessageResultToString(mr);
  return os;
}

std::string PacketNumberSpaceToString(PacketNumberSpace packet_number_space) {
  switch (packet_number_space) {
    RETURN_STRING_LITERAL(INITIAL_DATA);
    RETURN_STRING_LITERAL(HANDSHAKE_DATA);
    RETURN_STRING_LITERAL(APPLICATION_DATA);
    default:
      return quiche::QuicheStrCat("Unknown(",
                                  static_cast<int>(packet_number_space), ")");
      break;
  }
}

std::string SerializedPacketFateToString(SerializedPacketFate fate) {
  switch (fate) {
    RETURN_STRING_LITERAL(COALESCE);
    RETURN_STRING_LITERAL(BUFFER);
    RETURN_STRING_LITERAL(SEND_TO_WRITER);
    RETURN_STRING_LITERAL(FAILED_TO_WRITE_COALESCED_PACKET);
    default:
      return quiche::QuicheStrCat("Unknown(", static_cast<int>(fate), ")");
  }
}

std::string EncryptionLevelToString(EncryptionLevel level) {
  switch (level) {
    RETURN_STRING_LITERAL(ENCRYPTION_INITIAL);
    RETURN_STRING_LITERAL(ENCRYPTION_HANDSHAKE);
    RETURN_STRING_LITERAL(ENCRYPTION_ZERO_RTT);
    RETURN_STRING_LITERAL(ENCRYPTION_FORWARD_SECURE);
    default:
      return quiche::QuicheStrCat("Unknown(", static_cast<int>(level), ")");
      break;
  }
}

std::string QuicConnectionCloseTypeString(QuicConnectionCloseType type) {
  switch (type) {
    RETURN_STRING_LITERAL(GOOGLE_QUIC_CONNECTION_CLOSE);
    RETURN_STRING_LITERAL(IETF_QUIC_TRANSPORT_CONNECTION_CLOSE);
    RETURN_STRING_LITERAL(IETF_QUIC_APPLICATION_CONNECTION_CLOSE);
    default:
      return quiche::QuicheStrCat("Unknown(", static_cast<int>(type), ")");
      break;
  }
}

std::ostream& operator<<(std::ostream& os, const QuicConnectionCloseType type) {
  os << QuicConnectionCloseTypeString(type);
  return os;
}

std::string AddressChangeTypeToString(AddressChangeType type) {
  using IntType = typename std::underlying_type<AddressChangeType>::type;
  switch (type) {
    RETURN_STRING_LITERAL(NO_CHANGE);
    RETURN_STRING_LITERAL(PORT_CHANGE);
    RETURN_STRING_LITERAL(IPV4_SUBNET_CHANGE);
    RETURN_STRING_LITERAL(IPV4_TO_IPV4_CHANGE);
    RETURN_STRING_LITERAL(IPV4_TO_IPV6_CHANGE);
    RETURN_STRING_LITERAL(IPV6_TO_IPV4_CHANGE);
    RETURN_STRING_LITERAL(IPV6_TO_IPV6_CHANGE);
    default:
      return quiche::QuicheStrCat("Unknown(", static_cast<IntType>(type), ")");
  }
}

std::ostream& operator<<(std::ostream& os, AddressChangeType type) {
  os << AddressChangeTypeToString(type);
  return os;
}

#undef RETURN_STRING_LITERAL  // undef for jumbo builds

}  // namespace quic
