// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a simple interface for QUIC tests to create a variety of packets.

#ifndef NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
#define NET_QUIC_QUIC_TEST_PACKET_MAKER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "net/third_party/quiche/src/quic/core/http/http_encoder.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder.h"
#include "net/third_party/quiche/src/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_stream_frame_data_producer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quic/test_tools/qpack/qpack_encoder_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/simple_data_producer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace net {
namespace test {

class QuicTestPacketMaker {
 public:
  struct Http2StreamDependency {
    quic::QuicStreamId stream_id;
    quic::QuicStreamId parent_stream_id;
    spdy::SpdyPriority spdy_priority;
  };

  // |client_headers_include_h2_stream_dependency| affects the output of
  // the MakeRequestHeaders...() methods. If its value is true, then request
  // headers are constructed with the exclusive flag set to true and the parent
  // stream id set to the |parent_stream_id| param of MakeRequestHeaders...().
  // Otherwise, headers are constructed with the exclusive flag set to false and
  // the parent stream ID set to 0 (ignoring the |parent_stream_id| param).
  QuicTestPacketMaker(quic::ParsedQuicVersion version,
                      quic::QuicConnectionId connection_id,
                      const quic::QuicClock* clock,
                      const std::string& host,
                      quic::Perspective perspective,
                      bool client_headers_include_h2_stream_dependency);
  ~QuicTestPacketMaker();

  void set_hostname(const std::string& host);
  void set_max_allowed_push_id(quic::QuicStreamId push_id);

  std::unique_ptr<quic::QuicReceivedPacket> MakeConnectivityProbingPacket(
      uint64_t num,
      bool include_version);

  std::unique_ptr<quic::QuicReceivedPacket> MakePingPacket(
      uint64_t num,
      bool include_version);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDummyCHLOPacket(
      uint64_t packet_num);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndPingPacket(
      uint64_t num,
      bool include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked);

  std::unique_ptr<quic::QuicReceivedPacket> MakeStreamsBlockedPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamCount stream_count,
      bool unidirectional);

  std::unique_ptr<quic::QuicReceivedPacket> MakeMaxStreamsPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamCount stream_count,
      bool unidirectional);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      bool include_stop_sending_if_v99);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAndDataPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code,
      quic::QuicStreamId data_stream_id,
      quiche::QuicheStringPiece data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataAndRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId data_stream_id,
      quiche::QuicheStringPiece data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode rst_error_code);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool include_stop_sending_if_v99);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAckAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRstAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataRstAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId data_stream_id,
      quiche::QuicheStringPiece data,
      quic::QuicStreamId rst_stream_id,
      quic::QuicRstStreamErrorCode error_code,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeDataRstAckAndConnectionClosePacket(
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
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndConnectionClosePacket(
      uint64_t num,
      bool include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type);

  std::unique_ptr<quic::QuicReceivedPacket> MakeConnectionClosePacket(
      uint64_t num,
      bool include_version,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details);

  std::unique_ptr<quic::QuicReceivedPacket> MakeGoAwayPacket(
      uint64_t num,
      quic::QuicErrorCode error_code,
      std::string reason_phrase);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckPacket(
      uint64_t packet_number,
      uint64_t first_received,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked);

  std::unique_ptr<quic::QuicReceivedPacket> MakeDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      quiche::QuicheStringPiece data);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndDataPacket(
      uint64_t packet_number,
      bool include_version,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool fin,
      quiche::QuicheStringPiece data);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeRequestHeadersAndMultipleDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRequestHeadersAndRstPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      quic::QuicRstStreamErrorCode error_code);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakePushPromisePacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      quic::QuicStreamId promised_stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<quic::QuicReceivedPacket> MakeResponseHeadersPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // Creates a packet containing the initial SETTINGS frame, and saves the
  // headers stream offset into |offset|.
  std::unique_ptr<quic::QuicReceivedPacket> MakeInitialSettingsPacket(
      uint64_t packet_number);

  std::unique_ptr<quic::QuicReceivedPacket> MakePriorityPacket(
      uint64_t packet_number,
      bool should_include_version,
      quic::QuicStreamId id,
      quic::QuicStreamId parent_stream_id,
      spdy::SpdyPriority priority);

  std::unique_ptr<quic::QuicReceivedPacket>
  MakeAckAndMultiplePriorityFramesPacket(
      uint64_t packet_number,
      bool should_include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      const std::vector<Http2StreamDependency>& priority_frames);

  std::unique_ptr<quic::QuicReceivedPacket> MakeRetransmissionPacket(
      uint64_t original_packet_number,
      uint64_t new_packet_number,
      bool should_include_version);

  std::unique_ptr<quic::QuicReceivedPacket> MakeAckAndPriorityUpdatePacket(
      uint64_t packet_number,
      bool should_include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      quic::QuicStreamId id,
      spdy::SpdyPriority priority);

  // Removes all stream frames associated with |stream_id|.
  void RemoveSavedStreamFrames(quic::QuicStreamId stream_id);

  void SetEncryptionLevel(quic::EncryptionLevel level);

  spdy::SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                          const std::string& scheme,
                                          const std::string& path) const;

  spdy::SpdyHeaderBlock ConnectRequestHeaders(
      const std::string& host_port) const;

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status) const;

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status,
                                           const std::string& alt_svc) const;

  spdy::SpdyFramer* spdy_request_framer() { return &spdy_request_framer_; }
  spdy::SpdyFramer* spdy_response_framer() { return &spdy_response_framer_; }

  void Reset();

  quic::QuicStreamOffset stream_offset(quic::QuicStreamId stream_id) {
    return stream_offsets_[stream_id];
  }

  void set_save_packet_frames(bool save_packet_frames) {
    save_packet_frames_ = save_packet_frames;
  }

  std::string QpackEncodeHeaders(quic::QuicStreamId stream_id,
                                 spdy::SpdyHeaderBlock headers,
                                 size_t* encoded_data_length);

 private:
  // Initialize header of next packet to build.
  void InitializeHeader(uint64_t packet_number, bool should_include_version);

  // Add frames to current packet.
  void AddQuicPaddingFrame();
  void AddQuicPingFrame();
  void AddQuicMaxStreamsFrame(quic::QuicControlFrameId control_frame_id,
                              quic::QuicStreamCount stream_count,
                              bool unidirectional);
  void AddQuicStreamsBlockedFrame(quic::QuicControlFrameId control_frame_id,
                                  quic::QuicStreamCount stream_count,
                                  bool unidirectional);
  // Use and increase stream's current offset.
  void AddQuicStreamFrame(quic::QuicStreamId stream_id,
                          bool fin,
                          quiche::QuicheStringPiece data);
  // Use |offset| and do not change stream's current offset.
  void AddQuicStreamFrameWithOffset(quic::QuicStreamId stream_id,
                                    bool fin,
                                    quic::QuicStreamOffset offset,
                                    quiche::QuicheStringPiece data);
  void AddQuicAckFrame(uint64_t largest_received, uint64_t smallest_received);
  void AddQuicAckFrame(uint64_t first_received,
                       uint64_t largest_received,
                       uint64_t smallest_received);
  void AddQuicRstStreamFrame(quic::QuicStreamId stream_id,
                             quic::QuicRstStreamErrorCode error_code);
  void AddQuicConnectionCloseFrame(quic::QuicErrorCode quic_error,
                                   const std::string& quic_error_details);
  void AddQuicConnectionCloseFrame(quic::QuicErrorCode quic_error,
                                   const std::string& quic_error_details,
                                   uint64_t frame_type);
  void AddQuicGoAwayFrame(quic::QuicErrorCode error_code,
                          std::string reason_phrase);
  void AddQuicPathResponseFrame();
  void AddQuicPathChallengeFrame();
  void AddQuicStopSendingFrame(quic::QuicStreamId stream_id,
                               quic::QuicApplicationErrorCode error_code);
  void AddQuicCryptoFrame(quic::EncryptionLevel level,
                          quic::QuicStreamOffset offset,
                          quic::QuicPacketLength data_length);

  // Build packet using |header_|, |frames_|, and |data_producer_|,
  // and clear |frames_| and |data_producer_| afterwards.
  std::unique_ptr<quic::QuicReceivedPacket> BuildPacket();

  // Build packet using |header_|, |frames|, and |data_producer|.
  std::unique_ptr<quic::QuicReceivedPacket> BuildPacketImpl(
      const quic::QuicFrames& frames,
      quic::QuicStreamFrameDataProducer* data_producer);

  spdy::SpdySerializedFrame MakeSpdyHeadersFrame(
      quic::QuicStreamId stream_id,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id);

  bool ShouldIncludeVersion(bool include_version) const;

  quic::QuicPacketNumberLength GetPacketNumberLength() const;

  quic::QuicConnectionId DestinationConnectionId() const;
  quic::QuicConnectionId SourceConnectionId() const;

  quic::QuicConnectionIdIncluded HasDestinationConnectionId() const;
  quic::QuicConnectionIdIncluded HasSourceConnectionId() const;

  quic::QuicStreamId GetFirstBidirectionalStreamId() const;
  quic::QuicStreamId GetHeadersStreamId() const;

  std::string GenerateHttp3SettingsData();
  std::string GenerateHttp3MaxPushIdData();
  std::string GenerateHttp3PriorityData(spdy::SpdyPriority priority,
                                        quic::QuicStreamId stream_id);
  std::string GenerateHttp3GreaseData();

  void MaybeAddHttp3SettingsFrames();

  // Parameters used throughout the lifetime of the class.
  quic::ParsedQuicVersion version_;
  quic::QuicConnectionId connection_id_;
  const quic::QuicClock* clock_;  // Not owned.
  std::string host_;
  quic::QuicStreamId max_allowed_push_id_;
  spdy::SpdyFramer spdy_request_framer_;
  spdy::SpdyFramer spdy_response_framer_;
  quic::test::NoopDecoderStreamErrorDelegate decoder_stream_error_delegate_;
  quic::test::NoopQpackStreamSenderDelegate encoder_stream_sender_delegate_;
  quic::QpackEncoder qpack_encoder_;
  quic::test::MockRandom random_generator_;
  std::map<quic::QuicStreamId, quic::QuicStreamOffset> stream_offsets_;
  quic::Perspective perspective_;
  quic::EncryptionLevel encryption_level_;
  quic::QuicLongHeaderType long_header_type_;
  // If true, generated request headers will include non-default HTTP2 stream
  // dependency info.
  bool client_headers_include_h2_stream_dependency_;

  // Save a copy of stream frame data that QuicStreamFrame objects can refer to.
  std::vector<std::unique_ptr<std::string>> saved_stream_data_;
  // If |save_packet_frames_| is true, save generated packets in
  // |saved_frames_|, allowing retransmission packets to be built.
  bool save_packet_frames_;
  std::map<quic::QuicPacketNumber, quic::QuicFrames> saved_frames_;

  // State necessary for building the current packet.
  quic::QuicPacketHeader header_;
  quic::QuicFrames frames_;
  std::unique_ptr<quic::test::SimpleDataProducer> data_producer_;

  DISALLOW_COPY_AND_ASSIGN(QuicTestPacketMaker);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_TEST_PACKET_MAKER_H_
