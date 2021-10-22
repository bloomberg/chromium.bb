// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/core/http/quic_spdy_stream.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quic/core/http/capsule.h"
#include "quic/core/http/http_constants.h"
#include "quic/core/http/http_decoder.h"
#include "quic/core/http/http_frames.h"
#include "quic/core/http/quic_spdy_session.h"
#include "quic/core/http/spdy_utils.h"
#include "quic/core/http/web_transport_http3.h"
#include "quic/core/qpack/qpack_decoder.h"
#include "quic/core/qpack/qpack_encoder.h"
#include "quic/core/quic_types.h"
#include "quic/core/quic_utils.h"
#include "quic/core/quic_versions.h"
#include "quic/core/quic_write_blocked_list.h"
#include "quic/core/web_transport_interface.h"
#include "quic/platform/api/quic_bug_tracker.h"
#include "quic/platform/api/quic_flag_utils.h"
#include "quic/platform/api/quic_flags.h"
#include "quic/platform/api/quic_logging.h"
#include "quic/platform/api/quic_mem_slice_storage.h"
#include "common/quiche_text_utils.h"
#include "spdy/core/spdy_protocol.h"

using spdy::SpdyHeaderBlock;
using spdy::SpdyPriority;

namespace quic {

// Visitor of HttpDecoder that passes data frame to QuicSpdyStream and closes
// the connection on unexpected frames.
class QuicSpdyStream::HttpDecoderVisitor : public HttpDecoder::Visitor {
 public:
  explicit HttpDecoderVisitor(QuicSpdyStream* stream) : stream_(stream) {}
  HttpDecoderVisitor(const HttpDecoderVisitor&) = delete;
  HttpDecoderVisitor& operator=(const HttpDecoderVisitor&) = delete;

  void OnError(HttpDecoder* decoder) override {
    stream_->OnUnrecoverableError(decoder->error(), decoder->error_detail());
  }

  bool OnMaxPushIdFrame(const MaxPushIdFrame& /*frame*/) override {
    CloseConnectionOnWrongFrame("Max Push Id");
    return false;
  }

  bool OnGoAwayFrame(const GoAwayFrame& /*frame*/) override {
    CloseConnectionOnWrongFrame("Goaway");
    return false;
  }

  bool OnSettingsFrameStart(QuicByteCount /*header_length*/) override {
    CloseConnectionOnWrongFrame("Settings");
    return false;
  }

  bool OnSettingsFrame(const SettingsFrame& /*frame*/) override {
    CloseConnectionOnWrongFrame("Settings");
    return false;
  }

  bool OnDataFrameStart(QuicByteCount header_length,
                        QuicByteCount payload_length) override {
    return stream_->OnDataFrameStart(header_length, payload_length);
  }

  bool OnDataFramePayload(absl::string_view payload) override {
    QUICHE_DCHECK(!payload.empty());
    return stream_->OnDataFramePayload(payload);
  }

  bool OnDataFrameEnd() override { return stream_->OnDataFrameEnd(); }

  bool OnHeadersFrameStart(QuicByteCount header_length,
                           QuicByteCount payload_length) override {
    if (!VersionUsesHttp3(stream_->transport_version())) {
      CloseConnectionOnWrongFrame("Headers");
      return false;
    }
    return stream_->OnHeadersFrameStart(header_length, payload_length);
  }

  bool OnHeadersFramePayload(absl::string_view payload) override {
    QUICHE_DCHECK(!payload.empty());
    if (!VersionUsesHttp3(stream_->transport_version())) {
      CloseConnectionOnWrongFrame("Headers");
      return false;
    }
    return stream_->OnHeadersFramePayload(payload);
  }

  bool OnHeadersFrameEnd() override {
    if (!VersionUsesHttp3(stream_->transport_version())) {
      CloseConnectionOnWrongFrame("Headers");
      return false;
    }
    return stream_->OnHeadersFrameEnd();
  }

  bool OnPriorityUpdateFrameStart(QuicByteCount /*header_length*/) override {
    CloseConnectionOnWrongFrame("Priority update");
    return false;
  }

  bool OnPriorityUpdateFrame(const PriorityUpdateFrame& /*frame*/) override {
    CloseConnectionOnWrongFrame("Priority update");
    return false;
  }

  bool OnAcceptChFrameStart(QuicByteCount /*header_length*/) override {
    CloseConnectionOnWrongFrame("ACCEPT_CH");
    return false;
  }

  bool OnAcceptChFrame(const AcceptChFrame& /*frame*/) override {
    CloseConnectionOnWrongFrame("ACCEPT_CH");
    return false;
  }

  void OnWebTransportStreamFrameType(
      QuicByteCount header_length, WebTransportSessionId session_id) override {
    stream_->OnWebTransportStreamFrameType(header_length, session_id);
  }

  bool OnUnknownFrameStart(uint64_t frame_type, QuicByteCount header_length,
                           QuicByteCount payload_length) override {
    return stream_->OnUnknownFrameStart(frame_type, header_length,
                                        payload_length);
  }

  bool OnUnknownFramePayload(absl::string_view payload) override {
    return stream_->OnUnknownFramePayload(payload);
  }

  bool OnUnknownFrameEnd() override { return stream_->OnUnknownFrameEnd(); }

 private:
  void CloseConnectionOnWrongFrame(absl::string_view frame_type) {
    stream_->OnUnrecoverableError(
        QUIC_HTTP_FRAME_UNEXPECTED_ON_SPDY_STREAM,
        absl::StrCat(frame_type, " frame received on data stream"));
  }

  QuicSpdyStream* stream_;
};

#define ENDPOINT                                                   \
  (session()->perspective() == Perspective::IS_SERVER ? "Server: " \
                                                      : "Client:"  \
                                                        " ")

namespace {
HttpDecoder::Options HttpDecoderOptionsForBidiStream(
    QuicSpdySession* spdy_session) {
  HttpDecoder::Options options;
  options.allow_web_transport_stream =
      spdy_session->WillNegotiateWebTransport();
  return options;
}
}  // namespace

QuicSpdyStream::QuicSpdyStream(QuicStreamId id, QuicSpdySession* spdy_session,
                               StreamType type)
    : QuicStream(id, spdy_session, /*is_static=*/false, type),
      spdy_session_(spdy_session),
      on_body_available_called_because_sequencer_is_closed_(false),
      visitor_(nullptr),
      blocked_on_decoding_headers_(false),
      headers_decompressed_(false),
      header_list_size_limit_exceeded_(false),
      headers_payload_length_(0),
      trailers_decompressed_(false),
      trailers_consumed_(false),
      http_decoder_visitor_(std::make_unique<HttpDecoderVisitor>(this)),
      decoder_(http_decoder_visitor_.get(),
               HttpDecoderOptionsForBidiStream(spdy_session)),
      sequencer_offset_(0),
      is_decoder_processing_input_(false),
      ack_listener_(nullptr),
      last_sent_urgency_(kDefaultUrgency),
      datagram_next_available_context_id_(spdy_session->perspective() ==
                                                  Perspective::IS_SERVER
                                              ? kFirstDatagramContextIdServer
                                              : kFirstDatagramContextIdClient) {
  QUICHE_DCHECK_EQ(session()->connection(), spdy_session->connection());
  QUICHE_DCHECK_EQ(transport_version(), spdy_session->transport_version());
  QUICHE_DCHECK(!QuicUtils::IsCryptoStreamId(transport_version(), id));
  QUICHE_DCHECK_EQ(0u, sequencer()->NumBytesConsumed());
  // If headers are sent on the headers stream, then do not receive any
  // callbacks from the sequencer until headers are complete.
  if (!VersionUsesHttp3(transport_version())) {
    sequencer()->SetBlockedUntilFlush();
  }

  if (VersionUsesHttp3(transport_version())) {
    sequencer()->set_level_triggered(true);
  }

  spdy_session_->OnStreamCreated(this);
}

QuicSpdyStream::QuicSpdyStream(PendingStream* pending,
                               QuicSpdySession* spdy_session)
    : QuicStream(pending, spdy_session, /*is_static=*/false),
      spdy_session_(spdy_session),
      on_body_available_called_because_sequencer_is_closed_(false),
      visitor_(nullptr),
      blocked_on_decoding_headers_(false),
      headers_decompressed_(false),
      header_list_size_limit_exceeded_(false),
      headers_payload_length_(0),
      trailers_decompressed_(false),
      trailers_consumed_(false),
      http_decoder_visitor_(std::make_unique<HttpDecoderVisitor>(this)),
      decoder_(http_decoder_visitor_.get()),
      sequencer_offset_(sequencer()->NumBytesConsumed()),
      is_decoder_processing_input_(false),
      ack_listener_(nullptr),
      last_sent_urgency_(kDefaultUrgency) {
  QUICHE_DCHECK_EQ(session()->connection(), spdy_session->connection());
  QUICHE_DCHECK_EQ(transport_version(), spdy_session->transport_version());
  QUICHE_DCHECK(!QuicUtils::IsCryptoStreamId(transport_version(), id()));
  // If headers are sent on the headers stream, then do not receive any
  // callbacks from the sequencer until headers are complete.
  if (!VersionUsesHttp3(transport_version())) {
    sequencer()->SetBlockedUntilFlush();
  }

  if (VersionUsesHttp3(transport_version())) {
    sequencer()->set_level_triggered(true);
  }

  spdy_session_->OnStreamCreated(this);
}

QuicSpdyStream::~QuicSpdyStream() {}

size_t QuicSpdyStream::WriteHeaders(
    SpdyHeaderBlock header_block, bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (!AssertNotWebTransportDataStream("writing headers")) {
    return 0;
  }

  QuicConnection::ScopedPacketFlusher flusher(spdy_session_->connection());
  // Send stream type for server push stream
  if (VersionUsesHttp3(transport_version()) && type() == WRITE_UNIDIRECTIONAL &&
      send_buffer().stream_offset() == 0) {
    char data[sizeof(kServerPushStream)];
    QuicDataWriter writer(ABSL_ARRAYSIZE(data), data);
    writer.WriteVarInt62(kServerPushStream);

    // Similar to frame headers, stream type byte shouldn't be exposed to upper
    // layer applications.
    unacked_frame_headers_offsets_.Add(0, writer.length());

    QUIC_LOG(INFO) << ENDPOINT << "Stream " << id()
                   << " is writing type as server push";
    WriteOrBufferData(absl::string_view(writer.data(), writer.length()), false,
                      nullptr);
  }

  MaybeProcessSentWebTransportHeaders(header_block);

  size_t bytes_written =
      WriteHeadersImpl(std::move(header_block), fin, std::move(ack_listener));
  if (!VersionUsesHttp3(transport_version()) && fin) {
    // If HEADERS are sent on the headers stream, then |fin_sent_| needs to be
    // set and write side needs to be closed without actually sending a FIN on
    // this stream.
    // TODO(rch): Add test to ensure fin_sent_ is set whenever a fin is sent.
    SetFinSent();
    CloseWriteSide();
  }

  if (web_transport_ != nullptr &&
      session()->perspective() == Perspective::IS_CLIENT) {
    // This will send a capsule and therefore needs to happen after headers have
    // been sent.
    RegisterHttp3DatagramContextId(
        web_transport_->context_id(), DatagramFormatType::WEBTRANSPORT,
        /*format_additional_data=*/absl::string_view(), web_transport_.get());
  }

  return bytes_written;
}

void QuicSpdyStream::WriteOrBufferBody(absl::string_view data, bool fin) {
  if (!AssertNotWebTransportDataStream("writing body data")) {
    return;
  }
  if (!VersionUsesHttp3(transport_version()) || data.length() == 0) {
    WriteOrBufferData(data, fin, nullptr);
    return;
  }
  QuicConnection::ScopedPacketFlusher flusher(spdy_session_->connection());

  if (spdy_session_->debug_visitor()) {
    spdy_session_->debug_visitor()->OnDataFrameSent(id(), data.length());
  }

  const bool success =
      WriteDataFrameHeader(data.length(), /*force_write=*/true);
  QUICHE_DCHECK(success);

  // Write body.
  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id()
                  << " is writing DATA frame payload of length "
                  << data.length() << " with fin " << fin;
  WriteOrBufferData(data, fin, nullptr);
}

size_t QuicSpdyStream::WriteTrailers(
    SpdyHeaderBlock trailer_block,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (fin_sent()) {
    QUIC_BUG(quic_bug_10410_1)
        << "Trailers cannot be sent after a FIN, on stream " << id();
    return 0;
  }

  if (!VersionUsesHttp3(transport_version())) {
    // The header block must contain the final offset for this stream, as the
    // trailers may be processed out of order at the peer.
    const QuicStreamOffset final_offset =
        stream_bytes_written() + BufferedDataBytes();
    QUIC_DLOG(INFO) << ENDPOINT << "Inserting trailer: ("
                    << kFinalOffsetHeaderKey << ", " << final_offset << ")";
    trailer_block.insert(
        std::make_pair(kFinalOffsetHeaderKey, absl::StrCat(final_offset)));
  }

  // Write the trailing headers with a FIN, and close stream for writing:
  // trailers are the last thing to be sent on a stream.
  const bool kFin = true;
  size_t bytes_written =
      WriteHeadersImpl(std::move(trailer_block), kFin, std::move(ack_listener));

  // If trailers are sent on the headers stream, then |fin_sent_| needs to be
  // set without actually sending a FIN on this stream.
  if (!VersionUsesHttp3(transport_version())) {
    SetFinSent();

    // Also, write side of this stream needs to be closed.  However, only do
    // this if there is no more buffered data, otherwise it will never be sent.
    if (BufferedDataBytes() == 0) {
      CloseWriteSide();
    }
  }

  return bytes_written;
}

QuicConsumedData QuicSpdyStream::WritevBody(const struct iovec* iov, int count,
                                            bool fin) {
  QuicMemSliceStorage storage(
      iov, count,
      session()->connection()->helper()->GetStreamSendBufferAllocator(),
      GetQuicFlag(FLAGS_quic_send_buffer_max_data_slice_size));
  return WriteBodySlices(storage.ToSpan(), fin);
}

bool QuicSpdyStream::WriteDataFrameHeader(QuicByteCount data_length,
                                          bool force_write) {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));
  QUICHE_DCHECK_GT(data_length, 0u);
  QuicBuffer header = HttpEncoder::SerializeDataFrameHeader(
      data_length,
      spdy_session_->connection()->helper()->GetStreamSendBufferAllocator());
  const bool can_write = CanWriteNewDataAfterData(header.size());
  if (!can_write && !force_write) {
    return false;
  }

  if (spdy_session_->debug_visitor()) {
    spdy_session_->debug_visitor()->OnDataFrameSent(id(), data_length);
  }

  unacked_frame_headers_offsets_.Add(
      send_buffer().stream_offset(),
      send_buffer().stream_offset() + header.size());
  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id()
                  << " is writing DATA frame header of length "
                  << header.size();
  if (can_write) {
    // Save one copy and allocation if send buffer can accomodate the header.
    QuicMemSlice header_slice(std::move(header));
    WriteMemSlices(absl::MakeSpan(&header_slice, 1), false);
  } else {
    QUICHE_DCHECK(force_write);
    WriteOrBufferData(header.AsStringView(), false, nullptr);
  }
  return true;
}

QuicConsumedData QuicSpdyStream::WriteBodySlices(
    absl::Span<QuicMemSlice> slices, bool fin) {
  if (!VersionUsesHttp3(transport_version()) || slices.empty()) {
    return WriteMemSlices(slices, fin);
  }

  QuicConnection::ScopedPacketFlusher flusher(spdy_session_->connection());
  const QuicByteCount data_size = MemSliceSpanTotalSize(slices);
  if (!WriteDataFrameHeader(data_size, /*force_write=*/false)) {
    return {0, false};
  }

  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id()
                  << " is writing DATA frame payload of length " << data_size;
  return WriteMemSlices(slices, fin);
}

size_t QuicSpdyStream::Readv(const struct iovec* iov, size_t iov_len) {
  QUICHE_DCHECK(FinishedReadingHeaders());
  if (!VersionUsesHttp3(transport_version())) {
    return sequencer()->Readv(iov, iov_len);
  }
  size_t bytes_read = 0;
  sequencer()->MarkConsumed(body_manager_.ReadBody(iov, iov_len, &bytes_read));

  return bytes_read;
}

int QuicSpdyStream::GetReadableRegions(iovec* iov, size_t iov_len) const {
  QUICHE_DCHECK(FinishedReadingHeaders());
  if (!VersionUsesHttp3(transport_version())) {
    return sequencer()->GetReadableRegions(iov, iov_len);
  }
  return body_manager_.PeekBody(iov, iov_len);
}

void QuicSpdyStream::MarkConsumed(size_t num_bytes) {
  QUICHE_DCHECK(FinishedReadingHeaders());
  if (!VersionUsesHttp3(transport_version())) {
    sequencer()->MarkConsumed(num_bytes);
    return;
  }

  sequencer()->MarkConsumed(body_manager_.OnBodyConsumed(num_bytes));
}

bool QuicSpdyStream::IsDoneReading() const {
  bool done_reading_headers = FinishedReadingHeaders();
  bool done_reading_body = sequencer()->IsClosed();
  bool done_reading_trailers = FinishedReadingTrailers();
  return done_reading_headers && done_reading_body && done_reading_trailers;
}

bool QuicSpdyStream::HasBytesToRead() const {
  if (!VersionUsesHttp3(transport_version())) {
    return sequencer()->HasBytesToRead();
  }
  return body_manager_.HasBytesToRead();
}

void QuicSpdyStream::MarkTrailersConsumed() { trailers_consumed_ = true; }

uint64_t QuicSpdyStream::total_body_bytes_read() const {
  if (VersionUsesHttp3(transport_version())) {
    return body_manager_.total_body_bytes_received();
  }
  return sequencer()->NumBytesConsumed();
}

void QuicSpdyStream::ConsumeHeaderList() {
  header_list_.Clear();

  if (!FinishedReadingHeaders()) {
    return;
  }

  if (!VersionUsesHttp3(transport_version())) {
    sequencer()->SetUnblocked();
    return;
  }

  if (body_manager_.HasBytesToRead()) {
    HandleBodyAvailable();
    return;
  }

  if (sequencer()->IsClosed() &&
      !on_body_available_called_because_sequencer_is_closed_) {
    on_body_available_called_because_sequencer_is_closed_ = true;
    HandleBodyAvailable();
  }
}

void QuicSpdyStream::OnStreamHeadersPriority(
    const spdy::SpdyStreamPrecedence& precedence) {
  QUICHE_DCHECK_EQ(Perspective::IS_SERVER,
                   session()->connection()->perspective());
  SetPriority(precedence);
}

void QuicSpdyStream::OnStreamHeaderList(bool fin, size_t frame_len,
                                        const QuicHeaderList& header_list) {
  if (!spdy_session()->user_agent_id().has_value()) {
    std::string uaid;
    for (const auto& kv : header_list) {
      if (quiche::QuicheTextUtils::ToLower(kv.first) == kUserAgentHeaderName) {
        uaid = kv.second;
        break;
      }
    }
    spdy_session()->SetUserAgentId(std::move(uaid));
  }

  // TODO(b/134706391): remove |fin| argument.
  // When using Google QUIC, an empty header list indicates that the size limit
  // has been exceeded.
  // When using IETF QUIC, there is an explicit signal from
  // QpackDecodedHeadersAccumulator.
  if ((VersionUsesHttp3(transport_version()) &&
       header_list_size_limit_exceeded_) ||
      (!VersionUsesHttp3(transport_version()) && header_list.empty())) {
    OnHeadersTooLarge();
    if (IsDoneReading()) {
      return;
    }
  }
  if (!headers_decompressed_) {
    OnInitialHeadersComplete(fin, frame_len, header_list);
  } else {
    OnTrailingHeadersComplete(fin, frame_len, header_list);
  }
}

void QuicSpdyStream::OnHeadersDecoded(QuicHeaderList headers,
                                      bool header_list_size_limit_exceeded) {
  header_list_size_limit_exceeded_ = header_list_size_limit_exceeded;
  qpack_decoded_headers_accumulator_.reset();

  QuicSpdySession::LogHeaderCompressionRatioHistogram(
      /* using_qpack = */ true,
      /* is_sent = */ false, headers.compressed_header_bytes(),
      headers.uncompressed_header_bytes());

  const QuicStreamId promised_stream_id = spdy_session()->promised_stream_id();
  Http3DebugVisitor* const debug_visitor = spdy_session()->debug_visitor();
  if (promised_stream_id ==
      QuicUtils::GetInvalidStreamId(transport_version())) {
    if (debug_visitor) {
      debug_visitor->OnHeadersDecoded(id(), headers);
    }

    OnStreamHeaderList(/* fin = */ false, headers_payload_length_, headers);
  } else {
    spdy_session_->OnHeaderList(headers);
  }

  if (blocked_on_decoding_headers_) {
    blocked_on_decoding_headers_ = false;
    // Continue decoding HTTP/3 frames.
    OnDataAvailable();
  }
}

void QuicSpdyStream::OnHeaderDecodingError(QuicErrorCode error_code,
                                           absl::string_view error_message) {
  qpack_decoded_headers_accumulator_.reset();

  std::string connection_close_error_message = absl::StrCat(
      "Error decoding ", headers_decompressed_ ? "trailers" : "headers",
      " on stream ", id(), ": ", error_message);
  OnUnrecoverableError(error_code, connection_close_error_message);
}

void QuicSpdyStream::MaybeSendPriorityUpdateFrame() {
  if (!VersionUsesHttp3(transport_version()) ||
      session()->perspective() != Perspective::IS_CLIENT) {
    return;
  }

  // Value between 0 and 7, inclusive.  Lower value means higher priority.
  int urgency = precedence().spdy3_priority();
  if (last_sent_urgency_ == urgency) {
    return;
  }
  last_sent_urgency_ = urgency;

  PriorityUpdateFrame priority_update;
  priority_update.prioritized_element_type = REQUEST_STREAM;
  priority_update.prioritized_element_id = id();
  priority_update.priority_field_value = absl::StrCat("u=", urgency);
  spdy_session_->WriteHttp3PriorityUpdate(priority_update);
}

void QuicSpdyStream::OnHeadersTooLarge() { Reset(QUIC_HEADERS_TOO_LARGE); }

void QuicSpdyStream::OnInitialHeadersComplete(
    bool fin, size_t /*frame_len*/, const QuicHeaderList& header_list) {
  // TODO(b/134706391): remove |fin| argument.
  headers_decompressed_ = true;
  header_list_ = header_list;

  MaybeProcessReceivedWebTransportHeaders();

  if (VersionUsesHttp3(transport_version())) {
    if (fin) {
      OnStreamFrame(QuicStreamFrame(id(), /* fin = */ true,
                                    highest_received_byte_offset(),
                                    absl::string_view()));
    }
    return;
  }

  if (fin && !rst_sent()) {
    OnStreamFrame(
        QuicStreamFrame(id(), fin, /* offset = */ 0, absl::string_view()));
  }
  if (FinishedReadingHeaders()) {
    sequencer()->SetUnblocked();
  }
}

void QuicSpdyStream::OnPromiseHeaderList(
    QuicStreamId /* promised_id */, size_t /* frame_len */,
    const QuicHeaderList& /*header_list */) {
  // To be overridden in QuicSpdyClientStream.  Not supported on
  // server side.
  stream_delegate()->OnStreamError(QUIC_INVALID_HEADERS_STREAM_DATA,
                                   "Promise headers received by server");
}

void QuicSpdyStream::OnTrailingHeadersComplete(
    bool fin, size_t /*frame_len*/, const QuicHeaderList& header_list) {
  // TODO(b/134706391): remove |fin| argument.
  QUICHE_DCHECK(!trailers_decompressed_);
  if (!VersionUsesHttp3(transport_version()) && fin_received()) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Received Trailers after FIN, on stream: " << id();
    stream_delegate()->OnStreamError(QUIC_INVALID_HEADERS_STREAM_DATA,
                                     "Trailers after fin");
    return;
  }

  if (!VersionUsesHttp3(transport_version()) && !fin) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Trailers must have FIN set, on stream: " << id();
    stream_delegate()->OnStreamError(QUIC_INVALID_HEADERS_STREAM_DATA,
                                     "Fin missing from trailers");
    return;
  }

  size_t final_byte_offset = 0;
  const bool expect_final_byte_offset = !VersionUsesHttp3(transport_version());
  if (!SpdyUtils::CopyAndValidateTrailers(header_list, expect_final_byte_offset,
                                          &final_byte_offset,
                                          &received_trailers_)) {
    QUIC_DLOG(ERROR) << ENDPOINT << "Trailers for stream " << id()
                     << " are malformed.";
    stream_delegate()->OnStreamError(QUIC_INVALID_HEADERS_STREAM_DATA,
                                     "Trailers are malformed");
    return;
  }
  trailers_decompressed_ = true;
  if (fin) {
    const QuicStreamOffset offset = VersionUsesHttp3(transport_version())
                                        ? highest_received_byte_offset()
                                        : final_byte_offset;
    OnStreamFrame(QuicStreamFrame(id(), fin, offset, absl::string_view()));
  }
}

void QuicSpdyStream::OnPriorityFrame(
    const spdy::SpdyStreamPrecedence& precedence) {
  QUICHE_DCHECK_EQ(Perspective::IS_SERVER,
                   session()->connection()->perspective());
  SetPriority(precedence);
}

void QuicSpdyStream::OnStreamReset(const QuicRstStreamFrame& frame) {
  if (web_transport_data_ != nullptr) {
    WebTransportStreamVisitor* webtransport_visitor =
        web_transport_data_->adapter.visitor();
    if (webtransport_visitor != nullptr) {
      webtransport_visitor->OnResetStreamReceived(
          Http3ErrorToWebTransportOrDefault(frame.ietf_error_code));
    }
    QuicStream::OnStreamReset(frame);
    return;
  }

  // TODO(bnc): Merge the two blocks below when both
  // quic_abort_qpack_on_stream_reset and quic_fix_on_stream_reset are
  // deprecated.
  if (frame.error_code != QUIC_STREAM_NO_ERROR) {
    if (VersionUsesHttp3(transport_version()) && !fin_received() &&
        spdy_session_->qpack_decoder()) {
      QUIC_CODE_COUNT_N(quic_abort_qpack_on_stream_reset, 1, 2);
      spdy_session_->qpack_decoder()->OnStreamReset(id());
      if (GetQuicReloadableFlag(quic_abort_qpack_on_stream_reset)) {
        QUIC_RELOADABLE_FLAG_COUNT_N(quic_abort_qpack_on_stream_reset, 1, 2);
        qpack_decoded_headers_accumulator_.reset();
      }
    }

    QuicStream::OnStreamReset(frame);
    return;
  }

  if (GetQuicReloadableFlag(quic_fix_on_stream_reset) &&
      VersionUsesHttp3(transport_version())) {
    QUIC_RELOADABLE_FLAG_COUNT(quic_fix_on_stream_reset);
    if (!fin_received() && spdy_session_->qpack_decoder()) {
      spdy_session_->qpack_decoder()->OnStreamReset(id());
      qpack_decoded_headers_accumulator_.reset();
    }

    QuicStream::OnStreamReset(frame);
    return;
  }

  QUIC_DVLOG(1) << ENDPOINT
                << "Received QUIC_STREAM_NO_ERROR, not discarding response";
  set_rst_received(true);
  MaybeIncreaseHighestReceivedOffset(frame.byte_offset);
  set_stream_error(frame.error());
  CloseWriteSide();
}

void QuicSpdyStream::ResetWithError(QuicResetStreamError error) {
  if (VersionUsesHttp3(transport_version()) && !fin_received() &&
      spdy_session_->qpack_decoder() && web_transport_data_ == nullptr) {
    QUIC_CODE_COUNT_N(quic_abort_qpack_on_stream_reset, 2, 2);
    spdy_session_->qpack_decoder()->OnStreamReset(id());
    if (GetQuicReloadableFlag(quic_abort_qpack_on_stream_reset)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_abort_qpack_on_stream_reset, 2, 2);
      qpack_decoded_headers_accumulator_.reset();
    }
  }

  QuicStream::ResetWithError(error);
}

bool QuicSpdyStream::OnStopSending(QuicResetStreamError error) {
  if (web_transport_data_ != nullptr) {
    WebTransportStreamVisitor* visitor = web_transport_data_->adapter.visitor();
    if (visitor != nullptr) {
      visitor->OnStopSendingReceived(
          Http3ErrorToWebTransportOrDefault(error.ietf_application_code()));
    }
  }

  return QuicStream::OnStopSending(error);
}

void QuicSpdyStream::OnWriteSideInDataRecvdState() {
  if (web_transport_data_ != nullptr) {
    WebTransportStreamVisitor* visitor = web_transport_data_->adapter.visitor();
    if (visitor != nullptr) {
      visitor->OnWriteSideInDataRecvdState();
    }
  }

  QuicStream::OnWriteSideInDataRecvdState();
}

void QuicSpdyStream::OnDataAvailable() {
  if (!VersionUsesHttp3(transport_version())) {
    // Sequencer must be blocked until headers are consumed.
    QUICHE_DCHECK(FinishedReadingHeaders());
  }

  if (!VersionUsesHttp3(transport_version())) {
    HandleBodyAvailable();
    return;
  }

  if (web_transport_data_ != nullptr) {
    web_transport_data_->adapter.OnDataAvailable();
    return;
  }

  if (!spdy_session()->ShouldProcessIncomingRequests()) {
    spdy_session()->OnStreamWaitingForClientSettings(id());
    return;
  }

  if (is_decoder_processing_input_) {
    // Let the outermost nested OnDataAvailable() call do the work.
    return;
  }

  if (blocked_on_decoding_headers_) {
    return;
  }

  iovec iov;
  while (session()->connection()->connected() && !reading_stopped() &&
         decoder_.error() == QUIC_NO_ERROR) {
    QUICHE_DCHECK_GE(sequencer_offset_, sequencer()->NumBytesConsumed());
    if (!sequencer()->PeekRegion(sequencer_offset_, &iov)) {
      break;
    }

    QUICHE_DCHECK(!sequencer()->IsClosed());
    is_decoder_processing_input_ = true;
    QuicByteCount processed_bytes = decoder_.ProcessInput(
        reinterpret_cast<const char*>(iov.iov_base), iov.iov_len);
    is_decoder_processing_input_ = false;
    sequencer_offset_ += processed_bytes;
    if (blocked_on_decoding_headers_) {
      return;
    }
    if (web_transport_data_ != nullptr) {
      return;
    }
  }

  // Do not call HandleBodyAvailable() until headers are consumed.
  if (!FinishedReadingHeaders()) {
    return;
  }

  if (body_manager_.HasBytesToRead()) {
    HandleBodyAvailable();
    return;
  }

  if (sequencer()->IsClosed() &&
      !on_body_available_called_because_sequencer_is_closed_) {
    on_body_available_called_because_sequencer_is_closed_ = true;
    HandleBodyAvailable();
  }
}

void QuicSpdyStream::OnClose() {
  QuicStream::OnClose();

  qpack_decoded_headers_accumulator_.reset();

  if (visitor_) {
    Visitor* visitor = visitor_;
    // Calling Visitor::OnClose() may result the destruction of the visitor,
    // so we need to ensure we don't call it again.
    visitor_ = nullptr;
    visitor->OnClose(this);
  }

  if (datagram_flow_id_.has_value()) {
    spdy_session_->UnregisterHttp3DatagramFlowId(datagram_flow_id_.value());
  }

  if (web_transport_ != nullptr) {
    web_transport_->OnConnectStreamClosing();
  }
  if (web_transport_data_ != nullptr) {
    WebTransportHttp3* web_transport =
        spdy_session_->GetWebTransportSession(web_transport_data_->session_id);
    if (web_transport == nullptr) {
      // Since there is no guaranteed destruction order for streams, the session
      // could be already removed from the stream map by the time we reach here.
      QUIC_DLOG(WARNING) << ENDPOINT << "WebTransport stream " << id()
                         << " attempted to notify parent session "
                         << web_transport_data_->session_id
                         << ", but the session could not be found.";
      return;
    }
    web_transport->OnStreamClosed(id());
  }
}

void QuicSpdyStream::OnCanWrite() {
  QuicStream::OnCanWrite();

  // Trailers (and hence a FIN) may have been sent ahead of queued body bytes.
  if (!HasBufferedData() && fin_sent()) {
    CloseWriteSide();
  }
}

bool QuicSpdyStream::FinishedReadingHeaders() const {
  return headers_decompressed_ && header_list_.empty();
}

// static
bool QuicSpdyStream::ParseHeaderStatusCode(const SpdyHeaderBlock& header,
                                           int* status_code) {
  SpdyHeaderBlock::const_iterator it = header.find(spdy::kHttp2StatusHeader);
  if (it == header.end()) {
    return false;
  }
  const absl::string_view status(it->second);
  if (status.size() != 3) {
    return false;
  }
  // First character must be an integer in range [1,5].
  if (status[0] < '1' || status[0] > '5') {
    return false;
  }
  // The remaining two characters must be integers.
  if (!isdigit(status[1]) || !isdigit(status[2])) {
    return false;
  }
  return absl::SimpleAtoi(status, status_code);
}

bool QuicSpdyStream::FinishedReadingTrailers() const {
  // If no further trailing headers are expected, and the decompressed trailers
  // (if any) have been consumed, then reading of trailers is finished.
  if (!fin_received()) {
    return false;
  } else if (!trailers_decompressed_) {
    return true;
  } else {
    return trailers_consumed_;
  }
}

bool QuicSpdyStream::OnDataFrameStart(QuicByteCount header_length,
                                      QuicByteCount payload_length) {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));

  if (spdy_session_->debug_visitor()) {
    spdy_session_->debug_visitor()->OnDataFrameReceived(id(), payload_length);
  }

  if (!headers_decompressed_ || trailers_decompressed_) {
    stream_delegate()->OnStreamError(
        QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_SPDY_STREAM,
        "Unexpected DATA frame received.");
    return false;
  }

  sequencer()->MarkConsumed(body_manager_.OnNonBody(header_length));

  return true;
}

bool QuicSpdyStream::OnDataFramePayload(absl::string_view payload) {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));

  body_manager_.OnBody(payload);

  return true;
}

bool QuicSpdyStream::OnDataFrameEnd() {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));

  QUIC_DVLOG(1) << ENDPOINT
                << "Reaches the end of a data frame. Total bytes received are "
                << body_manager_.total_body_bytes_received();
  return true;
}

bool QuicSpdyStream::OnStreamFrameAcked(QuicStreamOffset offset,
                                        QuicByteCount data_length,
                                        bool fin_acked,
                                        QuicTime::Delta ack_delay_time,
                                        QuicTime receive_timestamp,
                                        QuicByteCount* newly_acked_length) {
  const bool new_data_acked = QuicStream::OnStreamFrameAcked(
      offset, data_length, fin_acked, ack_delay_time, receive_timestamp,
      newly_acked_length);

  const QuicByteCount newly_acked_header_length =
      GetNumFrameHeadersInInterval(offset, data_length);
  QUICHE_DCHECK_LE(newly_acked_header_length, *newly_acked_length);
  unacked_frame_headers_offsets_.Difference(offset, offset + data_length);
  if (ack_listener_ != nullptr && new_data_acked) {
    ack_listener_->OnPacketAcked(
        *newly_acked_length - newly_acked_header_length, ack_delay_time);
  }
  return new_data_acked;
}

void QuicSpdyStream::OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                                QuicByteCount data_length,
                                                bool fin_retransmitted) {
  QuicStream::OnStreamFrameRetransmitted(offset, data_length,
                                         fin_retransmitted);

  const QuicByteCount retransmitted_header_length =
      GetNumFrameHeadersInInterval(offset, data_length);
  QUICHE_DCHECK_LE(retransmitted_header_length, data_length);

  if (ack_listener_ != nullptr) {
    ack_listener_->OnPacketRetransmitted(data_length -
                                         retransmitted_header_length);
  }
}

QuicByteCount QuicSpdyStream::GetNumFrameHeadersInInterval(
    QuicStreamOffset offset, QuicByteCount data_length) const {
  QuicByteCount header_acked_length = 0;
  QuicIntervalSet<QuicStreamOffset> newly_acked(offset, offset + data_length);
  newly_acked.Intersection(unacked_frame_headers_offsets_);
  for (const auto& interval : newly_acked) {
    header_acked_length += interval.Length();
  }
  return header_acked_length;
}

bool QuicSpdyStream::OnHeadersFrameStart(QuicByteCount header_length,
                                         QuicByteCount payload_length) {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));
  QUICHE_DCHECK(!qpack_decoded_headers_accumulator_);

  if (spdy_session_->debug_visitor()) {
    spdy_session_->debug_visitor()->OnHeadersFrameReceived(id(),
                                                           payload_length);
  }

  headers_payload_length_ = payload_length;

  if (trailers_decompressed_) {
    stream_delegate()->OnStreamError(
        QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_SPDY_STREAM,
        "HEADERS frame received after trailing HEADERS.");
    return false;
  }

  sequencer()->MarkConsumed(body_manager_.OnNonBody(header_length));

  qpack_decoded_headers_accumulator_ =
      std::make_unique<QpackDecodedHeadersAccumulator>(
          id(), spdy_session_->qpack_decoder(), this,
          spdy_session_->max_inbound_header_list_size());

  return true;
}

bool QuicSpdyStream::OnHeadersFramePayload(absl::string_view payload) {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));
  QUICHE_DCHECK(qpack_decoded_headers_accumulator_);

  qpack_decoded_headers_accumulator_->Decode(payload);

  // |qpack_decoded_headers_accumulator_| is reset if an error is detected.
  if (!qpack_decoded_headers_accumulator_) {
    return false;
  }

  sequencer()->MarkConsumed(body_manager_.OnNonBody(payload.size()));
  return true;
}

bool QuicSpdyStream::OnHeadersFrameEnd() {
  QUICHE_DCHECK(VersionUsesHttp3(transport_version()));
  QUICHE_DCHECK(qpack_decoded_headers_accumulator_);

  qpack_decoded_headers_accumulator_->EndHeaderBlock();

  // If decoding is complete or an error is detected, then
  // |qpack_decoded_headers_accumulator_| is already reset.
  if (qpack_decoded_headers_accumulator_) {
    blocked_on_decoding_headers_ = true;
    return false;
  }

  return !sequencer()->IsClosed() && !reading_stopped();
}

void QuicSpdyStream::OnWebTransportStreamFrameType(
    QuicByteCount header_length, WebTransportSessionId session_id) {
  QUIC_DVLOG(1) << ENDPOINT << " Received WEBTRANSPORT_STREAM on stream "
                << id() << " for session " << session_id;
  sequencer()->MarkConsumed(header_length);

  if (headers_payload_length_ > 0 || headers_decompressed_) {
    QUIC_PEER_BUG(WEBTRANSPORT_STREAM received on HTTP request)
        << ENDPOINT << "Stream " << id()
        << " tried to convert to WebTransport, but it already "
           "has HTTP data on it";
    Reset(QUIC_STREAM_FRAME_UNEXPECTED);
  }
  if (QuicUtils::IsOutgoingStreamId(spdy_session_->version(), id(),
                                    spdy_session_->perspective())) {
    QUIC_PEER_BUG(WEBTRANSPORT_STREAM received on outgoing request)
        << ENDPOINT << "Stream " << id()
        << " tried to convert to WebTransport, but only the "
           "initiator of the stream can do it.";
    Reset(QUIC_STREAM_FRAME_UNEXPECTED);
  }

  QUICHE_DCHECK(web_transport_ == nullptr);
  web_transport_data_ =
      std::make_unique<WebTransportDataStream>(this, session_id);
  spdy_session_->AssociateIncomingWebTransportStreamWithSession(session_id,
                                                                id());
}

bool QuicSpdyStream::OnUnknownFrameStart(uint64_t frame_type,
                                         QuicByteCount header_length,
                                         QuicByteCount payload_length) {
  if (spdy_session_->debug_visitor()) {
    spdy_session_->debug_visitor()->OnUnknownFrameReceived(id(), frame_type,
                                                           payload_length);
  }

  // Ignore unknown frames, but consume frame header.
  QUIC_DVLOG(1) << ENDPOINT << "Discarding " << header_length
                << " byte long frame header of frame of unknown type "
                << frame_type << ".";
  sequencer()->MarkConsumed(body_manager_.OnNonBody(header_length));
  return true;
}

bool QuicSpdyStream::OnUnknownFramePayload(absl::string_view payload) {
  // Ignore unknown frames, but consume frame payload.
  QUIC_DVLOG(1) << ENDPOINT << "Discarding " << payload.size()
                << " bytes of payload of frame of unknown type.";
  sequencer()->MarkConsumed(body_manager_.OnNonBody(payload.size()));
  return true;
}

bool QuicSpdyStream::OnUnknownFrameEnd() { return true; }

size_t QuicSpdyStream::WriteHeadersImpl(
    spdy::SpdyHeaderBlock header_block, bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (!VersionUsesHttp3(transport_version())) {
    return spdy_session_->WriteHeadersOnHeadersStream(
        id(), std::move(header_block), fin, precedence(),
        std::move(ack_listener));
  }

  // Encode header list.
  QuicByteCount encoder_stream_sent_byte_count;
  std::string encoded_headers =
      spdy_session_->qpack_encoder()->EncodeHeaderList(
          id(), header_block, &encoder_stream_sent_byte_count);

  if (spdy_session_->debug_visitor()) {
    spdy_session_->debug_visitor()->OnHeadersFrameSent(id(), header_block);
  }

  // Write HEADERS frame.
  std::unique_ptr<char[]> headers_frame_header;
  const size_t headers_frame_header_length =
      HttpEncoder::SerializeHeadersFrameHeader(encoded_headers.size(),
                                               &headers_frame_header);
  unacked_frame_headers_offsets_.Add(
      send_buffer().stream_offset(),
      send_buffer().stream_offset() + headers_frame_header_length);

  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id()
                  << " is writing HEADERS frame header of length "
                  << headers_frame_header_length;
  WriteOrBufferData(absl::string_view(headers_frame_header.get(),
                                      headers_frame_header_length),
                    /* fin = */ false, /* ack_listener = */ nullptr);

  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id()
                  << " is writing HEADERS frame payload of length "
                  << encoded_headers.length() << " with fin " << fin;
  WriteOrBufferData(encoded_headers, fin, nullptr);

  QuicSpdySession::LogHeaderCompressionRatioHistogram(
      /* using_qpack = */ true,
      /* is_sent = */ true,
      encoded_headers.size() + encoder_stream_sent_byte_count,
      header_block.TotalBytesUsed());

  return encoded_headers.size();
}

bool QuicSpdyStream::CanWriteNewBodyData(QuicByteCount write_size) const {
  QUICHE_DCHECK_NE(0u, write_size);
  if (!VersionUsesHttp3(transport_version())) {
    return CanWriteNewData();
  }

  return CanWriteNewDataAfterData(
      HttpEncoder::GetDataFrameHeaderLength(write_size));
}

void QuicSpdyStream::MaybeProcessReceivedWebTransportHeaders() {
  if (!spdy_session_->SupportsWebTransport()) {
    return;
  }
  if (session()->perspective() != Perspective::IS_SERVER) {
    return;
  }
  QUICHE_DCHECK(IsValidWebTransportSessionId(id(), version()));

  std::string method;
  std::string protocol;
  absl::optional<QuicDatagramStreamId> flow_id;
  for (const auto& header : header_list_) {
    const std::string& header_name = header.first;
    const std::string& header_value = header.second;
    if (header_name == ":method") {
      if (!method.empty() || header_value.empty()) {
        return;
      }
      method = header_value;
    }
    if (header_name == ":protocol") {
      if (!protocol.empty() || header_value.empty()) {
        return;
      }
      protocol = header_value;
    }
    if (header_name == "datagram-flow-id") {
      if (spdy_session_->http_datagram_support() !=
          HttpDatagramSupport::kDraft00) {
        QUIC_DLOG(ERROR) << ENDPOINT
                         << "Rejecting WebTransport due to unexpected "
                            "Datagram-Flow-Id header";
        return;
      }
      if (flow_id.has_value() || header_value.empty()) {
        return;
      }
      QuicDatagramStreamId flow_id_out;
      if (!absl::SimpleAtoi(header_value, &flow_id_out)) {
        return;
      }
      flow_id = flow_id_out;
    }
  }

  if (method != "CONNECT" || protocol != "webtransport") {
    return;
  }

  if (spdy_session_->http_datagram_support() == HttpDatagramSupport::kDraft00) {
    if (!flow_id.has_value()) {
      QUIC_DLOG(ERROR)
          << ENDPOINT
          << "Rejecting WebTransport due to missing Datagram-Flow-Id header";
      return;
    }
    RegisterHttp3DatagramFlowId(*flow_id);
  }

  web_transport_ =
      std::make_unique<WebTransportHttp3>(spdy_session_, this, id());

  if (spdy_session_->http_datagram_support() != HttpDatagramSupport::kDraft00) {
    return;
  }
  // If we're in draft-ietf-masque-h3-datagram-00 mode, pretend we also received
  // a REGISTER_DATAGRAM_NO_CONTEXT capsule.
  // TODO(b/181256914) remove this when we remove support for
  // draft-ietf-masque-h3-datagram-00 in favor of later drafts.
  RegisterHttp3DatagramContextId(
      /*context_id=*/absl::nullopt, DatagramFormatType::WEBTRANSPORT,
      /*format_additional_data=*/absl::string_view(), web_transport_.get());
}

void QuicSpdyStream::MaybeProcessSentWebTransportHeaders(
    spdy::SpdyHeaderBlock& headers) {
  if (!spdy_session_->SupportsWebTransport()) {
    return;
  }
  if (session()->perspective() != Perspective::IS_CLIENT) {
    return;
  }
  QUICHE_DCHECK(IsValidWebTransportSessionId(id(), version()));

  const auto method_it = headers.find(":method");
  const auto protocol_it = headers.find(":protocol");
  if (method_it == headers.end() || protocol_it == headers.end()) {
    return;
  }
  if (method_it->second != "CONNECT" && protocol_it->second != "webtransport") {
    return;
  }

  if (spdy_session_->http_datagram_support() == HttpDatagramSupport::kDraft00) {
    headers["datagram-flow-id"] = absl::StrCat(id());
  }

  web_transport_ =
      std::make_unique<WebTransportHttp3>(spdy_session_, this, id());
}

void QuicSpdyStream::OnCanWriteNewData() {
  if (web_transport_data_ != nullptr) {
    web_transport_data_->adapter.OnCanWriteNewData();
  }
}

bool QuicSpdyStream::AssertNotWebTransportDataStream(
    absl::string_view operation) {
  if (web_transport_data_ != nullptr) {
    QUIC_BUG(Invalid operation on WebTransport stream)
        << "Attempted to " << operation << " on WebTransport data stream "
        << id() << " associated with session "
        << web_transport_data_->session_id;
    OnUnrecoverableError(QUIC_INTERNAL_ERROR,
                         absl::StrCat("Attempted to ", operation,
                                      " on WebTransport data stream"));
    return false;
  }
  return true;
}

void QuicSpdyStream::ConvertToWebTransportDataStream(
    WebTransportSessionId session_id) {
  if (send_buffer().stream_offset() != 0) {
    QUIC_BUG(Sending WEBTRANSPORT_STREAM when data already sent)
        << "Attempted to send a WEBTRANSPORT_STREAM frame when other data has "
           "already been sent on the stream.";
    OnUnrecoverableError(QUIC_INTERNAL_ERROR,
                         "Attempted to send a WEBTRANSPORT_STREAM frame when "
                         "other data has already been sent on the stream.");
    return;
  }

  std::unique_ptr<char[]> header;
  QuicByteCount header_size =
      HttpEncoder::SerializeWebTransportStreamFrameHeader(session_id, &header);
  if (header_size == 0) {
    QUIC_BUG(Failed to serialize WEBTRANSPORT_STREAM)
        << "Failed to serialize a WEBTRANSPORT_STREAM frame.";
    OnUnrecoverableError(QUIC_INTERNAL_ERROR,
                         "Failed to serialize a WEBTRANSPORT_STREAM frame.");
    return;
  }

  WriteOrBufferData(absl::string_view(header.get(), header_size), /*fin=*/false,
                    nullptr);
  web_transport_data_ =
      std::make_unique<WebTransportDataStream>(this, session_id);
  QUIC_DVLOG(1) << ENDPOINT << "Successfully opened WebTransport data stream "
                << id() << " for session " << session_id;
}

QuicSpdyStream::WebTransportDataStream::WebTransportDataStream(
    QuicSpdyStream* stream, WebTransportSessionId session_id)
    : session_id(session_id),
      adapter(stream->spdy_session_, stream, stream->sequencer()) {}

void QuicSpdyStream::HandleReceivedDatagram(
    absl::optional<QuicDatagramContextId> context_id,
    absl::string_view payload) {
  Http3DatagramVisitor* visitor;
  if (context_id.has_value()) {
    auto it = datagram_context_visitors_.find(context_id.value());
    if (it == datagram_context_visitors_.end()) {
      QUIC_DLOG(ERROR) << ENDPOINT
                       << "Received datagram without any visitor for context "
                       << context_id.value();
      return;
    }
    visitor = it->second;
  } else {
    if (datagram_no_context_visitor_ == nullptr) {
      QUIC_DLOG(ERROR)
          << ENDPOINT << "Received datagram without any visitor for no context";
      return;
    }
    visitor = datagram_no_context_visitor_;
  }
  visitor->OnHttp3Datagram(id(), context_id, payload);
}

bool QuicSpdyStream::OnCapsule(const Capsule& capsule) {
  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id() << " received capsule "
                  << capsule;
  if (!headers_decompressed_) {
    QUIC_PEER_BUG(capsule before headers)
        << ENDPOINT << "Stream " << id() << " received capsule " << capsule
        << " before headers";
    return false;
  }
  if (web_transport_ != nullptr && web_transport_->close_received()) {
    QUIC_PEER_BUG(capsule after close)
        << ENDPOINT << "Stream " << id() << " received capsule " << capsule
        << " after CLOSE_WEBTRANSPORT_SESSION.";
    return false;
  }
  switch (capsule.capsule_type()) {
    case CapsuleType::DATAGRAM: {
      HandleReceivedDatagram(capsule.datagram_capsule().context_id,
                             capsule.datagram_capsule().http_datagram_payload);
    } break;
    case CapsuleType::REGISTER_DATAGRAM_CONTEXT:
      if (datagram_registration_visitor_ == nullptr) {
        QUIC_DLOG(ERROR) << ENDPOINT << "Received capsule " << capsule
                         << " without any registration visitor";
        return false;
      }
      datagram_registration_visitor_->OnContextReceived(
          id(), capsule.register_datagram_context_capsule().context_id,
          capsule.register_datagram_context_capsule().format_type,
          capsule.register_datagram_context_capsule().format_additional_data);
      break;
    case CapsuleType::REGISTER_DATAGRAM_NO_CONTEXT:
      if (datagram_registration_visitor_ == nullptr) {
        QUIC_DLOG(ERROR) << ENDPOINT << "Received capsule " << capsule
                         << " without any registration visitor";
        return false;
      }
      datagram_registration_visitor_->OnContextReceived(
          id(), /*context_id=*/absl::nullopt,
          capsule.register_datagram_no_context_capsule().format_type,
          capsule.register_datagram_no_context_capsule()
              .format_additional_data);
      break;
    case CapsuleType::CLOSE_DATAGRAM_CONTEXT:
      if (datagram_registration_visitor_ == nullptr) {
        QUIC_DLOG(ERROR) << ENDPOINT << "Received capsule " << capsule
                         << " without any registration visitor";
        return false;
      }
      datagram_registration_visitor_->OnContextClosed(
          id(), capsule.close_datagram_context_capsule().context_id,
          capsule.close_datagram_context_capsule().close_code,
          capsule.close_datagram_context_capsule().close_details);
      break;
    case CapsuleType::CLOSE_WEBTRANSPORT_SESSION:
      if (web_transport_ == nullptr) {
        QUIC_DLOG(ERROR) << ENDPOINT << "Received capsule " << capsule
                         << " for a non-WebTransport stream.";
        return false;
      }
      web_transport_->OnCloseReceived(
          capsule.close_web_transport_session_capsule().error_code,
          capsule.close_web_transport_session_capsule().error_message);
      break;
  }
  return true;
}

void QuicSpdyStream::OnCapsuleParseFailure(const std::string& error_message) {
  QUIC_DLOG(ERROR) << ENDPOINT << "Capsule parse failure: " << error_message;
  Reset(QUIC_BAD_APPLICATION_PAYLOAD);
}

void QuicSpdyStream::WriteCapsule(const Capsule& capsule, bool fin) {
  QUIC_DLOG(INFO) << ENDPOINT << "Stream " << id() << " sending capsule "
                  << capsule;
  QuicBuffer serialized_capsule = SerializeCapsule(
      capsule,
      spdy_session_->connection()->helper()->GetStreamSendBufferAllocator());
  QUICHE_DCHECK_GT(serialized_capsule.size(), 0u);
  WriteOrBufferBody(serialized_capsule.AsStringView(), /*fin=*/fin);
}

void QuicSpdyStream::WriteGreaseCapsule() {
  // GREASE capsulde IDs have a form of 41 * N + 23.
  QuicRandom* random = spdy_session_->connection()->random_generator();
  uint64_t type = random->InsecureRandUint64() >> 4;
  type = (type / 41) * 41 + 23;
  QUICHE_DCHECK_EQ((type - 23) % 41, 0u);

  constexpr size_t kMaxLength = 64;
  size_t length = random->InsecureRandUint64() % kMaxLength;
  std::string bytes(length, '\0');
  random->InsecureRandBytes(&bytes[0], bytes.size());
  Capsule capsule = Capsule::Unknown(type, bytes);
  WriteCapsule(capsule, /*fin=*/false);
}

MessageStatus QuicSpdyStream::SendHttp3Datagram(
    absl::optional<QuicDatagramContextId> context_id,
    absl::string_view payload) {
  QuicDatagramStreamId stream_id =
      datagram_flow_id_.has_value() ? datagram_flow_id_.value() : id();
  return spdy_session_->SendHttp3Datagram(stream_id, context_id, payload);
}

void QuicSpdyStream::RegisterHttp3DatagramRegistrationVisitor(
    Http3DatagramRegistrationVisitor* visitor) {
  if (visitor == nullptr) {
    QUIC_BUG(null datagram registration visitor)
        << ENDPOINT << "Null datagram registration visitor for" << id();
    return;
  }
  if (datagram_registration_visitor_ != nullptr) {
    QUIC_BUG(double datagram registration visitor)
        << ENDPOINT << "Double datagram registration visitor for" << id();
    return;
  }
  QUIC_DLOG(INFO) << ENDPOINT << "Registering datagram stream ID " << id();
  datagram_registration_visitor_ = visitor;
  QUICHE_DCHECK(!capsule_parser_);
  capsule_parser_.reset(new CapsuleParser(this));
}

void QuicSpdyStream::UnregisterHttp3DatagramRegistrationVisitor() {
  QUIC_BUG_IF(h3 datagram unregister unknown stream ID,
              datagram_registration_visitor_ == nullptr)
      << ENDPOINT
      << "Attempted to unregister unknown HTTP/3 datagram stream ID " << id();
  QUIC_DLOG(INFO) << ENDPOINT << "Unregistering datagram stream ID " << id();
  datagram_registration_visitor_ = nullptr;
}

void QuicSpdyStream::MoveHttp3DatagramRegistration(
    Http3DatagramRegistrationVisitor* visitor) {
  QUIC_BUG_IF(h3 datagram move unknown stream ID,
              datagram_registration_visitor_ == nullptr)
      << ENDPOINT << "Attempted to move unknown HTTP/3 datagram stream ID "
      << id();
  QUIC_DLOG(INFO) << ENDPOINT << "Moving datagram stream ID " << id();
  datagram_registration_visitor_ = visitor;
}

void QuicSpdyStream::RegisterHttp3DatagramContextId(
    absl::optional<QuicDatagramContextId> context_id,
    DatagramFormatType format_type, absl::string_view format_additional_data,
    Http3DatagramVisitor* visitor) {
  if (visitor == nullptr) {
    QUIC_BUG(null datagram visitor)
        << ENDPOINT << "Null datagram visitor for stream ID " << id()
        << " context ID "
        << (context_id.has_value() ? absl::StrCat(context_id.value()) : "none");
    return;
  }
  if (datagram_registration_visitor_ == nullptr) {
    QUIC_BUG(context registration without registration visitor)
        << ENDPOINT << "Cannot register context ID "
        << (context_id.has_value() ? absl::StrCat(context_id.value()) : "none")
        << " without registration visitor for stream ID " << id();
    return;
  }
  QUIC_DLOG(INFO) << ENDPOINT << "Registering datagram context ID "
                  << (context_id.has_value() ? absl::StrCat(context_id.value())
                                             : "none")
                  << " with stream ID " << id();

  if (context_id.has_value()) {
    if (datagram_no_context_visitor_ != nullptr) {
      QUIC_BUG(h3 datagram context ID mix1)
          << ENDPOINT
          << "Attempted to mix registrations without and with context IDs "
             "for stream ID "
          << id();
      return;
    }
    auto insertion_result =
        datagram_context_visitors_.insert({context_id.value(), visitor});
    if (!insertion_result.second) {
      QUIC_BUG(h3 datagram double context registration)
          << ENDPOINT << "Attempted to doubly register HTTP/3 stream ID "
          << id() << " context ID " << context_id.value();
      return;
    }
    capsule_parser_->set_datagram_context_id_present(true);
  } else {
    // Registration without a context ID.
    if (!datagram_context_visitors_.empty()) {
      QUIC_BUG(h3 datagram context ID mix2)
          << ENDPOINT
          << "Attempted to mix registrations with and without context IDs "
             "for stream ID "
          << id();
      return;
    }
    if (datagram_no_context_visitor_ != nullptr) {
      QUIC_BUG(h3 datagram double no context registration)
          << ENDPOINT << "Attempted to doubly register HTTP/3 stream ID "
          << id() << " with no context ID";
      return;
    }
    datagram_no_context_visitor_ = visitor;
    capsule_parser_->set_datagram_context_id_present(false);
  }
  if (spdy_session_->http_datagram_support() == HttpDatagramSupport::kDraft04) {
    const bool is_client = session()->perspective() == Perspective::IS_CLIENT;
    if (context_id.has_value()) {
      const bool is_client_context = context_id.value() % 2 == 0;
      if (is_client == is_client_context) {
        QuicConnection::ScopedPacketFlusher flusher(
            spdy_session_->connection());
        WriteGreaseCapsule();
        WriteCapsule(Capsule::RegisterDatagramContext(
            context_id.value(), format_type, format_additional_data));
        WriteGreaseCapsule();
      }
    } else if (is_client) {
      QuicConnection::ScopedPacketFlusher flusher(spdy_session_->connection());
      WriteGreaseCapsule();
      WriteCapsule(Capsule::RegisterDatagramNoContext(format_type,
                                                      format_additional_data));
      WriteGreaseCapsule();
    }
  }
}

void QuicSpdyStream::UnregisterHttp3DatagramContextId(
    absl::optional<QuicDatagramContextId> context_id) {
  if (datagram_registration_visitor_ == nullptr) {
    QUIC_BUG(context unregistration without registration visitor)
        << ENDPOINT << "Cannot unregister context ID "
        << (context_id.has_value() ? absl::StrCat(context_id.value()) : "none")
        << " without registration visitor for stream ID " << id();
    return;
  }
  QUIC_DLOG(INFO) << ENDPOINT << "Unregistering datagram context ID "
                  << (context_id.has_value() ? absl::StrCat(context_id.value())
                                             : "none")
                  << " with stream ID " << id();
  if (context_id.has_value()) {
    size_t num_erased = datagram_context_visitors_.erase(context_id.value());
    QUIC_BUG_IF(h3 datagram unregister unknown context, num_erased != 1)
        << "Attempted to unregister unknown HTTP/3 context ID "
        << context_id.value() << " on stream ID " << id();
  } else {
    // Unregistration without a context ID.
    QUIC_BUG_IF(h3 datagram unknown context unregistration,
                datagram_no_context_visitor_ == nullptr)
        << "Attempted to unregister unknown no context on HTTP/3 stream ID "
        << id();
    datagram_no_context_visitor_ = nullptr;
  }
  if (spdy_session_->http_datagram_support() == HttpDatagramSupport::kDraft04 &&
      context_id.has_value()) {
    WriteCapsule(Capsule::CloseDatagramContext(context_id.value()));
  }
}

void QuicSpdyStream::MoveHttp3DatagramContextIdRegistration(
    absl::optional<QuicDatagramContextId> context_id,
    Http3DatagramVisitor* visitor) {
  if (datagram_registration_visitor_ == nullptr) {
    QUIC_BUG(context move without registration visitor)
        << ENDPOINT << "Cannot move context ID "
        << (context_id.has_value() ? absl::StrCat(context_id.value()) : "none")
        << " without registration visitor for stream ID " << id();
    return;
  }
  QUIC_DLOG(INFO) << ENDPOINT << "Moving datagram context ID "
                  << (context_id.has_value() ? absl::StrCat(context_id.value())
                                             : "none")
                  << " with stream ID " << id();
  if (context_id.has_value()) {
    QUIC_BUG_IF(h3 datagram move unknown context,
                !datagram_context_visitors_.contains(context_id.value()))
        << ENDPOINT << "Attempted to move unknown context ID "
        << context_id.value() << " on stream ID " << id();
    datagram_context_visitors_[context_id.value()] = visitor;
    return;
  }
  // Move without a context ID.
  QUIC_BUG_IF(h3 datagram unknown context move,
              datagram_no_context_visitor_ == nullptr)
      << "Attempted to move unknown no context on HTTP/3 stream ID " << id();
  datagram_no_context_visitor_ = visitor;
}

void QuicSpdyStream::SetMaxDatagramTimeInQueue(
    QuicTime::Delta max_time_in_queue) {
  spdy_session_->SetMaxDatagramTimeInQueueForStreamId(id(), max_time_in_queue);
}

QuicDatagramContextId QuicSpdyStream::GetNextDatagramContextId() {
  QuicDatagramContextId result = datagram_next_available_context_id_;
  datagram_next_available_context_id_ += kDatagramContextIdIncrement;
  return result;
}

void QuicSpdyStream::OnDatagramReceived(QuicDataReader* reader) {
  absl::optional<QuicDatagramContextId> context_id;
  const bool context_id_present = !datagram_context_visitors_.empty();
  if (context_id_present) {
    QuicDatagramContextId parsed_context_id;
    if (!reader->ReadVarInt62(&parsed_context_id)) {
      QUIC_DLOG(ERROR) << "Failed to parse context ID in received HTTP/3 "
                          "datagram on stream ID "
                       << id();
      return;
    }
    context_id = parsed_context_id;
  }
  absl::string_view payload = reader->ReadRemainingPayload();
  HandleReceivedDatagram(context_id, payload);
}

QuicByteCount QuicSpdyStream::GetMaxDatagramSize(
    absl::optional<QuicDatagramContextId> context_id) const {
  QuicByteCount prefix_size = 0;
  switch (spdy_session_->http_datagram_support()) {
    case HttpDatagramSupport::kDraft00:
      if (!datagram_flow_id_.has_value()) {
        QUIC_BUG(GetMaxDatagramSize with no flow ID)
            << "GetMaxDatagramSize() called when no flow ID available";
        break;
      }
      prefix_size = QuicDataWriter::GetVarInt62Len(*datagram_flow_id_);
      break;
    case HttpDatagramSupport::kDraft04:
      prefix_size =
          QuicDataWriter::GetVarInt62Len(id() / kHttpDatagramStreamIdDivisor);
      break;
    case HttpDatagramSupport::kNone:
    case HttpDatagramSupport::kDraft00And04:
      QUIC_BUG(GetMaxDatagramSize called with no datagram support)
          << "GetMaxDatagramSize() called when no HTTP/3 datagram support has "
             "been negotiated.  Support value: "
          << spdy_session_->http_datagram_support();
      break;
  }
  // If the logic above fails, use the largest possible value as the safe one.
  if (prefix_size == 0) {
    prefix_size = 8;
  }

  if (context_id.has_value()) {
    QUIC_BUG_IF(
        context_id with draft00 in GetMaxDatagramSize,
        spdy_session_->http_datagram_support() == HttpDatagramSupport::kDraft00)
        << "GetMaxDatagramSize() called with a context ID specified, but "
           "draft00 does not support contexts.";
    prefix_size += QuicDataWriter::GetVarInt62Len(*context_id);
  }

  QuicByteCount max_datagram_size =
      session()->GetGuaranteedLargestMessagePayload();
  if (max_datagram_size < prefix_size) {
    QUIC_BUG(max_datagram_size smaller than prefix_size)
        << "GetGuaranteedLargestMessagePayload() returned a datagram size that "
           "is not sufficient to fit stream and/or context ID into it.";
    return 0;
  }
  return max_datagram_size - prefix_size;
}

void QuicSpdyStream::RegisterHttp3DatagramFlowId(QuicDatagramStreamId flow_id) {
  datagram_flow_id_ = flow_id;
  spdy_session_->RegisterHttp3DatagramFlowId(datagram_flow_id_.value(), id());
}

void QuicSpdyStream::HandleBodyAvailable() {
  if (!capsule_parser_) {
    OnBodyAvailable();
    return;
  }
  while (body_manager_.HasBytesToRead()) {
    iovec iov;
    int num_iov = GetReadableRegions(&iov, /*iov_len=*/1);
    if (num_iov == 0) {
      break;
    }
    if (!capsule_parser_->IngestCapsuleFragment(absl::string_view(
            reinterpret_cast<const char*>(iov.iov_base), iov.iov_len))) {
      break;
    }
    MarkConsumed(iov.iov_len);
  }
  // If we received a FIN, make sure that there isn't a partial capsule buffered
  // in the capsule parser.
  if (sequencer()->IsClosed()) {
    capsule_parser_->ErrorIfThereIsRemainingBufferedData();
    if (web_transport_ != nullptr) {
      web_transport_->OnConnectStreamFinReceived();
    }
    OnFinRead();
  }
}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
