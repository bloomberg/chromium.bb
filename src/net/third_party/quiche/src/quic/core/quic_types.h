// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QUIC_TYPES_H_
#define QUICHE_QUIC_CORE_QUIC_TYPES_H_

#include <array>
#include <cstddef>
#include <map>
#include <ostream>
#include <vector>

#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_number.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_containers.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"

namespace quic {

using QuicPacketLength = uint16_t;
using QuicControlFrameId = uint32_t;
using QuicMessageId = uint32_t;
using QuicDatagramFlowId = uint64_t;

// IMPORTANT: IETF QUIC defines stream IDs and stream counts as being unsigned
// 62-bit numbers. However, we have decided to only support up to 2^32-1 streams
// in order to reduce the size of data structures such as QuicStreamFrame
// and QuicTransmissionInfo, as that allows them to fit in cache lines and has
// visible perfomance impact.
using QuicStreamId = uint32_t;

// Count of stream IDs. Used in MAX_STREAMS and STREAMS_BLOCKED frames.
using QuicStreamCount = QuicStreamId;

using QuicByteCount = uint64_t;
using QuicPacketCount = uint64_t;
using QuicPublicResetNonceProof = uint64_t;
using QuicStreamOffset = uint64_t;
using DiversificationNonce = std::array<char, 32>;
using PacketTimeVector = std::vector<std::pair<QuicPacketNumber, QuicTime>>;

enum : size_t { kQuicPathFrameBufferSize = 8 };
using QuicPathFrameBuffer = std::array<uint8_t, kQuicPathFrameBufferSize>;

// Application error code used in the QUIC Stop Sending frame.
using QuicApplicationErrorCode = uint16_t;

// The connection id sequence number specifies the order that connection
// ids must be used in. This is also the sequence number carried in
// the IETF QUIC NEW_CONNECTION_ID and RETIRE_CONNECTION_ID frames.
using QuicConnectionIdSequenceNumber = uint64_t;

// A custom data that represents application-specific settings.
// In HTTP/3 for example, it includes the encoded SETTINGS.
using ApplicationState = std::vector<uint8_t>;

// A struct for functions which consume data payloads and fins.
struct QUIC_EXPORT_PRIVATE QuicConsumedData {
  constexpr QuicConsumedData(size_t bytes_consumed, bool fin_consumed)
      : bytes_consumed(bytes_consumed), fin_consumed(fin_consumed) {}

  // By default, gtest prints the raw bytes of an object. The bool data
  // member causes this object to have padding bytes, which causes the
  // default gtest object printer to read uninitialize memory. So we need
  // to teach gtest how to print this object.
  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os,
      const QuicConsumedData& s);

  // How many bytes were consumed.
  size_t bytes_consumed;

  // True if an incoming fin was consumed.
  bool fin_consumed;
};

// QuicAsyncStatus enumerates the possible results of an asynchronous
// operation.
enum QuicAsyncStatus {
  QUIC_SUCCESS = 0,
  QUIC_FAILURE = 1,
  // QUIC_PENDING results from an operation that will occur asynchronously. When
  // the operation is complete, a callback's |Run| method will be called.
  QUIC_PENDING = 2,
};

// TODO(wtc): see if WriteStatus can be replaced by QuicAsyncStatus.
enum WriteStatus : int16_t {
  WRITE_STATUS_OK,
  // Write is blocked, caller needs to retry.
  WRITE_STATUS_BLOCKED,
  // Write is blocked but the packet data is buffered, caller should not retry.
  WRITE_STATUS_BLOCKED_DATA_BUFFERED,
  // To make the IsWriteError(WriteStatus) function work properly:
  // - Non-errors MUST be added before WRITE_STATUS_ERROR.
  // - Errors MUST be added after WRITE_STATUS_ERROR.
  WRITE_STATUS_ERROR,
  WRITE_STATUS_MSG_TOO_BIG,
  WRITE_STATUS_FAILED_TO_COALESCE_PACKET,
  WRITE_STATUS_NUM_VALUES,
};

std::string HistogramEnumString(WriteStatus enum_value);
QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                             const WriteStatus& status);

inline std::string HistogramEnumDescription(WriteStatus /*dummy*/) {
  return "status";
}

inline bool IsWriteBlockedStatus(WriteStatus status) {
  return status == WRITE_STATUS_BLOCKED ||
         status == WRITE_STATUS_BLOCKED_DATA_BUFFERED;
}

inline bool IsWriteError(WriteStatus status) {
  return status >= WRITE_STATUS_ERROR;
}

// A struct used to return the result of write calls including either the number
// of bytes written or the error code, depending upon the status.
struct QUIC_EXPORT_PRIVATE WriteResult {
  constexpr WriteResult(WriteStatus status, int bytes_written_or_error_code)
      : status(status), bytes_written(bytes_written_or_error_code) {}

  constexpr WriteResult() : WriteResult(WRITE_STATUS_ERROR, 0) {}

  bool operator==(const WriteResult& other) const {
    if (status != other.status) {
      return false;
    }
    switch (status) {
      case WRITE_STATUS_OK:
        return bytes_written == other.bytes_written;
      case WRITE_STATUS_BLOCKED:
      case WRITE_STATUS_BLOCKED_DATA_BUFFERED:
        return true;
      default:
        return error_code == other.error_code;
    }
  }

  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(std::ostream& os,
                                                      const WriteResult& s);

  WriteStatus status;
  // Number of packets dropped as a result of this write.
  // Only used by batch writers. Otherwise always 0.
  uint16_t dropped_packets = 0;
  // TODO(wub): In some cases, WRITE_STATUS_ERROR may set an error_code and
  // WRITE_STATUS_BLOCKED_DATA_BUFFERED may set bytes_written. This may need
  // some cleaning up so that perhaps both values can be set and valid.
  union {
    int bytes_written;  // only valid when status is WRITE_STATUS_OK
    int error_code;     // only valid when status is WRITE_STATUS_ERROR
  };
};

enum TransmissionType : int8_t {
  NOT_RETRANSMISSION,
  FIRST_TRANSMISSION_TYPE = NOT_RETRANSMISSION,
  HANDSHAKE_RETRANSMISSION,  // Retransmits due to handshake timeouts.
  // TODO(fayang): remove ALL_UNACKED_RETRANSMISSION.
  ALL_UNACKED_RETRANSMISSION,  // Retransmits all unacked packets.
  ALL_INITIAL_RETRANSMISSION,  // Retransmits all initially encrypted packets.
  LOSS_RETRANSMISSION,         // Retransmits due to loss detection.
  RTO_RETRANSMISSION,          // Retransmits due to retransmit time out.
  TLP_RETRANSMISSION,          // Tail loss probes.
  PTO_RETRANSMISSION,          // Retransmission due to probe timeout.
  PROBING_RETRANSMISSION,      // Retransmission in order to probe bandwidth.
  LAST_TRANSMISSION_TYPE = PROBING_RETRANSMISSION,
};

QUIC_EXPORT_PRIVATE std::string TransmissionTypeToString(
    TransmissionType transmission_type);

enum HasRetransmittableData : uint8_t {
  NO_RETRANSMITTABLE_DATA,
  HAS_RETRANSMITTABLE_DATA,
};

enum IsHandshake : uint8_t { NOT_HANDSHAKE, IS_HANDSHAKE };

enum class Perspective : uint8_t { IS_SERVER, IS_CLIENT };

QUIC_EXPORT_PRIVATE std::string PerspectiveToString(Perspective perspective);
QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                             const Perspective& perspective);

// Describes whether a ConnectionClose was originated by the peer.
enum class ConnectionCloseSource { FROM_PEER, FROM_SELF };

QUIC_EXPORT_PRIVATE std::string ConnectionCloseSourceToString(
    ConnectionCloseSource connection_close_source);
QUIC_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream& os,
    const ConnectionCloseSource& connection_close_source);

// Should a connection be closed silently or not.
enum class ConnectionCloseBehavior {
  SILENT_CLOSE,
  SEND_CONNECTION_CLOSE_PACKET
};

QUIC_EXPORT_PRIVATE std::string ConnectionCloseBehaviorToString(
    ConnectionCloseBehavior connection_close_behavior);
QUIC_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream& os,
    const ConnectionCloseBehavior& connection_close_behavior);

enum QuicFrameType : uint8_t {
  // Regular frame types. The values set here cannot change without the
  // introduction of a new QUIC version.
  PADDING_FRAME = 0,
  RST_STREAM_FRAME = 1,
  CONNECTION_CLOSE_FRAME = 2,
  GOAWAY_FRAME = 3,
  WINDOW_UPDATE_FRAME = 4,
  BLOCKED_FRAME = 5,
  STOP_WAITING_FRAME = 6,
  PING_FRAME = 7,
  CRYPTO_FRAME = 8,

  // STREAM and ACK frames are special frames. They are encoded differently on
  // the wire and their values do not need to be stable.
  STREAM_FRAME,
  ACK_FRAME,
  // The path MTU discovery frame is encoded as a PING frame on the wire.
  MTU_DISCOVERY_FRAME,

  // These are for IETF-specific frames for which there is no mapping
  // from Google QUIC frames. These are valid/allowed if and only if IETF-
  // QUIC has been negotiated. Values are not important, they are not
  // the values that are in the packets (see QuicIetfFrameType, below).
  NEW_CONNECTION_ID_FRAME,
  MAX_STREAMS_FRAME,
  STREAMS_BLOCKED_FRAME,
  PATH_RESPONSE_FRAME,
  PATH_CHALLENGE_FRAME,
  STOP_SENDING_FRAME,
  MESSAGE_FRAME,
  NEW_TOKEN_FRAME,
  RETIRE_CONNECTION_ID_FRAME,
  HANDSHAKE_DONE_FRAME,

  NUM_FRAME_TYPES
};

// Ietf frame types. These are defined in the IETF QUIC Specification.
// Explicit values are given in the enum so that we can be sure that
// the symbol will map to the correct stream type.
// All types are defined here, even if we have not yet implmented the
// quic/core/stream/.... stuff needed.
// Note: The protocol specifies that frame types are varint-62 encoded,
// further stating that the shortest encoding must be used.  The current set of
// frame types all have values less than 0x40 (64) so can be encoded in a single
// byte, with the two most significant bits being 0. Thus, the following
// enumerations are valid as both the numeric values of frame types AND their
// encodings.
enum QuicIetfFrameType : uint8_t {
  IETF_PADDING = 0x00,
  IETF_PING = 0x01,
  IETF_ACK = 0x02,
  IETF_ACK_ECN = 0x03,
  IETF_RST_STREAM = 0x04,
  IETF_STOP_SENDING = 0x05,
  IETF_CRYPTO = 0x06,
  IETF_NEW_TOKEN = 0x07,
  // the low-3 bits of the stream frame type value are actually flags
  // declaring what parts of the frame are/are-not present, as well as
  // some other control information. The code would then do something
  // along the lines of "if ((frame_type & 0xf8) == 0x08)" to determine
  // whether the frame is a stream frame or not, and then examine each
  // bit specifically when/as needed.
  IETF_STREAM = 0x08,
  // 0x09 through 0x0f are various flag settings of the IETF_STREAM frame.
  IETF_MAX_DATA = 0x10,
  IETF_MAX_STREAM_DATA = 0x11,
  IETF_MAX_STREAMS_BIDIRECTIONAL = 0x12,
  IETF_MAX_STREAMS_UNIDIRECTIONAL = 0x13,
  IETF_DATA_BLOCKED = 0x14,
  IETF_STREAM_DATA_BLOCKED = 0x15,
  IETF_STREAMS_BLOCKED_BIDIRECTIONAL = 0x16,
  IETF_STREAMS_BLOCKED_UNIDIRECTIONAL = 0x17,
  IETF_NEW_CONNECTION_ID = 0x18,
  IETF_RETIRE_CONNECTION_ID = 0x19,
  IETF_PATH_CHALLENGE = 0x1a,
  IETF_PATH_RESPONSE = 0x1b,
  // Both of the following are "Connection Close" frames,
  // the first signals transport-layer errors, the second application-layer
  // errors.
  IETF_CONNECTION_CLOSE = 0x1c,
  IETF_APPLICATION_CLOSE = 0x1d,

  IETF_HANDSHAKE_DONE = 0x1e,

  // The MESSAGE frame type has not yet been fully standardized.
  // QUIC versions starting with 46 and before 99 use 0x20-0x21.
  // IETF QUIC (v99) uses 0x30-0x31, see draft-pauly-quic-datagram.
  IETF_EXTENSION_MESSAGE_NO_LENGTH = 0x20,
  IETF_EXTENSION_MESSAGE = 0x21,
  IETF_EXTENSION_MESSAGE_NO_LENGTH_V99 = 0x30,
  IETF_EXTENSION_MESSAGE_V99 = 0x31,
};
QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                             const QuicIetfFrameType& c);
QUIC_EXPORT_PRIVATE std::string QuicIetfFrameTypeString(QuicIetfFrameType t);

// Masks for the bits that indicate the frame is a Stream frame vs the
// bits used as flags.
#define IETF_STREAM_FRAME_TYPE_MASK 0xfffffffffffffff8
#define IETF_STREAM_FRAME_FLAG_MASK 0x07
#define IS_IETF_STREAM_FRAME(_stype_) \
  (((_stype_)&IETF_STREAM_FRAME_TYPE_MASK) == IETF_STREAM)

// These are the values encoded in the low-order 3 bits of the
// IETF_STREAMx frame type.
#define IETF_STREAM_FRAME_FIN_BIT 0x01
#define IETF_STREAM_FRAME_LEN_BIT 0x02
#define IETF_STREAM_FRAME_OFF_BIT 0x04

enum QuicVariableLengthIntegerLength : uint8_t {
  // Length zero means the variable length integer is not present.
  VARIABLE_LENGTH_INTEGER_LENGTH_0 = 0,
  VARIABLE_LENGTH_INTEGER_LENGTH_1 = 1,
  VARIABLE_LENGTH_INTEGER_LENGTH_2 = 2,
  VARIABLE_LENGTH_INTEGER_LENGTH_4 = 4,
  VARIABLE_LENGTH_INTEGER_LENGTH_8 = 8,

  // By default we write the IETF long header length using the 2-byte encoding
  // of variable length integers, even when the length is below 64, which allows
  // us to fill in the length before knowing what the length actually is.
  kQuicDefaultLongHeaderLengthLength = VARIABLE_LENGTH_INTEGER_LENGTH_2,
};

enum QuicPacketNumberLength : uint8_t {
  PACKET_1BYTE_PACKET_NUMBER = 1,
  PACKET_2BYTE_PACKET_NUMBER = 2,
  PACKET_3BYTE_PACKET_NUMBER = 3,  // Used in versions 45+.
  PACKET_4BYTE_PACKET_NUMBER = 4,
  IETF_MAX_PACKET_NUMBER_LENGTH = 4,
  // TODO(rch): Remove these when we remove QUIC_VERSION_43 since these values
  // are not representable with v46 and above.
  PACKET_6BYTE_PACKET_NUMBER = 6,
  PACKET_8BYTE_PACKET_NUMBER = 8
};

// Used to indicate a QuicSequenceNumberLength using two flag bits.
enum QuicPacketNumberLengthFlags {
  PACKET_FLAGS_1BYTE_PACKET = 0,           // 00
  PACKET_FLAGS_2BYTE_PACKET = 1,           // 01
  PACKET_FLAGS_4BYTE_PACKET = 1 << 1,      // 10
  PACKET_FLAGS_8BYTE_PACKET = 1 << 1 | 1,  // 11
};

// The public flags are specified in one byte.
enum QuicPacketPublicFlags {
  PACKET_PUBLIC_FLAGS_NONE = 0,

  // Bit 0: Does the packet header contains version info?
  PACKET_PUBLIC_FLAGS_VERSION = 1 << 0,

  // Bit 1: Is this packet a public reset packet?
  PACKET_PUBLIC_FLAGS_RST = 1 << 1,

  // Bit 2: indicates the header includes a nonce.
  PACKET_PUBLIC_FLAGS_NONCE = 1 << 2,

  // Bit 3: indicates whether a ConnectionID is included.
  PACKET_PUBLIC_FLAGS_0BYTE_CONNECTION_ID = 0,
  PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID = 1 << 3,

  // QUIC_VERSION_32 and earlier use two bits for an 8 byte
  // connection id.
  PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID_OLD = 1 << 3 | 1 << 2,

  // Bits 4 and 5 describe the packet number length as follows:
  // --00----: 1 byte
  // --01----: 2 bytes
  // --10----: 4 bytes
  // --11----: 6 bytes
  PACKET_PUBLIC_FLAGS_1BYTE_PACKET = PACKET_FLAGS_1BYTE_PACKET << 4,
  PACKET_PUBLIC_FLAGS_2BYTE_PACKET = PACKET_FLAGS_2BYTE_PACKET << 4,
  PACKET_PUBLIC_FLAGS_4BYTE_PACKET = PACKET_FLAGS_4BYTE_PACKET << 4,
  PACKET_PUBLIC_FLAGS_6BYTE_PACKET = PACKET_FLAGS_8BYTE_PACKET << 4,

  // Reserved, unimplemented flags:

  // Bit 7: indicates the presence of a second flags byte.
  PACKET_PUBLIC_FLAGS_TWO_OR_MORE_BYTES = 1 << 7,

  // All bits set (bits 6 and 7 are not currently used): 00111111
  PACKET_PUBLIC_FLAGS_MAX = (1 << 6) - 1,
};

// The private flags are specified in one byte.
enum QuicPacketPrivateFlags {
  PACKET_PRIVATE_FLAGS_NONE = 0,

  // Bit 0: Does this packet contain an entropy bit?
  PACKET_PRIVATE_FLAGS_ENTROPY = 1 << 0,

  // (bits 1-7 are not used): 00000001
  PACKET_PRIVATE_FLAGS_MAX = (1 << 1) - 1
};

// Defines for all types of congestion control algorithms that can be used in
// QUIC. Note that this is separate from the congestion feedback type -
// some congestion control algorithms may use the same feedback type
// (Reno and Cubic are the classic example for that).
enum CongestionControlType {
  kCubicBytes,
  kRenoBytes,
  kBBR,
  kPCC,
  kGoogCC,
  kBBRv2,
};

// EncryptionLevel enumerates the stages of encryption that a QUIC connection
// progresses through. When retransmitting a packet, the encryption level needs
// to be specified so that it is retransmitted at a level which the peer can
// understand.
enum EncryptionLevel : int8_t {
  ENCRYPTION_INITIAL = 0,
  ENCRYPTION_HANDSHAKE = 1,
  ENCRYPTION_ZERO_RTT = 2,
  ENCRYPTION_FORWARD_SECURE = 3,

  NUM_ENCRYPTION_LEVELS,
};

inline bool EncryptionLevelIsValid(EncryptionLevel level) {
  return ENCRYPTION_INITIAL <= level && level < NUM_ENCRYPTION_LEVELS;
}

QUIC_EXPORT_PRIVATE std::string EncryptionLevelToString(EncryptionLevel level);

// Enumeration of whether a server endpoint will request a client certificate,
// and whether that endpoint requires a valid client certificate to establish a
// connection.
enum class ClientCertMode {
  kNone,     // Do not request a client certificate.  Default server behavior.
  kRequest,  // Request a certificate, but allow unauthenticated connections.
  kRequire,  // Require clients to provide a valid certificate.
};

enum AddressChangeType : uint8_t {
  // IP address and port remain unchanged.
  NO_CHANGE,
  // Port changed, but IP address remains unchanged.
  PORT_CHANGE,
  // IPv4 address changed, but within the /24 subnet (port may have changed.)
  IPV4_SUBNET_CHANGE,
  // IPv4 address changed, excluding /24 subnet change (port may have changed.)
  IPV4_TO_IPV4_CHANGE,
  // IP address change from an IPv4 to an IPv6 address (port may have changed.)
  IPV4_TO_IPV6_CHANGE,
  // IP address change from an IPv6 to an IPv4 address (port may have changed.)
  IPV6_TO_IPV4_CHANGE,
  // IP address change from an IPv6 to an IPv6 address (port may have changed.)
  IPV6_TO_IPV6_CHANGE,
};

QUIC_EXPORT_PRIVATE std::string AddressChangeTypeToString(
    AddressChangeType type);

QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                             AddressChangeType type);

enum StreamSendingState {
  // Sender has more data to send on this stream.
  NO_FIN,
  // Sender is done sending on this stream.
  FIN,
  // Sender is done sending on this stream and random padding needs to be
  // appended after all stream frames.
  FIN_AND_PADDING,
};

enum SentPacketState : uint8_t {
  // The packet is in flight and waiting to be acked.
  OUTSTANDING,
  FIRST_PACKET_STATE = OUTSTANDING,
  // The packet was never sent.
  NEVER_SENT,
  // The packet has been acked.
  ACKED,
  // This packet is not expected to be acked.
  UNACKABLE,
  // This packet has been delivered or unneeded.
  NEUTERED,

  // States below are corresponding to retransmission types in TransmissionType.

  // This packet has been retransmitted when retransmission timer fires in
  // HANDSHAKE mode.
  HANDSHAKE_RETRANSMITTED,
  // This packet is considered as lost, this is used for LOST_RETRANSMISSION.
  LOST,
  // This packet has been retransmitted when TLP fires.
  TLP_RETRANSMITTED,
  // This packet has been retransmitted when RTO fires.
  RTO_RETRANSMITTED,
  // This packet has been retransmitted when PTO fires.
  PTO_RETRANSMITTED,
  // This packet has been retransmitted for probing purpose.
  PROBE_RETRANSMITTED,
  LAST_PACKET_STATE = PROBE_RETRANSMITTED,
};

enum PacketHeaderFormat : uint8_t {
  IETF_QUIC_LONG_HEADER_PACKET,
  IETF_QUIC_SHORT_HEADER_PACKET,
  GOOGLE_QUIC_PACKET,
};

QUIC_EXPORT_PRIVATE std::string PacketHeaderFormatToString(
    PacketHeaderFormat format);

// Information about a newly acknowledged packet.
struct QUIC_EXPORT_PRIVATE AckedPacket {
  constexpr AckedPacket(QuicPacketNumber packet_number,
                        QuicPacketLength bytes_acked,
                        QuicTime receive_timestamp)
      : packet_number(packet_number),
        bytes_acked(bytes_acked),
        receive_timestamp(receive_timestamp) {}

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const AckedPacket& acked_packet);

  QuicPacketNumber packet_number;
  // Number of bytes sent in the packet that was acknowledged.
  QuicPacketLength bytes_acked;
  // The time |packet_number| was received by the peer, according to the
  // optional timestamp the peer included in the ACK frame which acknowledged
  // |packet_number|. Zero if no timestamp was available for this packet.
  QuicTime receive_timestamp;
};

// A vector of acked packets.
using AckedPacketVector = QuicInlinedVector<AckedPacket, 2>;

// Information about a newly lost packet.
struct QUIC_EXPORT_PRIVATE LostPacket {
  LostPacket(QuicPacketNumber packet_number, QuicPacketLength bytes_lost)
      : packet_number(packet_number), bytes_lost(bytes_lost) {}

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const LostPacket& lost_packet);

  QuicPacketNumber packet_number;
  // Number of bytes sent in the packet that was lost.
  QuicPacketLength bytes_lost;
};

// A vector of lost packets.
using LostPacketVector = QuicInlinedVector<LostPacket, 2>;

// Please note, this value cannot used directly for packet serialization.
enum QuicLongHeaderType : uint8_t {
  VERSION_NEGOTIATION,
  INITIAL,
  ZERO_RTT_PROTECTED,
  HANDSHAKE,
  RETRY,

  INVALID_PACKET_TYPE,
};

QUIC_EXPORT_PRIVATE std::string QuicLongHeaderTypeToString(
    QuicLongHeaderType type);

enum QuicPacketHeaderTypeFlags : uint8_t {
  // Bit 2: Reserved for experimentation for short header.
  FLAGS_EXPERIMENTATION_BIT = 1 << 2,
  // Bit 3: Google QUIC Demultiplexing bit, the short header always sets this
  // bit to 0, allowing to distinguish Google QUIC packets from short header
  // packets.
  FLAGS_DEMULTIPLEXING_BIT = 1 << 3,
  // Bits 4 and 5: Reserved bits for short header.
  FLAGS_SHORT_HEADER_RESERVED_1 = 1 << 4,
  FLAGS_SHORT_HEADER_RESERVED_2 = 1 << 5,
  // Bit 6: the 'QUIC' bit.
  FLAGS_FIXED_BIT = 1 << 6,
  // Bit 7: Indicates the header is long or short header.
  FLAGS_LONG_HEADER = 1 << 7,
};

enum MessageStatus {
  MESSAGE_STATUS_SUCCESS,
  MESSAGE_STATUS_ENCRYPTION_NOT_ESTABLISHED,  // Failed to send message because
                                              // encryption is not established
                                              // yet.
  MESSAGE_STATUS_UNSUPPORTED,  // Failed to send message because MESSAGE frame
                               // is not supported by the connection.
  MESSAGE_STATUS_BLOCKED,      // Failed to send message because connection is
                           // congestion control blocked or underlying socket is
                           // write blocked.
  MESSAGE_STATUS_TOO_LARGE,  // Failed to send message because the message is
                             // too large to fit into a single packet.
  MESSAGE_STATUS_INTERNAL_ERROR,  // Failed to send message because connection
                                  // reaches an invalid state.
};

QUIC_EXPORT_PRIVATE std::string MessageStatusToString(
    MessageStatus message_status);

// Used to return the result of SendMessage calls
struct QUIC_EXPORT_PRIVATE MessageResult {
  MessageResult(MessageStatus status, QuicMessageId message_id);

  bool operator==(const MessageResult& other) const {
    return status == other.status && message_id == other.message_id;
  }

  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(std::ostream& os,
                                                      const MessageResult& mr);

  MessageStatus status;
  // Only valid when status is MESSAGE_STATUS_SUCCESS.
  QuicMessageId message_id;
};

QUIC_EXPORT_PRIVATE std::string MessageResultToString(
    MessageResult message_result);

enum WriteStreamDataResult {
  WRITE_SUCCESS,
  STREAM_MISSING,  // Trying to write data of a nonexistent stream (e.g.
                   // closed).
  WRITE_FAILED,    // Trying to write nonexistent data of a stream
};

enum StreamType {
  // Bidirectional streams allow for data to be sent in both directions.
  BIDIRECTIONAL,

  // Unidirectional streams carry data in one direction only.
  WRITE_UNIDIRECTIONAL,
  READ_UNIDIRECTIONAL,
  // Not actually a stream type. Used only by QuicCryptoStream when it uses
  // CRYPTO frames and isn't actually a QuicStream.
  CRYPTO,
};

// A packet number space is the context in which a packet can be processed and
// acknowledged.
enum PacketNumberSpace : uint8_t {
  INITIAL_DATA = 0,  // Only used in IETF QUIC.
  HANDSHAKE_DATA = 1,
  APPLICATION_DATA = 2,

  NUM_PACKET_NUMBER_SPACES,
};

QUIC_EXPORT_PRIVATE std::string PacketNumberSpaceToString(
    PacketNumberSpace packet_number_space);

enum AckMode { TCP_ACKING, ACK_DECIMATION, ACK_DECIMATION_WITH_REORDERING };

// Used to return the result of processing a received ACK frame.
enum AckResult {
  PACKETS_NEWLY_ACKED,
  NO_PACKETS_NEWLY_ACKED,
  UNSENT_PACKETS_ACKED,     // Peer acks unsent packets.
  UNACKABLE_PACKETS_ACKED,  // Peer acks packets that are not expected to be
                            // acked. For example, encryption is reestablished,
                            // and all sent encrypted packets cannot be
                            // decrypted by the peer. Version gets negotiated,
                            // and all sent packets in the different version
                            // cannot be processed by the peer.
  PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE,
};

// Indicates the fate of a serialized packet in WritePacket().
enum SerializedPacketFate : uint8_t {
  COALESCE,                          // Try to coalesce packet.
  BUFFER,                            // Buffer packet in buffered_packets_.
  SEND_TO_WRITER,                    // Send packet to writer.
  FAILED_TO_WRITE_COALESCED_PACKET,  // Packet cannot be coalesced, error occurs
                                     // when sending existing coalesced packet.
};

QUIC_EXPORT_PRIVATE std::string SerializedPacketFateToString(
    SerializedPacketFate fate);

// There are three different forms of CONNECTION_CLOSE.
enum QuicConnectionCloseType {
  GOOGLE_QUIC_CONNECTION_CLOSE = 0,
  IETF_QUIC_TRANSPORT_CONNECTION_CLOSE = 1,
  IETF_QUIC_APPLICATION_CONNECTION_CLOSE = 2
};

QUIC_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream& os,
    const QuicConnectionCloseType type);

QUIC_EXPORT_PRIVATE std::string QuicConnectionCloseTypeString(
    QuicConnectionCloseType type);

// Indicate handshake state of a connection.
enum HandshakeState {
  // Initial state.
  HANDSHAKE_START,
  // Only used in IETF QUIC with TLS handshake. State proceeds to
  // HANDSHAKE_PROCESSED after a packet of HANDSHAKE packet number space
  // gets successfully processed, and the initial key can be dropped.
  HANDSHAKE_PROCESSED,
  // In QUIC crypto, state proceeds to HANDSHAKE_COMPLETE if client receives
  // SHLO or server successfully processes an ENCRYPTION_FORWARD_SECURE
  // packet, such that the handshake packets can be neutered. In IETF QUIC
  // with TLS handshake, state proceeds to HANDSHAKE_COMPLETE once the client
  // has both 1-RTT send and receive keys.
  HANDSHAKE_COMPLETE,
  // Only used in IETF QUIC with TLS handshake. State proceeds to
  // HANDSHAKE_CONFIRMED if 1) a client receives HANDSHAKE_DONE frame or
  // acknowledgment for 1-RTT packet or 2) server has
  // 1-RTT send and receive keys.
  HANDSHAKE_CONFIRMED,
};

struct QUIC_NO_EXPORT NextReleaseTimeResult {
  // The ideal release time of the packet being sent.
  QuicTime release_time;
  // Whether it is allowed to send the packet before release_time.
  bool allow_burst;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QUIC_TYPES_H_
