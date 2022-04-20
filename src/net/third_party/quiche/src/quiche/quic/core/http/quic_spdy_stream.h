// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The base class for streams which deliver data to/from an application.
// In each direction, the data on such a stream first contains compressed
// headers then body data.

#ifndef QUICHE_QUIC_CORE_HTTP_QUIC_SPDY_STREAM_H_
#define QUICHE_QUIC_CORE_HTTP_QUIC_SPDY_STREAM_H_

#include <sys/types.h>

#include <cstddef>
#include <list>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/http/capsule.h"
#include "quiche/quic/core/http/http_decoder.h"
#include "quiche/quic/core/http/http_encoder.h"
#include "quiche/quic/core/http/quic_header_list.h"
#include "quiche/quic/core/http/quic_spdy_stream_body_manager.h"
#include "quiche/quic/core/http/web_transport_stream_adapter.h"
#include "quiche/quic/core/qpack/qpack_decoded_headers_accumulator.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_stream.h"
#include "quiche/quic/core/quic_stream_sequencer.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/web_transport_interface.h"
#include "quiche/quic/platform/api/quic_export.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/common/platform/api/quiche_mem_slice.h"
#include "quiche/spdy/core/spdy_framer.h"
#include "quiche/spdy/core/spdy_header_block.h"

namespace quic {

namespace test {
class QuicSpdyStreamPeer;
class QuicStreamPeer;
}  // namespace test

class QuicSpdySession;
class WebTransportHttp3;

// A QUIC stream that can send and receive HTTP2 (SPDY) headers.
class QUIC_EXPORT_PRIVATE QuicSpdyStream
    : public QuicStream,
      public CapsuleParser::Visitor,
      public QpackDecodedHeadersAccumulator::Visitor {
 public:
  // Visitor receives callbacks from the stream.
  class QUIC_EXPORT_PRIVATE Visitor {
   public:
    Visitor() {}
    Visitor(const Visitor&) = delete;
    Visitor& operator=(const Visitor&) = delete;

    // Called when the stream is closed.
    virtual void OnClose(QuicSpdyStream* stream) = 0;

    // Allows subclasses to override and do work.
    virtual void OnPromiseHeadersComplete(QuicStreamId /*promised_id*/,
                                          size_t /*frame_len*/) {}

   protected:
    virtual ~Visitor() {}
  };

  QuicSpdyStream(QuicStreamId id, QuicSpdySession* spdy_session,
                 StreamType type);
  QuicSpdyStream(PendingStream* pending, QuicSpdySession* spdy_session);
  QuicSpdyStream(const QuicSpdyStream&) = delete;
  QuicSpdyStream& operator=(const QuicSpdyStream&) = delete;
  ~QuicSpdyStream() override;

  // QuicStream implementation
  void OnClose() override;

  // Override to maybe close the write side after writing.
  void OnCanWrite() override;

  // Called by the session when headers with a priority have been received
  // for this stream.  This method will only be called for server streams.
  virtual void OnStreamHeadersPriority(
      const spdy::SpdyStreamPrecedence& precedence);

  // Called by the session when decompressed headers have been completely
  // delivered to this stream.  If |fin| is true, then this stream
  // should be closed; no more data will be sent by the peer.
  virtual void OnStreamHeaderList(bool fin, size_t frame_len,
                                  const QuicHeaderList& header_list);

  // Called by the session when decompressed push promise headers have
  // been completely delivered to this stream.
  virtual void OnPromiseHeaderList(QuicStreamId promised_id, size_t frame_len,
                                   const QuicHeaderList& header_list);

  // Called by the session when a PRIORITY frame has been been received for this
  // stream. This method will only be called for server streams.
  void OnPriorityFrame(const spdy::SpdyStreamPrecedence& precedence);

  // Override the base class to not discard response when receiving
  // QUIC_STREAM_NO_ERROR.
  void OnStreamReset(const QuicRstStreamFrame& frame) override;
  void ResetWithError(QuicResetStreamError error) override;
  bool OnStopSending(QuicResetStreamError error) override;

  // Called by the sequencer when new data is available. Decodes the data and
  // calls OnBodyAvailable() to pass to the upper layer.
  void OnDataAvailable() override;

  // Called in OnDataAvailable() after it finishes the decoding job.
  virtual void OnBodyAvailable() = 0;

  // Writes the headers contained in |header_block| on the dedicated headers
  // stream or on this stream, depending on VersionUsesHttp3().  Returns the
  // number of bytes sent, including data sent on the encoder stream when using
  // QPACK.
  virtual size_t WriteHeaders(
      spdy::SpdyHeaderBlock header_block, bool fin,
      quiche::QuicheReferenceCountedPointer<QuicAckListenerInterface>
          ack_listener);

  // Sends |data| to the peer, or buffers if it can't be sent immediately.
  void WriteOrBufferBody(absl::string_view data, bool fin);

  // Writes the trailers contained in |trailer_block| on the dedicated headers
  // stream or on this stream, depending on VersionUsesHttp3().  Trailers will
  // always have the FIN flag set.  Returns the number of bytes sent, including
  // data sent on the encoder stream when using QPACK.
  virtual size_t WriteTrailers(
      spdy::SpdyHeaderBlock trailer_block,
      quiche::QuicheReferenceCountedPointer<QuicAckListenerInterface>
          ack_listener);

  // Override to report newly acked bytes via ack_listener_.
  bool OnStreamFrameAcked(QuicStreamOffset offset, QuicByteCount data_length,
                          bool fin_acked, QuicTime::Delta ack_delay_time,
                          QuicTime receive_timestamp,
                          QuicByteCount* newly_acked_length) override;

  // Override to report bytes retransmitted via ack_listener_.
  void OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                  QuicByteCount data_length,
                                  bool fin_retransmitted) override;

  // Does the same thing as WriteOrBufferBody except this method takes iovec
  // as the data input. Right now it only calls WritevData.
  QuicConsumedData WritevBody(const struct iovec* iov, int count, bool fin);

  // Does the same thing as WriteOrBufferBody except this method takes
  // memslicespan as the data input. Right now it only calls WriteMemSlices.
  QuicConsumedData WriteBodySlices(absl::Span<quiche::QuicheMemSlice> slices,
                                   bool fin);

  // Marks the trailers as consumed. This applies to the case where this object
  // receives headers and trailers as QuicHeaderLists via calls to
  // OnStreamHeaderList(). Trailer data will be consumed from the sequencer only
  // once all body data has been consumed.
  void MarkTrailersConsumed();

  // Clears |header_list_|.
  void ConsumeHeaderList();

  // This block of functions wraps the sequencer's functions of the same
  // name.  These methods return uncompressed data until that has
  // been fully processed.  Then they simply delegate to the sequencer.
  virtual size_t Readv(const struct iovec* iov, size_t iov_len);
  virtual int GetReadableRegions(iovec* iov, size_t iov_len) const;
  void MarkConsumed(size_t num_bytes);

  // Returns true if header contains a valid 3-digit status and parse the status
  // code to |status_code|.
  static bool ParseHeaderStatusCode(const spdy::SpdyHeaderBlock& header,
                                    int* status_code);

  // Returns true when all data from the peer has been read and consumed,
  // including the fin.
  bool IsDoneReading() const;
  bool HasBytesToRead() const;

  void set_visitor(Visitor* visitor) { visitor_ = visitor; }

  bool headers_decompressed() const { return headers_decompressed_; }

  // Returns total amount of body bytes that have been read.
  uint64_t total_body_bytes_read() const;

  const QuicHeaderList& header_list() const { return header_list_; }

  bool trailers_decompressed() const { return trailers_decompressed_; }

  // Returns whatever trailers have been received for this stream.
  const spdy::SpdyHeaderBlock& received_trailers() const {
    return received_trailers_;
  }

  // Returns true if headers have been fully read and consumed.
  bool FinishedReadingHeaders() const;

  // Returns true if FIN has been received and either trailers have been fully
  // read and consumed or there are no trailers.
  bool FinishedReadingTrailers() const;

  // Returns true if the sequencer has delivered the FIN, and no more body bytes
  // will be available.
  bool IsSequencerClosed() { return sequencer()->IsClosed(); }

  // QpackDecodedHeadersAccumulator::Visitor implementation.
  void OnHeadersDecoded(QuicHeaderList headers,
                        bool header_list_size_limit_exceeded) override;
  void OnHeaderDecodingError(QuicErrorCode error_code,
                             absl::string_view error_message) override;

  QuicSpdySession* spdy_session() const { return spdy_session_; }

  // Send PRIORITY_UPDATE frame and update |last_sent_urgency_| if
  // |last_sent_urgency_| is different from current priority.
  void MaybeSendPriorityUpdateFrame() override;

  // Returns the WebTransport session owned by this stream, if one exists.
  WebTransportHttp3* web_transport() { return web_transport_.get(); }

  // Returns the WebTransport data stream associated with this QUIC stream, or
  // null if this is not a WebTransport data stream.
  WebTransportStream* web_transport_stream() {
    if (web_transport_data_ == nullptr) {
      return nullptr;
    }
    return &web_transport_data_->adapter;
  }

  // Sends a WEBTRANSPORT_STREAM frame and sets up the appropriate metadata.
  void ConvertToWebTransportDataStream(WebTransportSessionId session_id);

  void OnCanWriteNewData() override;

  // If this stream is a WebTransport data stream, closes the connection with an
  // error, and returns false.
  bool AssertNotWebTransportDataStream(absl::string_view operation);

  // Indicates whether a call to WriteBodySlices will be successful and not
  // rejected due to buffer being full.  |write_size| must be non-zero.
  bool CanWriteNewBodyData(QuicByteCount write_size) const;

  // From CapsuleParser::Visitor.
  bool OnCapsule(const Capsule& capsule) override;
  void OnCapsuleParseFailure(const std::string& error_message) override;

  // Sends an HTTP/3 datagram. The stream and context IDs are not part of
  // |payload|.
  MessageStatus SendHttp3Datagram(
      absl::optional<QuicDatagramContextId> context_id,
      absl::string_view payload);

  class QUIC_EXPORT_PRIVATE Http3DatagramVisitor {
   public:
    virtual ~Http3DatagramVisitor() {}

    // Called when an HTTP/3 datagram is received. |payload| does not contain
    // the stream or context IDs. Note that this contains the stream ID even if
    // flow IDs from draft-ietf-masque-h3-datagram-00 are in use.
    virtual void OnHttp3Datagram(
        QuicStreamId stream_id,
        absl::optional<QuicDatagramContextId> context_id,
        absl::string_view payload) = 0;
  };

  class QUIC_EXPORT_PRIVATE Http3DatagramRegistrationVisitor {
   public:
    virtual ~Http3DatagramRegistrationVisitor() {}

    // Called when a REGISTER_DATAGRAM_CONTEXT or REGISTER_DATAGRAM_NO_CONTEXT
    // capsule is received. Note that this contains the stream ID even if flow
    // IDs from draft-ietf-masque-h3-datagram-00 are in use.
    virtual void OnContextReceived(
        QuicStreamId stream_id,
        absl::optional<QuicDatagramContextId> context_id,
        DatagramFormatType format_type,
        absl::string_view format_additional_data) = 0;

    // Called when a CLOSE_DATAGRAM_CONTEXT capsule is received. Note that this
    // contains the stream ID even if flow IDs from
    // draft-ietf-masque-h3-datagram-00 are in use.
    virtual void OnContextClosed(
        QuicStreamId stream_id,
        absl::optional<QuicDatagramContextId> context_id,
        ContextCloseCode close_code, absl::string_view close_details) = 0;
  };

  // Registers |visitor| to receive HTTP/3 datagram context registrations. This
  // must not be called without first calling
  // UnregisterHttp3DatagramRegistrationVisitor. |visitor| must be valid until a
  // corresponding call to UnregisterHttp3DatagramRegistrationVisitor.
  void RegisterHttp3DatagramRegistrationVisitor(
      Http3DatagramRegistrationVisitor* visitor,
      bool use_datagram_contexts = false);

  // Unregisters for HTTP/3 datagram context registrations. Must not be called
  // unless previously registered.
  void UnregisterHttp3DatagramRegistrationVisitor();

  // Moves an HTTP/3 datagram registration to a different visitor. Mainly meant
  // to be used by the visitors' move operators.
  void MoveHttp3DatagramRegistration(Http3DatagramRegistrationVisitor* visitor);

  // Registers |visitor| to receive HTTP/3 datagrams for optional context ID
  // |context_id|. This must not be called on a previously registered context ID
  // without first calling UnregisterHttp3DatagramContextId. |visitor| must be
  // valid until a corresponding call to UnregisterHttp3DatagramContextId. If
  // this method is called multiple times, the context ID MUST either be always
  // present, or always absent.
  void RegisterHttp3DatagramContextId(
      absl::optional<QuicDatagramContextId> context_id,
      DatagramFormatType format_type, absl::string_view format_additional_data,
      Http3DatagramVisitor* visitor);

  // Unregisters an HTTP/3 datagram context ID. Must be called on a previously
  // registered context.
  void UnregisterHttp3DatagramContextId(
      absl::optional<QuicDatagramContextId> context_id);

  // Moves an HTTP/3 datagram context ID to a different visitor. Mainly meant
  // to be used by the visitors' move operators.
  void MoveHttp3DatagramContextIdRegistration(
      absl::optional<QuicDatagramContextId> context_id,
      Http3DatagramVisitor* visitor);

  // Sets max datagram time in queue.
  void SetMaxDatagramTimeInQueue(QuicTime::Delta max_time_in_queue);

  // Generates a new HTTP/3 datagram context ID for this stream. A datagram
  // registration visitor must be currently registered on this stream.
  QuicDatagramContextId GetNextDatagramContextId();

  void OnDatagramReceived(QuicDataReader* reader);

  void RegisterHttp3DatagramFlowId(QuicDatagramStreamId flow_id);

  QuicByteCount GetMaxDatagramSize(
      absl::optional<QuicDatagramContextId> context_id) const;

  // Writes |capsule| onto the DATA stream.
  void WriteCapsule(const Capsule& capsule, bool fin = false);

  void WriteGreaseCapsule();

 protected:
  // Called when the received headers are too large. By default this will
  // reset the stream.
  virtual void OnHeadersTooLarge();

  virtual void OnInitialHeadersComplete(bool fin, size_t frame_len,
                                        const QuicHeaderList& header_list);
  virtual void OnTrailingHeadersComplete(bool fin, size_t frame_len,
                                         const QuicHeaderList& header_list);
  virtual size_t WriteHeadersImpl(
      spdy::SpdyHeaderBlock header_block, bool fin,
      quiche::QuicheReferenceCountedPointer<QuicAckListenerInterface>
          ack_listener);

  Visitor* visitor() { return visitor_; }

  void set_headers_decompressed(bool val) { headers_decompressed_ = val; }

  void set_ack_listener(
      quiche::QuicheReferenceCountedPointer<QuicAckListenerInterface>
          ack_listener) {
    ack_listener_ = std::move(ack_listener);
  }

  void OnWriteSideInDataRecvdState() override;

  virtual bool AreHeadersValid(const QuicHeaderList& header_list) const;

  // Reset stream upon invalid request headers.
  virtual void OnInvalidHeaders();

 private:
  friend class test::QuicSpdyStreamPeer;
  friend class test::QuicStreamPeer;
  friend class QuicStreamUtils;
  class HttpDecoderVisitor;

  struct QUIC_EXPORT_PRIVATE WebTransportDataStream {
    WebTransportDataStream(QuicSpdyStream* stream,
                           WebTransportSessionId session_id);

    WebTransportSessionId session_id;
    WebTransportStreamAdapter adapter;
  };

  // Reason codes for `qpack_decoded_headers_accumulator_` being nullptr.
  enum class QpackDecodedHeadersAccumulatorResetReason {
    // `qpack_decoded_headers_accumulator_` was default constructed to nullptr.
    kUnSet = 0,
    // `qpack_decoded_headers_accumulator_` was reset in the corresponding
    // method.
    kResetInOnHeadersDecoded = 1,
    kResetInOnHeaderDecodingError = 2,
    kResetInOnStreamReset1 = 3,
    kResetInOnStreamReset2 = 4,
    kResetInResetWithError = 5,
    kResetInOnClose = 6,
  };

  // Called by HttpDecoderVisitor.
  bool OnDataFrameStart(QuicByteCount header_length,
                        QuicByteCount payload_length);
  bool OnDataFramePayload(absl::string_view payload);
  bool OnDataFrameEnd();
  bool OnHeadersFrameStart(QuicByteCount header_length,
                           QuicByteCount payload_length);
  bool OnHeadersFramePayload(absl::string_view payload);
  bool OnHeadersFrameEnd();
  void OnWebTransportStreamFrameType(QuicByteCount header_length,
                                     WebTransportSessionId session_id);
  bool OnUnknownFrameStart(uint64_t frame_type, QuicByteCount header_length,
                           QuicByteCount payload_length);
  bool OnUnknownFramePayload(absl::string_view payload);
  bool OnUnknownFrameEnd();

  // Given the interval marked by [|offset|, |offset| + |data_length|), return
  // the number of frame header bytes contained in it.
  QuicByteCount GetNumFrameHeadersInInterval(QuicStreamOffset offset,
                                             QuicByteCount data_length) const;

  void MaybeProcessSentWebTransportHeaders(spdy::SpdyHeaderBlock& headers);
  void MaybeProcessReceivedWebTransportHeaders();

  // Writes HTTP/3 DATA frame header. If |force_write| is true, use
  // WriteOrBufferData if send buffer cannot accomodate the header + data.
  ABSL_MUST_USE_RESULT bool WriteDataFrameHeader(QuicByteCount data_length,
                                                 bool force_write);

  // Simply calls OnBodyAvailable() unless capsules are in use, in which case
  // pass the capsule fragments to the capsule manager.
  void HandleBodyAvailable();

  // Called when a datagram frame or capsule is received.
  void HandleReceivedDatagram(absl::optional<QuicDatagramContextId> context_id,
                              absl::string_view payload);

  // Whether datagram contexts should be used on this stream.
  bool ShouldUseDatagramContexts() const;

  QuicSpdySession* spdy_session_;

  bool on_body_available_called_because_sequencer_is_closed_;

  Visitor* visitor_;

  // True if read side processing is blocked while waiting for callback from
  // QPACK decoder.
  bool blocked_on_decoding_headers_;
  // True if the headers have been completely decompressed.
  bool headers_decompressed_;
  // True if uncompressed headers or trailers exceed maximum allowed size
  // advertised to peer via SETTINGS_MAX_HEADER_LIST_SIZE.
  bool header_list_size_limit_exceeded_;
  // Contains a copy of the decompressed header (name, value) pairs until they
  // are consumed via Readv.
  QuicHeaderList header_list_;
  // Length of most recently received HEADERS frame payload.
  QuicByteCount headers_payload_length_;

  // True if the trailers have been completely decompressed.
  bool trailers_decompressed_;
  // True if the trailers have been consumed.
  bool trailers_consumed_;

  // The parsed trailers received from the peer.
  spdy::SpdyHeaderBlock received_trailers_;

  // Headers accumulator for decoding HEADERS frame payload.
  std::unique_ptr<QpackDecodedHeadersAccumulator>
      qpack_decoded_headers_accumulator_;
  // Reason for `qpack_decoded_headers_accumulator_` being nullptr.
  QpackDecodedHeadersAccumulatorResetReason
      qpack_decoded_headers_accumulator_reset_reason_;
  // Visitor of the HttpDecoder.
  std::unique_ptr<HttpDecoderVisitor> http_decoder_visitor_;
  // HttpDecoder for processing raw incoming stream frames.
  HttpDecoder decoder_;
  // Object that manages references to DATA frame payload fragments buffered by
  // the sequencer and calculates how much data should be marked consumed with
  // the sequencer each time new stream data is processed.
  QuicSpdyStreamBodyManager body_manager_;

  std::unique_ptr<CapsuleParser> capsule_parser_;

  // Sequencer offset keeping track of how much data HttpDecoder has processed.
  // Initial value is zero for fresh streams, or sequencer()->NumBytesConsumed()
  // at time of construction if a PendingStream is converted to account for the
  // length of the unidirectional stream type at the beginning of the stream.
  QuicStreamOffset sequencer_offset_;

  // True when inside an HttpDecoder::ProcessInput() call.
  // Used for detecting reentrancy.
  bool is_decoder_processing_input_;

  // Ack listener of this stream, and it is notified when any of written bytes
  // are acked or retransmitted.
  quiche::QuicheReferenceCountedPointer<QuicAckListenerInterface> ack_listener_;

  // Offset of unacked frame headers.
  QuicIntervalSet<QuicStreamOffset> unacked_frame_headers_offsets_;

  // Urgency value sent in the last PRIORITY_UPDATE frame, or default urgency
  // defined by the spec if no PRIORITY_UPDATE frame has been sent.
  int last_sent_urgency_;

  // If this stream is a WebTransport extended CONNECT stream, contains the
  // WebTransport session associated with this stream.
  std::unique_ptr<WebTransportHttp3> web_transport_;

  // If this stream is a WebTransport data stream, |web_transport_data_|
  // contains all of the associated metadata.
  std::unique_ptr<WebTransportDataStream> web_transport_data_;

  // HTTP/3 Datagram support.
  Http3DatagramRegistrationVisitor* datagram_registration_visitor_ = nullptr;
  Http3DatagramVisitor* datagram_no_context_visitor_ = nullptr;
  absl::optional<QuicDatagramStreamId> datagram_flow_id_;
  QuicDatagramContextId datagram_next_available_context_id_;
  absl::flat_hash_map<QuicDatagramContextId, Http3DatagramVisitor*>
      datagram_context_visitors_;
  bool use_datagram_contexts_ = false;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_HTTP_QUIC_SPDY_STREAM_H_
