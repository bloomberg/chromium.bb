// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_crypto_stream.h"

#include <string>

#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_optional.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace quic {

#define ENDPOINT                                                   \
  (session()->perspective() == Perspective::IS_SERVER ? "Server: " \
                                                      : "Client:"  \
                                                        " ")

QuicCryptoStream::QuicCryptoStream(QuicSession* session)
    : QuicStream(
          QuicVersionUsesCryptoFrames(session->transport_version())
              ? QuicUtils::GetInvalidStreamId(session->transport_version())
              : QuicUtils::GetCryptoStreamId(session->transport_version()),
          session,
          /*is_static=*/true,
          QuicVersionUsesCryptoFrames(session->transport_version())
              ? CRYPTO
              : BIDIRECTIONAL),
      substreams_{{{this, ENCRYPTION_INITIAL},
                   {this, ENCRYPTION_HANDSHAKE},
                   {this, ENCRYPTION_ZERO_RTT},
                   {this, ENCRYPTION_FORWARD_SECURE}}} {
  // The crypto stream is exempt from connection level flow control.
  DisableConnectionFlowControlForThisStream();
}

QuicCryptoStream::~QuicCryptoStream() {}

// static
QuicByteCount QuicCryptoStream::CryptoMessageFramingOverhead(
    QuicTransportVersion version,
    QuicConnectionId connection_id) {
  DCHECK(QuicUtils::IsConnectionIdValidForVersion(connection_id, version));
  QuicVariableLengthIntegerLength retry_token_length_length =
      VARIABLE_LENGTH_INTEGER_LENGTH_1;
  QuicVariableLengthIntegerLength length_length =
      VARIABLE_LENGTH_INTEGER_LENGTH_2;
  if (!QuicVersionHasLongHeaderLengths(version)) {
    retry_token_length_length = VARIABLE_LENGTH_INTEGER_LENGTH_0;
    length_length = VARIABLE_LENGTH_INTEGER_LENGTH_0;
  }
  return QuicPacketCreator::StreamFramePacketOverhead(
      version, static_cast<QuicConnectionIdLength>(connection_id.length()),
      PACKET_0BYTE_CONNECTION_ID,
      /*include_version=*/true,
      /*include_diversification_nonce=*/true,
      VersionHasIetfInvariantHeader(version) ? PACKET_4BYTE_PACKET_NUMBER
                                             : PACKET_1BYTE_PACKET_NUMBER,
      retry_token_length_length, length_length,
      /*offset=*/0);
}

void QuicCryptoStream::OnCryptoFrame(const QuicCryptoFrame& frame) {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 shouldn't receive CRYPTO frames";
  EncryptionLevel level = session()->connection()->last_decrypted_level();
  substreams_[level].sequencer.OnCryptoFrame(frame);
  EncryptionLevel frame_level = level;
  if (substreams_[level].sequencer.NumBytesBuffered() >
      BufferSizeLimitForLevel(frame_level)) {
    OnUnrecoverableError(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA,
                         "Too much crypto data received");
  }
}

void QuicCryptoStream::OnStreamFrame(const QuicStreamFrame& frame) {
  if (QuicVersionUsesCryptoFrames(session()->transport_version())) {
    QUIC_PEER_BUG
        << "Crypto data received in stream frame instead of crypto frame";
    OnUnrecoverableError(QUIC_INVALID_STREAM_DATA, "Unexpected stream frame");
  }
  QuicStream::OnStreamFrame(frame);
}

void QuicCryptoStream::OnDataAvailable() {
  EncryptionLevel level = session()->connection()->last_decrypted_level();
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    // Versions less than 47 only support QUIC crypto, which ignores the
    // EncryptionLevel passed into CryptoMessageParser::ProcessInput (and
    // OnDataAvailableInSequencer).
    OnDataAvailableInSequencer(sequencer(), level);
    return;
  }
  OnDataAvailableInSequencer(&substreams_[level].sequencer, level);
}

void QuicCryptoStream::OnDataAvailableInSequencer(
    QuicStreamSequencer* sequencer,
    EncryptionLevel level) {
  struct iovec iov;
  while (sequencer->GetReadableRegion(&iov)) {
    quiche::QuicheStringPiece data(static_cast<char*>(iov.iov_base),
                                   iov.iov_len);
    if (!crypto_message_parser()->ProcessInput(data, level)) {
      OnUnrecoverableError(crypto_message_parser()->error(),
                           crypto_message_parser()->error_detail());
      return;
    }
    sequencer->MarkConsumed(iov.iov_len);
    if (one_rtt_keys_available() &&
        crypto_message_parser()->InputBytesRemaining() == 0) {
      // If the handshake is complete and the current message has been fully
      // processed then no more handshake messages are likely to arrive soon
      // so release the memory in the stream sequencer.
      sequencer->ReleaseBufferIfEmpty();
    }
  }
}

bool QuicCryptoStream::ExportKeyingMaterial(quiche::QuicheStringPiece label,
                                            quiche::QuicheStringPiece context,
                                            size_t result_len,
                                            std::string* result) const {
  if (!one_rtt_keys_available()) {
    QUIC_DLOG(ERROR) << "ExportKeyingMaterial was called before forward-secure"
                     << "encryption was established.";
    return false;
  }
  return CryptoUtils::ExportKeyingMaterial(
      crypto_negotiated_params().subkey_secret, label, context, result_len,
      result);
}

void QuicCryptoStream::WriteCryptoData(EncryptionLevel level,
                                       quiche::QuicheStringPiece data) {
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    // The QUIC crypto handshake takes care of setting the appropriate
    // encryption level before writing data. Since that is the only handshake
    // supported in versions less than 47, |level| can be ignored here.
    WriteOrBufferData(data, /* fin */ false, /* ack_listener */ nullptr);
    return;
  }
  if (data.empty()) {
    QUIC_BUG << "Empty crypto data being written";
    return;
  }
  const bool had_buffered_data = HasBufferedCryptoFrames();
  // Append |data| to the send buffer for this encryption level.
  struct iovec iov(QuicUtils::MakeIovec(data));
  QuicStreamSendBuffer* send_buffer = &substreams_[level].send_buffer;
  QuicStreamOffset offset = send_buffer->stream_offset();
  send_buffer->SaveStreamData(&iov, /*iov_count=*/1, /*iov_offset=*/0,
                              data.length());
  if (kMaxStreamLength - offset < data.length()) {
    QUIC_BUG << "Writing too much crypto handshake data";
    // TODO(nharper): Switch this to an IETF QUIC error code, possibly
    // INTERNAL_ERROR?
    OnUnrecoverableError(QUIC_STREAM_LENGTH_OVERFLOW,
                         "Writing too much crypto handshake data");
  }
  if (had_buffered_data) {
    // Do not try to write if there is buffered data.
    return;
  }

  size_t bytes_consumed = stream_delegate()->SendCryptoData(
      level, data.length(), offset, NOT_RETRANSMISSION);
  send_buffer->OnStreamDataConsumed(bytes_consumed);
}

size_t QuicCryptoStream::BufferSizeLimitForLevel(EncryptionLevel) const {
  return GetQuicFlag(FLAGS_quic_max_buffered_crypto_bytes);
}

bool QuicCryptoStream::OnCryptoFrameAcked(const QuicCryptoFrame& frame,
                                          QuicTime::Delta /*ack_delay_time*/) {
  QuicByteCount newly_acked_length = 0;
  if (!substreams_[frame.level].send_buffer.OnStreamDataAcked(
          frame.offset, frame.data_length, &newly_acked_length)) {
    OnUnrecoverableError(QUIC_INTERNAL_ERROR,
                         "Trying to ack unsent crypto data.");
    return false;
  }
  return newly_acked_length > 0;
}

void QuicCryptoStream::OnStreamReset(const QuicRstStreamFrame& /*frame*/) {
  stream_delegate()->OnStreamError(QUIC_INVALID_STREAM_ID,
                                   "Attempt to reset crypto stream");
}

void QuicCryptoStream::NeuterUnencryptedStreamData() {
  NeuterStreamDataOfEncryptionLevel(ENCRYPTION_INITIAL);
}

void QuicCryptoStream::NeuterStreamDataOfEncryptionLevel(
    EncryptionLevel level) {
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    for (const auto& interval : bytes_consumed_[level]) {
      QuicByteCount newly_acked_length = 0;
      send_buffer().OnStreamDataAcked(
          interval.min(), interval.max() - interval.min(), &newly_acked_length);
    }
    return;
  }
  QuicStreamSendBuffer* send_buffer = &substreams_[level].send_buffer;
  // TODO(nharper): Consider adding a Clear() method to QuicStreamSendBuffer to
  // replace the following code.
  QuicIntervalSet<QuicStreamOffset> to_ack = send_buffer->bytes_acked();
  to_ack.Complement(0, send_buffer->stream_offset());
  for (const auto& interval : to_ack) {
    QuicByteCount newly_acked_length = 0;
    send_buffer->OnStreamDataAcked(
        interval.min(), interval.max() - interval.min(), &newly_acked_length);
  }
}

void QuicCryptoStream::OnStreamDataConsumed(QuicByteCount bytes_consumed) {
  if (QuicVersionUsesCryptoFrames(session()->transport_version())) {
    QUIC_BUG << "Stream data consumed when CRYPTO frames should be in use";
  }
  if (bytes_consumed > 0) {
    bytes_consumed_[session()->connection()->encryption_level()].Add(
        stream_bytes_written(), stream_bytes_written() + bytes_consumed);
  }
  QuicStream::OnStreamDataConsumed(bytes_consumed);
}

namespace {

constexpr std::array<EncryptionLevel, NUM_ENCRYPTION_LEVELS>
AllEncryptionLevels() {
  return {ENCRYPTION_INITIAL, ENCRYPTION_HANDSHAKE, ENCRYPTION_ZERO_RTT,
          ENCRYPTION_FORWARD_SECURE};
}

}  // namespace

bool QuicCryptoStream::HasPendingCryptoRetransmission() const {
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    return false;
  }
  for (EncryptionLevel level : AllEncryptionLevels()) {
    if (substreams_[level].send_buffer.HasPendingRetransmission()) {
      return true;
    }
  }
  return false;
}

void QuicCryptoStream::WritePendingCryptoRetransmission() {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 don't write CRYPTO frames";
  for (EncryptionLevel level : AllEncryptionLevels()) {
    QuicStreamSendBuffer* send_buffer = &substreams_[level].send_buffer;
    while (send_buffer->HasPendingRetransmission()) {
      auto pending = send_buffer->NextPendingRetransmission();
      size_t bytes_consumed = stream_delegate()->SendCryptoData(
          level, pending.length, pending.offset, HANDSHAKE_RETRANSMISSION);
      send_buffer->OnStreamDataRetransmitted(pending.offset, bytes_consumed);
      if (bytes_consumed < pending.length) {
        break;
      }
    }
  }
}

void QuicCryptoStream::WritePendingRetransmission() {
  while (HasPendingRetransmission()) {
    StreamPendingRetransmission pending =
        send_buffer().NextPendingRetransmission();
    QuicIntervalSet<QuicStreamOffset> retransmission(
        pending.offset, pending.offset + pending.length);
    EncryptionLevel retransmission_encryption_level = ENCRYPTION_INITIAL;
    // Determine the encryption level to write the retransmission
    // at. The retransmission should be written at the same encryption level
    // as the original transmission.
    for (size_t i = 0; i < NUM_ENCRYPTION_LEVELS; ++i) {
      if (retransmission.Intersects(bytes_consumed_[i])) {
        retransmission_encryption_level = static_cast<EncryptionLevel>(i);
        retransmission.Intersection(bytes_consumed_[i]);
        break;
      }
    }
    pending.offset = retransmission.begin()->min();
    pending.length =
        retransmission.begin()->max() - retransmission.begin()->min();
    QuicConsumedData consumed = RetransmitStreamDataAtLevel(
        pending.offset, pending.length, retransmission_encryption_level,
        HANDSHAKE_RETRANSMISSION);
    if (consumed.bytes_consumed < pending.length) {
      // The connection is write blocked.
      break;
    }
  }
}

bool QuicCryptoStream::RetransmitStreamData(QuicStreamOffset offset,
                                            QuicByteCount data_length,
                                            bool /*fin*/,
                                            TransmissionType type) {
  DCHECK_EQ(HANDSHAKE_RETRANSMISSION, type);
  QuicIntervalSet<QuicStreamOffset> retransmission(offset,
                                                   offset + data_length);
  // Determine the encryption level to send data. This only needs to be once as
  // [offset, offset + data_length) is guaranteed to be in the same packet.
  EncryptionLevel send_encryption_level = ENCRYPTION_INITIAL;
  for (size_t i = 0; i < NUM_ENCRYPTION_LEVELS; ++i) {
    if (retransmission.Intersects(bytes_consumed_[i])) {
      send_encryption_level = static_cast<EncryptionLevel>(i);
      break;
    }
  }
  retransmission.Difference(bytes_acked());
  for (const auto& interval : retransmission) {
    QuicStreamOffset retransmission_offset = interval.min();
    QuicByteCount retransmission_length = interval.max() - interval.min();
    QuicConsumedData consumed = RetransmitStreamDataAtLevel(
        retransmission_offset, retransmission_length, send_encryption_level,
        type);
    if (consumed.bytes_consumed < retransmission_length) {
      // The connection is write blocked.
      return false;
    }
  }

  return true;
}

QuicConsumedData QuicCryptoStream::RetransmitStreamDataAtLevel(
    QuicStreamOffset retransmission_offset,
    QuicByteCount retransmission_length,
    EncryptionLevel encryption_level,
    TransmissionType type) {
  DCHECK_EQ(HANDSHAKE_RETRANSMISSION, type);
  const auto consumed = stream_delegate()->WritevData(
      id(), retransmission_length, retransmission_offset, NO_FIN, type,
      encryption_level);
  QUIC_DVLOG(1) << ENDPOINT << "stream " << id()
                << " is forced to retransmit stream data ["
                << retransmission_offset << ", "
                << retransmission_offset + retransmission_length
                << "), with encryption level: " << encryption_level
                << ", consumed: " << consumed;
  OnStreamFrameRetransmitted(retransmission_offset, consumed.bytes_consumed,
                             consumed.fin_consumed);

  return consumed;
}

uint64_t QuicCryptoStream::crypto_bytes_read() const {
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    return stream_bytes_read();
  }
  uint64_t bytes_read = 0;
  for (EncryptionLevel level : AllEncryptionLevels()) {
    bytes_read += substreams_[level].sequencer.NumBytesConsumed();
  }
  return bytes_read;
}

uint64_t QuicCryptoStream::BytesReadOnLevel(EncryptionLevel level) const {
  return substreams_[level].sequencer.NumBytesConsumed();
}

bool QuicCryptoStream::WriteCryptoFrame(EncryptionLevel level,
                                        QuicStreamOffset offset,
                                        QuicByteCount data_length,
                                        QuicDataWriter* writer) {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 don't write CRYPTO frames (2)";
  return substreams_[level].send_buffer.WriteStreamData(offset, data_length,
                                                        writer);
}

void QuicCryptoStream::OnCryptoFrameLost(QuicCryptoFrame* crypto_frame) {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 don't lose CRYPTO frames";
  substreams_[crypto_frame->level].send_buffer.OnStreamDataLost(
      crypto_frame->offset, crypto_frame->data_length);
}

void QuicCryptoStream::RetransmitData(QuicCryptoFrame* crypto_frame,
                                      TransmissionType type) {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 don't retransmit CRYPTO frames";
  QuicIntervalSet<QuicStreamOffset> retransmission(
      crypto_frame->offset, crypto_frame->offset + crypto_frame->data_length);
  QuicStreamSendBuffer* send_buffer =
      &substreams_[crypto_frame->level].send_buffer;
  retransmission.Difference(send_buffer->bytes_acked());
  if (retransmission.Empty()) {
    return;
  }
  for (const auto& interval : retransmission) {
    size_t retransmission_offset = interval.min();
    size_t retransmission_length = interval.max() - interval.min();
    size_t bytes_consumed = stream_delegate()->SendCryptoData(
        crypto_frame->level, retransmission_length, retransmission_offset,
        type);
    send_buffer->OnStreamDataRetransmitted(retransmission_offset,
                                           bytes_consumed);
    if (bytes_consumed < retransmission_length) {
      break;
    }
  }
}

void QuicCryptoStream::WriteBufferedCryptoFrames() {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 don't use CRYPTO frames";
  for (EncryptionLevel level : AllEncryptionLevels()) {
    QuicStreamSendBuffer* send_buffer = &substreams_[level].send_buffer;
    const size_t data_length =
        send_buffer->stream_offset() - send_buffer->stream_bytes_written();
    if (data_length == 0) {
      // No buffered data for this encryption level.
      continue;
    }
    size_t bytes_consumed = stream_delegate()->SendCryptoData(
        level, data_length, send_buffer->stream_bytes_written(),
        NOT_RETRANSMISSION);
    send_buffer->OnStreamDataConsumed(bytes_consumed);
    if (bytes_consumed < data_length) {
      // Connection is write blocked.
      break;
    }
  }
}

bool QuicCryptoStream::HasBufferedCryptoFrames() const {
  QUIC_BUG_IF(!QuicVersionUsesCryptoFrames(session()->transport_version()))
      << "Versions less than 47 don't use CRYPTO frames";
  for (EncryptionLevel level : AllEncryptionLevels()) {
    const QuicStreamSendBuffer& send_buffer = substreams_[level].send_buffer;
    DCHECK_GE(send_buffer.stream_offset(), send_buffer.stream_bytes_written());
    if (send_buffer.stream_offset() > send_buffer.stream_bytes_written()) {
      return true;
    }
  }
  return false;
}

bool QuicCryptoStream::IsFrameOutstanding(EncryptionLevel level,
                                          size_t offset,
                                          size_t length) const {
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    // This only happens if a client was originally configured for a version
    // greater than 45, but received a version negotiation packet and is
    // attempting to retransmit for a version less than 47. Outside of tests,
    // this is a misconfiguration of the client, and this connection will be
    // doomed. Return false here to avoid trying to retransmit CRYPTO frames on
    // the wrong transport version.
    return false;
  }
  return substreams_[level].send_buffer.IsStreamDataOutstanding(offset, length);
}

bool QuicCryptoStream::IsWaitingForAcks() const {
  if (!QuicVersionUsesCryptoFrames(session()->transport_version())) {
    return QuicStream::IsWaitingForAcks();
  }
  for (EncryptionLevel level : AllEncryptionLevels()) {
    if (substreams_[level].send_buffer.stream_bytes_outstanding()) {
      return true;
    }
  }
  return false;
}

QuicCryptoStream::CryptoSubstream::CryptoSubstream(
    QuicCryptoStream* crypto_stream,
    EncryptionLevel)
    : sequencer(crypto_stream),
      send_buffer(crypto_stream->session()
                      ->connection()
                      ->helper()
                      ->GetStreamSendBufferAllocator()) {}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
