// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_test_packet_maker.h"

#include <list>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/third_party/quiche/src/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quiche/src/quic/core/http/http_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {
namespace {

quic::QuicFrames CloneFrames(const quic::QuicFrames& frames) {
  quic::QuicFrames new_frames = frames;
  for (auto& frame : new_frames) {
    switch (frame.type) {
      // Frames smaller than a pointer are inlined, so don't need to be cloned.
      case quic::PADDING_FRAME:
      case quic::MTU_DISCOVERY_FRAME:
      case quic::PING_FRAME:
      case quic::MAX_STREAMS_FRAME:
      case quic::STOP_WAITING_FRAME:
      case quic::STREAMS_BLOCKED_FRAME:
      case quic::STREAM_FRAME:
      case quic::HANDSHAKE_DONE_FRAME:
        break;
      case quic::ACK_FRAME:
        frame.ack_frame = new quic::QuicAckFrame(*frame.ack_frame);
        break;
      case quic::RST_STREAM_FRAME:
        frame.rst_stream_frame =
            new quic::QuicRstStreamFrame(*frame.rst_stream_frame);
        break;
      case quic::CONNECTION_CLOSE_FRAME:
        frame.connection_close_frame =
            new quic::QuicConnectionCloseFrame(*frame.connection_close_frame);
        break;
      case quic::GOAWAY_FRAME:
        frame.goaway_frame = new quic::QuicGoAwayFrame(*frame.goaway_frame);
        break;
      case quic::BLOCKED_FRAME:
        frame.blocked_frame = new quic::QuicBlockedFrame(*frame.blocked_frame);
        break;
      case quic::WINDOW_UPDATE_FRAME:
        frame.window_update_frame =
            new quic::QuicWindowUpdateFrame(*frame.window_update_frame);
        break;
      case quic::PATH_CHALLENGE_FRAME:
        frame.path_challenge_frame =
            new quic::QuicPathChallengeFrame(*frame.path_challenge_frame);
        break;
      case quic::STOP_SENDING_FRAME:
        frame.stop_sending_frame =
            new quic::QuicStopSendingFrame(*frame.stop_sending_frame);
        break;
      case quic::NEW_CONNECTION_ID_FRAME:
        frame.new_connection_id_frame =
            new quic::QuicNewConnectionIdFrame(*frame.new_connection_id_frame);
        break;
      case quic::RETIRE_CONNECTION_ID_FRAME:
        frame.retire_connection_id_frame =
            new quic::QuicRetireConnectionIdFrame(
                *frame.retire_connection_id_frame);
        break;
      case quic::PATH_RESPONSE_FRAME:
        frame.path_response_frame =
            new quic::QuicPathResponseFrame(*frame.path_response_frame);
        break;
      case quic::MESSAGE_FRAME:
        DCHECK(false) << "Message frame not supported";
        // frame.message_frame = new
        // quic::QuicMessageFrame(*frame.message_frame);
        break;
      case quic::CRYPTO_FRAME:
        frame.crypto_frame = new quic::QuicCryptoFrame(*frame.crypto_frame);
        break;
      case quic::NEW_TOKEN_FRAME:
        frame.new_token_frame =
            new quic::QuicNewTokenFrame(*frame.new_token_frame);
        break;

      case quic::NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot clone frame type: " << frame.type;
    }
  }
  return new_frames;
}

}  // namespace

QuicTestPacketMaker::QuicTestPacketMaker(
    quic::ParsedQuicVersion version,
    quic::QuicConnectionId connection_id,
    const quic::QuicClock* clock,
    const std::string& host,
    quic::Perspective perspective,
    bool client_headers_include_h2_stream_dependency)
    : version_(version),
      connection_id_(connection_id),
      clock_(clock),
      host_(host),
      max_allowed_push_id_(0),
      spdy_request_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      spdy_response_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      qpack_encoder_(&decoder_stream_error_delegate_),
      perspective_(perspective),
      encryption_level_(quic::ENCRYPTION_FORWARD_SECURE),
      long_header_type_(quic::INVALID_PACKET_TYPE),
      client_headers_include_h2_stream_dependency_(
          client_headers_include_h2_stream_dependency &&
          version.transport_version >= quic::QUIC_VERSION_43),
      save_packet_frames_(false) {
  DCHECK(!(perspective_ == quic::Perspective::IS_SERVER &&
           client_headers_include_h2_stream_dependency_));

  qpack_encoder_.set_qpack_stream_sender_delegate(
      &encoder_stream_sender_delegate_);
}

QuicTestPacketMaker::~QuicTestPacketMaker() {
  for (auto& kv : saved_frames_) {
    quic::DeleteFrames(&(kv.second));
  }
}

void QuicTestPacketMaker::set_hostname(const std::string& host) {
  host_.assign(host);
}

void QuicTestPacketMaker::set_max_allowed_push_id(quic::QuicStreamId push_id) {
  max_allowed_push_id_ = push_id;
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectivityProbingPacket(uint64_t num,
                                                   bool include_version) {
  InitializeHeader(num, include_version);

  if (!version_.HasIetfQuicFrames()) {
    AddQuicPingFrame();
  } else if (perspective_ == quic::Perspective::IS_CLIENT) {
    AddQuicPathChallengeFrame();
  } else {
    AddQuicPathResponseFrame();
  }

  AddQuicPaddingFrame();

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakePingPacket(
    uint64_t num,
    bool include_version) {
  InitializeHeader(num, include_version);
  AddQuicPingFrame();
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDummyCHLOPacket(uint64_t packet_num) {
  SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  InitializeHeader(packet_num, /*include_version=*/true);

  quic::CryptoHandshakeMessage message =
      MockCryptoClientStream::GetDummyCHLOMessage();
  const quic::QuicData& data = message.GetSerialized();

  if (!QuicVersionUsesCryptoFrames(version_.transport_version)) {
    AddQuicStreamFrameWithOffset(
        quic::QuicUtils::GetCryptoStreamId(version_.transport_version),
        /*fin=*/false, /*offset=*/0, data.AsStringPiece());
  } else {
    AddQuicCryptoFrame(quic::ENCRYPTION_INITIAL, 0, data.length());

    data_producer_ = std::make_unique<quic::test::SimpleDataProducer>();
    data_producer_->SaveCryptoData(quic::ENCRYPTION_INITIAL, 0,
                                   data.AsStringPiece());
  }
  AddQuicPaddingFrame();

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPingPacket(uint64_t num,
                                          bool include_version,
                                          uint64_t largest_received,
                                          uint64_t smallest_received,
                                          uint64_t least_unacked) {
  InitializeHeader(num, include_version);
  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicPingFrame();
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeStreamsBlockedPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  InitializeHeader(num, include_version);
  AddQuicStreamsBlockedFrame(1, stream_count, unidirectional);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeMaxStreamsPacket(uint64_t num,
                                          bool include_version,
                                          quic::QuicStreamCount stream_count,
                                          bool unidirectional) {
  InitializeHeader(num, include_version);
  AddQuicMaxStreamsFrame(1, stream_count, unidirectional);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  return MakeRstPacket(num, include_version, stream_id, error_code,
                       /*include_stop_sending_if_v99=*/true);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    bool include_stop_sending_if_v99) {
  InitializeHeader(num, include_version);

  if (!version_.HasIetfQuicFrames() ||
      quic::QuicUtils::IsBidirectionalStreamId(stream_id)) {
    AddQuicRstStreamFrame(stream_id, error_code);
  }

  if (include_stop_sending_if_v99 && version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAndDataPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code,
    quic::QuicStreamId data_stream_id,
    quiche::QuicheStringPiece data) {
  InitializeHeader(num, include_version);

  AddQuicRstStreamFrame(rst_stream_id, rst_error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, rst_error_code);
  }

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    quiche::QuicheStringPiece data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode rst_error_code) {
  InitializeHeader(num, include_version);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  AddQuicRstStreamFrame(rst_stream_id, rst_error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, rst_error_code);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked) {
  return MakeAckAndRstPacket(num, include_version, stream_id, error_code,
                             largest_received, smallest_received, least_unacked,
                             /*include_stop_sending_if_v99=*/true);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRstPacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    bool include_stop_sending_if_v99) {
  InitializeHeader(num, include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  if (!version_.HasIetfQuicFrames() ||
      quic::QuicUtils::IsBidirectionalStreamId(stream_id)) {
    AddQuicRstStreamFrame(stream_id, error_code);
  }

  if (version_.HasIetfQuicFrames() && include_stop_sending_if_v99) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  AddQuicRstStreamFrame(stream_id, error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRstAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  AddQuicRstStreamFrame(stream_id, error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }

  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataRstAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    quiche::QuicheStringPiece data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode error_code,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  AddQuicRstStreamFrame(rst_stream_id, error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, error_code);
  }

  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDataRstAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicStreamId data_stream_id,
    quiche::QuicheStringPiece data,
    quic::QuicStreamId rst_stream_id,
    quic::QuicRstStreamErrorCode error_code,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);

  AddQuicStreamFrame(data_stream_id, /* fin = */ false, data);
  AddQuicRstStreamFrame(rst_stream_id, error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(rst_stream_id, error_code);
  }

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndConnectionClosePacket(
    uint64_t num,
    bool include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  InitializeHeader(num, include_version);
  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details, frame_type);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeConnectionClosePacket(
    uint64_t num,
    bool include_version,
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  InitializeHeader(num, include_version);
  AddQuicConnectionCloseFrame(quic_error, quic_error_details);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeGoAwayPacket(
    uint64_t num,
    quic::QuicErrorCode error_code,
    std::string reason_phrase) {
  InitializeHeader(num, /*include_version=*/false);
  AddQuicGoAwayFrame(error_code, reason_phrase);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked) {
  return MakeAckPacket(packet_number, 1, largest_received, smallest_received,
                       least_unacked);
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeAckPacket(
    uint64_t packet_number,
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked) {
  InitializeHeader(packet_number, /*include_version=*/false);
  AddQuicAckFrame(first_received, largest_received, smallest_received);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::MakeDataPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    quiche::QuicheStringPiece data) {
  InitializeHeader(packet_number, should_include_version);
  AddQuicStreamFrame(stream_id, fin, data);
  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndDataPacket(uint64_t packet_number,
                                          bool include_version,
                                          quic::QuicStreamId stream_id,
                                          uint64_t largest_received,
                                          uint64_t smallest_received,
                                          uint64_t least_unacked,
                                          bool fin,
                                          quiche::QuicheStringPiece data) {
  InitializeHeader(packet_number, include_version);

  AddQuicAckFrame(largest_received, smallest_received);
  AddQuicStreamFrame(stream_id, fin, data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndMultipleDataFramesPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    const std::vector<std::string>& data_writes) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    MaybeAddHttp3SettingsFrames();

    if (priority != 1) {
      std::string priority_data =
          GenerateHttp3PriorityData(priority, stream_id);
      AddQuicStreamFrame(2, false, priority_data);
    }

    std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                          spdy_headers_frame_length);
    for (size_t i = 0; i < data_writes.size(); ++i) {
      data += data_writes[i];
    }
    AddQuicStreamFrame(stream_id, fin, data);

    return BuildPacket();
  }

  spdy::SpdySerializedFrame spdy_frame =
      MakeSpdyHeadersFrame(stream_id, fin && data_writes.empty(), priority,
                           std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  AddQuicStreamFrame(
      GetHeadersStreamId(), false,
      quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));

  for (size_t i = 0; i < data_writes.size(); ++i) {
    bool is_fin = fin && (i == data_writes.size() - 1);
    AddQuicStreamFrame(stream_id, is_fin,
                       quiche::QuicheStringPiece(data_writes[i]));
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    MaybeAddHttp3SettingsFrames();

    if (priority != 1) {
      std::string priority_data =
          GenerateHttp3PriorityData(priority, stream_id);
      AddQuicStreamFrame(2, false, priority_data);
    }

    std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                          spdy_headers_frame_length);
    AddQuicStreamFrame(stream_id, fin, data);

    return BuildPacket();
  }

  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length)
    *spdy_headers_frame_length = spdy_frame.size();
  AddQuicStreamFrame(
      GetHeadersStreamId(), false,
      quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndRstPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id,
    size_t* spdy_headers_frame_length,
    quic::QuicRstStreamErrorCode error_code) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    MaybeAddHttp3SettingsFrames();

    std::string priority_data = GenerateHttp3PriorityData(priority, stream_id);
    AddQuicStreamFrame(2, false, priority_data);

    std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                          spdy_headers_frame_length);
    AddQuicStreamFrame(stream_id, fin, data);
    AddQuicRstStreamFrame(stream_id, error_code);
    AddQuicStopSendingFrame(stream_id, error_code);

    return BuildPacket();
  }

  spdy::SpdySerializedFrame spdy_frame = MakeSpdyHeadersFrame(
      stream_id, fin, priority, std::move(headers), parent_stream_id);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  AddQuicStreamFrame(
      GetHeadersStreamId(), false,
      quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));

  AddQuicRstStreamFrame(stream_id, error_code);

  if (version_.HasIetfQuicFrames()) {
    AddQuicStopSendingFrame(stream_id, error_code);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePushPromisePacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    quic::QuicStreamId promised_stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    std::string encoded_headers =
        qpack_encoder_.EncodeHeaderList(stream_id, headers, nullptr);
    quic::PushPromiseFrame frame;
    frame.push_id = promised_stream_id;
    frame.headers = encoded_headers;
    std::unique_ptr<char[]> buffer;
    quic::QuicByteCount frame_length =
        quic::HttpEncoder::SerializePushPromiseFrameWithOnlyPushId(frame,
                                                                   &buffer);
    std::string push_promise_data(buffer.get(), frame_length);
    AddQuicStreamFrame(stream_id, false, push_promise_data);
    AddQuicStreamFrame(stream_id, false, encoded_headers);

    return BuildPacket();
  }

  spdy::SpdySerializedFrame spdy_frame;
  spdy::SpdyPushPromiseIR promise_frame(stream_id, promised_stream_id,
                                        std::move(headers));
  promise_frame.set_fin(fin);
  spdy_frame = spdy_request_framer_.SerializeFrame(promise_frame);
  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  AddQuicStreamFrame(
      GetHeadersStreamId(), false,
      quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool should_include_version,
    bool fin,
    spdy::SpdyHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  InitializeHeader(packet_number, should_include_version);

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                          spdy_headers_frame_length);

    AddQuicStreamFrame(stream_id, fin, data);

    return BuildPacket();
  }

  spdy::SpdySerializedFrame spdy_frame;
  spdy::SpdyHeadersIR headers_frame(stream_id, std::move(headers));
  headers_frame.set_fin(fin);
  spdy_frame = spdy_response_framer_.SerializeFrame(headers_frame);

  if (spdy_headers_frame_length) {
    *spdy_headers_frame_length = spdy_frame.size();
  }
  AddQuicStreamFrame(
      GetHeadersStreamId(), false,
      quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeInitialSettingsPacket(uint64_t packet_number) {
  InitializeHeader(packet_number, /*should_include_version*/ true);

  if (!quic::VersionUsesHttp3(version_.transport_version)) {
    spdy::SpdySettingsIR settings_frame;
    settings_frame.AddSetting(spdy::SETTINGS_MAX_HEADER_LIST_SIZE,
                              kQuicMaxHeaderListSize);
    settings_frame.AddSetting(quic::SETTINGS_QPACK_BLOCKED_STREAMS,
                              quic::kDefaultMaximumBlockedStreams);
    spdy::SpdySerializedFrame spdy_frame(
        spdy_request_framer_.SerializeFrame(settings_frame));
    AddQuicStreamFrame(
        GetHeadersStreamId(), false,
        quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));
    return BuildPacket();
  }

  MaybeAddHttp3SettingsFrames();

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePriorityPacket(uint64_t packet_number,
                                        bool should_include_version,
                                        quic::QuicStreamId id,
                                        quic::QuicStreamId parent_stream_id,
                                        spdy::SpdyPriority priority) {
  InitializeHeader(packet_number, should_include_version);

  if (!client_headers_include_h2_stream_dependency_) {
    parent_stream_id = 0;
  }
  int weight = spdy::Spdy3PriorityToHttp2Weight(priority);
  bool exclusive = client_headers_include_h2_stream_dependency_;

  if (!VersionUsesHttp3(version_.transport_version)) {
    spdy::SpdyPriorityIR priority_frame(id, parent_stream_id, weight,
                                        exclusive);
    spdy::SpdySerializedFrame spdy_frame(
        spdy_request_framer_.SerializeFrame(priority_frame));
    AddQuicStreamFrame(
        GetHeadersStreamId(), false,
        quiche::QuicheStringPiece(spdy_frame.data(), spdy_frame.size()));

    return BuildPacket();
  }
  if (priority != quic::QuicStream::kDefaultUrgency) {
    std::string priority_data = GenerateHttp3PriorityData(priority, id);
    AddQuicStreamFrame(2, false, priority_data);
  }

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndPriorityUpdatePacket(
    uint64_t packet_number,
    bool should_include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    quic::QuicStreamId id,
    spdy::SpdyPriority priority) {
  InitializeHeader(packet_number, should_include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  std::string priority_data = GenerateHttp3PriorityData(priority, id);
  AddQuicStreamFrame(2, false, priority_data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndMultiplePriorityFramesPacket(
    uint64_t packet_number,
    bool should_include_version,
    uint64_t largest_received,
    uint64_t smallest_received,
    uint64_t least_unacked,
    const std::vector<Http2StreamDependency>& priority_frames) {
  InitializeHeader(packet_number, should_include_version);

  AddQuicAckFrame(largest_received, smallest_received);

  const bool exclusive = client_headers_include_h2_stream_dependency_;
  std::string coalesced_data;
  for (const Http2StreamDependency& info : priority_frames) {
    spdy::SpdyPriorityIR priority_frame(
        info.stream_id, info.parent_stream_id,
        spdy::Spdy3PriorityToHttp2Weight(info.spdy_priority), exclusive);
    auto spdy_frame = spdy_request_framer_.SerializeFrame(priority_frame);
    coalesced_data += std::string(spdy_frame.data(), spdy_frame.size());
  }
  AddQuicStreamFrame(quic::VersionUsesHttp3(version_.transport_version)
                         ? GetFirstBidirectionalStreamId()
                         : GetHeadersStreamId(),
                     false, coalesced_data);

  return BuildPacket();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionPacket(uint64_t original_packet_number,
                                              uint64_t new_packet_number,
                                              bool should_include_version) {
  DCHECK(save_packet_frames_);
  InitializeHeader(new_packet_number, should_include_version);
  return BuildPacketImpl(
      saved_frames_[quic::QuicPacketNumber(original_packet_number)], nullptr);
}

void QuicTestPacketMaker::RemoveSavedStreamFrames(
    quic::QuicStreamId stream_id) {
  for (auto& kv : saved_frames_) {
    auto it = kv.second.begin();
    while (it != kv.second.end()) {
      if (it->type == quic::STREAM_FRAME &&
          it->stream_frame.stream_id == stream_id) {
        it = kv.second.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void QuicTestPacketMaker::SetEncryptionLevel(quic::EncryptionLevel level) {
  encryption_level_ = level;
  switch (level) {
    case quic::ENCRYPTION_INITIAL:
      long_header_type_ = quic::INITIAL;
      break;
    case quic::ENCRYPTION_ZERO_RTT:
      long_header_type_ = quic::ZERO_RTT_PROTECTED;
      break;
    case quic::ENCRYPTION_FORWARD_SECURE:
      long_header_type_ = quic::INVALID_PACKET_TYPE;
      break;
    default:
      QUIC_BUG << quic::EncryptionLevelToString(level);
      long_header_type_ = quic::INVALID_PACKET_TYPE;
  }
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::GetRequestHeaders(
    const std::string& method,
    const std::string& scheme,
    const std::string& path) const {
  spdy::SpdyHeaderBlock headers;
  headers[":method"] = method;
  headers[":authority"] = host_;
  headers[":scheme"] = scheme;
  headers[":path"] = path;
  return headers;
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::ConnectRequestHeaders(
    const std::string& host_port) const {
  spdy::SpdyHeaderBlock headers;
  headers[":method"] = "CONNECT";
  headers[":authority"] = host_port;
  return headers;
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status) const {
  spdy::SpdyHeaderBlock headers;
  headers[":status"] = status;
  headers["content-type"] = "text/plain";
  return headers;
}

spdy::SpdyHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status,
    const std::string& alt_svc) const {
  spdy::SpdyHeaderBlock headers;
  headers[":status"] = status;
  headers["alt-svc"] = alt_svc;
  headers["content-type"] = "text/plain";
  return headers;
}

void QuicTestPacketMaker::Reset() {
  stream_offsets_.clear();
}

std::string QuicTestPacketMaker::QpackEncodeHeaders(
    quic::QuicStreamId stream_id,
    spdy::SpdyHeaderBlock headers,
    size_t* encoded_data_length) {
  DCHECK(quic::VersionUsesHttp3(version_.transport_version));
  std::string data;

  std::string encoded_headers =
      qpack_encoder_.EncodeHeaderList(stream_id, headers, nullptr);

  // Generate HEADERS frame header.
  std::unique_ptr<char[]> headers_frame_header;
  const size_t headers_frame_header_length =
      quic::HttpEncoder::SerializeHeadersFrameHeader(encoded_headers.size(),
                                                     &headers_frame_header);

  // Possible add a PUSH stream type.
  if (!quic::QuicUtils::IsBidirectionalStreamId(stream_id) &&
      stream_offsets_[stream_id] == 0) {
    // Push stream type header
    data += "\x01";
  }

  // Add the HEADERS frame header.
  data += std::string(headers_frame_header.get(), headers_frame_header_length);
  // Add the HEADERS frame payload.
  data += encoded_headers;

  // Compute the total data length.
  if (encoded_data_length) {
    *encoded_data_length = data.length();
  }
  return data;
}

void QuicTestPacketMaker::InitializeHeader(uint64_t packet_number,
                                           bool should_include_version) {
  header_.destination_connection_id = DestinationConnectionId();
  header_.destination_connection_id_included = HasDestinationConnectionId();
  header_.source_connection_id = SourceConnectionId();
  header_.source_connection_id_included = HasSourceConnectionId();
  header_.reset_flag = false;
  header_.version_flag = ShouldIncludeVersion(should_include_version);
  if (quic::VersionHasIetfInvariantHeader(version_.transport_version)) {
    header_.form = header_.version_flag ? quic::IETF_QUIC_LONG_HEADER_PACKET
                                        : quic::IETF_QUIC_SHORT_HEADER_PACKET;
  }
  header_.long_packet_type = long_header_type_;
  header_.packet_number_length = GetPacketNumberLength();
  header_.packet_number = quic::QuicPacketNumber(packet_number);
  if (quic::QuicVersionHasLongHeaderLengths(version_.transport_version) &&
      header_.version_flag) {
    if (long_header_type_ == quic::INITIAL) {
      header_.retry_token_length_length =
          quic::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    }
    header_.length_length = quic::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }
}

void QuicTestPacketMaker::AddQuicPaddingFrame() {
  quic::QuicPaddingFrame padding_frame;
  frames_.push_back(quic::QuicFrame(padding_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicPingFrame() {
  quic::QuicPingFrame ping_frame;
  frames_.push_back(quic::QuicFrame(ping_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicMaxStreamsFrame(
    quic::QuicControlFrameId control_frame_id,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  quic::QuicMaxStreamsFrame max_streams_frame(control_frame_id, stream_count,
                                              unidirectional);
  frames_.push_back(quic::QuicFrame(max_streams_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicStreamsBlockedFrame(
    quic::QuicControlFrameId control_frame_id,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  quic::QuicStreamsBlockedFrame streams_blocked_frame(
      control_frame_id, stream_count, unidirectional);
  frames_.push_back(quic::QuicFrame(streams_blocked_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicStreamFrame(quic::QuicStreamId stream_id,
                                             bool fin,
                                             quiche::QuicheStringPiece data) {
  AddQuicStreamFrameWithOffset(stream_id, fin, stream_offsets_[stream_id],
                               data);
  stream_offsets_[stream_id] += data.length();
}

void QuicTestPacketMaker::AddQuicStreamFrameWithOffset(
    quic::QuicStreamId stream_id,
    bool fin,
    quic::QuicStreamOffset offset,
    quiche::QuicheStringPiece data) {
  // Save the stream data so that callers can use temporary objects for data.
  saved_stream_data_.push_back(std::make_unique<std::string>(data));
  quiche::QuicheStringPiece saved_data = *saved_stream_data_.back();

  quic::QuicStreamFrame stream_frame(stream_id, fin, offset, saved_data);
  frames_.push_back(quic::QuicFrame(stream_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicAckFrame(uint64_t largest_received,
                                          uint64_t smallest_received) {
  AddQuicAckFrame(1, largest_received, smallest_received);
}

void QuicTestPacketMaker::AddQuicAckFrame(uint64_t first_received,
                                          uint64_t largest_received,
                                          uint64_t smallest_received) {
  auto* ack_frame = new quic::QuicAckFrame;
  ack_frame->largest_acked = quic::QuicPacketNumber(largest_received);
  ack_frame->ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack_frame->received_packet_times.push_back(
        std::make_pair(quic::QuicPacketNumber(i), clock_->Now()));
  }
  if (largest_received > 0) {
    DCHECK_GE(largest_received, first_received);
    ack_frame->packets.AddRange(quic::QuicPacketNumber(first_received),
                                quic::QuicPacketNumber(largest_received + 1));
  }
  frames_.push_back(quic::QuicFrame(ack_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicRstStreamFrame(
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  auto* rst_stream_frame = new quic::QuicRstStreamFrame(
      1, stream_id, error_code, stream_offsets_[stream_id]);
  frames_.push_back(quic::QuicFrame(rst_stream_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicConnectionCloseFrame(
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details) {
  AddQuicConnectionCloseFrame(quic_error, quic_error_details, 0);
}

void QuicTestPacketMaker::AddQuicConnectionCloseFrame(
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  auto* close_frame = new quic::QuicConnectionCloseFrame(
      version_.transport_version, quic_error, quic_error_details, frame_type);
  frames_.push_back(quic::QuicFrame(close_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicGoAwayFrame(quic::QuicErrorCode error_code,
                                             std::string reason_phrase) {
  auto* goaway_frame = new quic::QuicGoAwayFrame();
  goaway_frame->error_code = error_code;
  goaway_frame->last_good_stream_id = 0;
  goaway_frame->reason_phrase = reason_phrase;
  frames_.push_back(quic::QuicFrame(goaway_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicPathResponseFrame() {
  quic::test::MockRandom rand(0);
  quic::QuicPathFrameBuffer payload;
  rand.RandBytes(payload.data(), payload.size());
  auto* path_response_frame = new quic::QuicPathResponseFrame(0, payload);
  frames_.push_back(quic::QuicFrame(path_response_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicPathChallengeFrame() {
  quic::test::MockRandom rand(0);
  quic::QuicPathFrameBuffer payload;
  rand.RandBytes(payload.data(), payload.size());
  auto* path_challenge_frame = new quic::QuicPathChallengeFrame(0, payload);
  frames_.push_back(quic::QuicFrame(path_challenge_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicStopSendingFrame(
    quic::QuicStreamId stream_id,
    quic::QuicApplicationErrorCode error_code) {
  auto* stop_sending_frame =
      new quic::QuicStopSendingFrame(1, stream_id, error_code);
  frames_.push_back(quic::QuicFrame(stop_sending_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

void QuicTestPacketMaker::AddQuicCryptoFrame(
    quic::EncryptionLevel level,
    quic::QuicStreamOffset offset,
    quic::QuicPacketLength data_length) {
  auto* crypto_frame = new quic::QuicCryptoFrame(level, offset, data_length);
  frames_.push_back(quic::QuicFrame(crypto_frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::BuildPacket() {
  auto packet = BuildPacketImpl(frames_, data_producer_.get());

  DeleteFrames(&frames_);
  data_producer_.reset();

  return packet;
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::BuildPacketImpl(
    const quic::QuicFrames& frames,
    quic::QuicStreamFrameDataProducer* data_producer) {
  quic::QuicFramer framer(quic::test::SupportedVersions(version_),
                          clock_->Now(), perspective_,
                          quic::kQuicDefaultConnectionIdLength);
  if (encryption_level_ == quic::ENCRYPTION_INITIAL) {
    framer.SetInitialObfuscators(perspective_ == quic::Perspective::IS_CLIENT
                                     ? header_.destination_connection_id
                                     : header_.source_connection_id);
  } else {
    framer.SetEncrypter(encryption_level_,
                        std::make_unique<quic::NullEncrypter>(perspective_));
  }
  if (data_producer != nullptr) {
    framer.set_data_producer(data_producer);
  }
  quic::QuicFrames frames_copy = CloneFrames(frames);
  size_t max_plaintext_size =
      framer.GetMaxPlaintextSize(quic::kDefaultMaxPacketSize);
  if (version_.HasHeaderProtection()) {
    size_t packet_size =
        quic::GetPacketHeaderSize(version_.transport_version, header_);
    size_t frames_size = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
      bool first_frame = i == 0;
      bool last_frame = i == frames.size() - 1;
      const size_t frame_size = framer.GetSerializedFrameLength(
          frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
          header_.packet_number_length);
      packet_size += frame_size;
      frames_size += frame_size;
    }
    // This should be done by calling QuicPacketCreator::MinPlaintextPacketSize.
    const size_t min_plaintext_size = 7;
    if (frames_size < min_plaintext_size) {
      size_t padding_length = min_plaintext_size - frames_size;
      frames_copy.push_back(
          quic::QuicFrame(quic::QuicPaddingFrame(padding_length)));
    }
  }
  std::unique_ptr<quic::QuicPacket> packet(quic::test::BuildUnsizedDataPacket(
      &framer, header_, frames_copy, max_plaintext_size));
  char buffer[quic::kMaxOutgoingPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(encryption_level_, header_.packet_number, *packet,
                            buffer, quic::kMaxOutgoingPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false);
  if (save_packet_frames_) {
    saved_frames_[header_.packet_number] = frames_copy;
  } else {
    saved_stream_data_.clear();
    DeleteFrames(&frames_copy);
  }

  return encrypted.Clone();
}

spdy::SpdySerializedFrame QuicTestPacketMaker::MakeSpdyHeadersFrame(
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority priority,
    spdy::SpdyHeaderBlock headers,
    quic::QuicStreamId parent_stream_id) {
  spdy::SpdyHeadersIR headers_frame(stream_id, std::move(headers));
  headers_frame.set_fin(fin);
  headers_frame.set_weight(spdy::Spdy3PriorityToHttp2Weight(priority));
  headers_frame.set_has_priority(true);

  if (client_headers_include_h2_stream_dependency_) {
    headers_frame.set_parent_stream_id(parent_stream_id);
    headers_frame.set_exclusive(true);
  } else {
    headers_frame.set_parent_stream_id(0);
    headers_frame.set_exclusive(false);
  }

  return spdy_request_framer_.SerializeFrame(headers_frame);
}

bool QuicTestPacketMaker::ShouldIncludeVersion(bool include_version) const {
  if (version_.transport_version > quic::QUIC_VERSION_43) {
    return encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE;
  }
  return include_version;
}

quic::QuicPacketNumberLength QuicTestPacketMaker::GetPacketNumberLength()
    const {
  if (version_.transport_version > quic::QUIC_VERSION_43 &&
      encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE &&
      !version_.SendsVariableLengthPacketNumberInLongHeader()) {
    return quic::PACKET_4BYTE_PACKET_NUMBER;
  }
  return quic::PACKET_1BYTE_PACKET_NUMBER;
}

quic::QuicConnectionId QuicTestPacketMaker::DestinationConnectionId() const {
  if (perspective_ == quic::Perspective::IS_SERVER) {
    return quic::EmptyQuicConnectionId();
  }
  return connection_id_;
}

quic::QuicConnectionId QuicTestPacketMaker::SourceConnectionId() const {
  if (perspective_ == quic::Perspective::IS_CLIENT) {
    return quic::EmptyQuicConnectionId();
  }
  return connection_id_;
}

quic::QuicConnectionIdIncluded QuicTestPacketMaker::HasDestinationConnectionId()
    const {
  if (!version_.SupportsClientConnectionIds() &&
      perspective_ == quic::Perspective::IS_SERVER) {
    return quic::CONNECTION_ID_ABSENT;
  }
  return quic::CONNECTION_ID_PRESENT;
}

quic::QuicConnectionIdIncluded QuicTestPacketMaker::HasSourceConnectionId()
    const {
  if (version_.SupportsClientConnectionIds() ||
      (perspective_ == quic::Perspective::IS_SERVER &&
       encryption_level_ < quic::ENCRYPTION_FORWARD_SECURE)) {
    return quic::CONNECTION_ID_PRESENT;
  }
  return quic::CONNECTION_ID_ABSENT;
}

quic::QuicStreamId QuicTestPacketMaker::GetFirstBidirectionalStreamId() const {
  return quic::QuicUtils::GetFirstBidirectionalStreamId(
      version_.transport_version, perspective_);
}

quic::QuicStreamId QuicTestPacketMaker::GetHeadersStreamId() const {
  return quic::QuicUtils::GetHeadersStreamId(version_.transport_version);
}

std::string QuicTestPacketMaker::GenerateHttp3SettingsData() {
  quic::SettingsFrame settings;
  settings.values[quic::SETTINGS_MAX_HEADER_LIST_SIZE] = kQuicMaxHeaderListSize;
  settings.values[quic::SETTINGS_QPACK_MAX_TABLE_CAPACITY] =
      quic::kDefaultQpackMaxDynamicTableCapacity;
  settings.values[quic::SETTINGS_QPACK_BLOCKED_STREAMS] =
      quic::kDefaultMaximumBlockedStreams;
  // Greased setting.
  settings.values[0x40] = 20;
  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializeSettingsFrame(settings, &buffer);
  return std::string(buffer.get(), frame_length);
}

std::string QuicTestPacketMaker::GenerateHttp3MaxPushIdData() {
  quic::MaxPushIdFrame max_push_id;
  max_push_id.push_id = max_allowed_push_id_;
  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializeMaxPushIdFrame(max_push_id, &buffer);
  return std::string(buffer.get(), frame_length);
}

std::string QuicTestPacketMaker::GenerateHttp3PriorityData(
    spdy::SpdyPriority priority,
    quic::QuicStreamId stream_id) {
  quic::PriorityUpdateFrame priority_update;
  priority_update.prioritized_element_type = quic::REQUEST_STREAM;
  priority_update.prioritized_element_id = stream_id;
  priority_update.priority_field_value =
      quiche::QuicheStrCat("u=", static_cast<int>(priority));
  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializePriorityUpdateFrame(priority_update, &buffer);
  return std::string(buffer.get(), frame_length);
}

std::string QuicTestPacketMaker::GenerateHttp3GreaseData() {
  std::unique_ptr<char[]> buffer;
  quic::QuicByteCount frame_length =
      quic::HttpEncoder::SerializeGreasingFrame(&buffer);
  return std::string(buffer.get(), frame_length);
}

void QuicTestPacketMaker::MaybeAddHttp3SettingsFrames() {
  DCHECK(quic::VersionUsesHttp3(version_.transport_version));

  quic::QuicStreamId stream_id =
      quic::QuicUtils::GetFirstUnidirectionalStreamId(
          version_.transport_version, perspective_);

  if (stream_offsets_[stream_id] != 0)
    return;

  // A stream frame containing stream type will be written on the control
  // stream first.
  std::string type(1, 0x00);
  std::string settings_data = GenerateHttp3SettingsData();
  std::string grease_data = GenerateHttp3GreaseData();
  std::string max_push_id_data = GenerateHttp3MaxPushIdData();

  // The type and the SETTINGS frame may be sent in multiple QUIC STREAM
  // frames.
  std::string data = type + settings_data + grease_data + max_push_id_data;

  AddQuicStreamFrame(stream_id, false, data);
}

}  // namespace test
}  // namespace net
