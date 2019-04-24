// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The base class for streams which deliver data to/from an application.
// In each direction, the data on such a stream first contains compressed
// headers then body data.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_STREAM_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_STREAM_H_

#include <sys/types.h>

#include <cstddef>
#include <list>

#include "base/macros.h"
#include "net/third_party/quic/core/http/http_decoder.h"
#include "net/third_party/quic/core/http/http_encoder.h"
#include "net/third_party/quic/core/http/quic_header_list.h"
#include "net/third_party/quic/core/http/quic_spdy_stream_body_buffer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_stream.h"
#include "net/third_party/quic/core/quic_stream_sequencer.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"

namespace quic {

namespace test {
class QuicSpdyStreamPeer;
class QuicStreamPeer;
}  // namespace test

class QuicSpdySession;

// A QUIC stream that can send and receive HTTP2 (SPDY) headers.
class QUIC_EXPORT_PRIVATE QuicSpdyStream : public QuicStream {
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
    virtual void OnPromiseHeadersComplete(QuicStreamId promised_id,
                                          size_t frame_len) {}

   protected:
    virtual ~Visitor() {}
  };

  QuicSpdyStream(QuicStreamId id,
                 QuicSpdySession* spdy_session,
                 StreamType type);
  QuicSpdyStream(PendingStream pending,
                 QuicSpdySession* spdy_session,
                 StreamType type);
  QuicSpdyStream(const QuicSpdyStream&) = delete;
  QuicSpdyStream& operator=(const QuicSpdyStream&) = delete;
  ~QuicSpdyStream() override;

  // QuicStream implementation
  void OnClose() override;

  // Override to maybe close the write side after writing.
  void OnCanWrite() override;

  // Called by the session when headers with a priority have been received
  // for this stream.  This method will only be called for server streams.
  virtual void OnStreamHeadersPriority(spdy::SpdyPriority priority);

  // Called by the session when decompressed headers have been completely
  // delivered to this stream.  If |fin| is true, then this stream
  // should be closed; no more data will be sent by the peer.
  virtual void OnStreamHeaderList(bool fin,
                                  size_t frame_len,
                                  const QuicHeaderList& header_list);

  // Called when the received headers are too large. By default this will
  // reset the stream.
  virtual void OnHeadersTooLarge();

  // Called by the session when decompressed push promise headers have
  // been completely delivered to this stream.
  virtual void OnPromiseHeaderList(QuicStreamId promised_id,
                                   size_t frame_len,
                                   const QuicHeaderList& header_list);

  // Called by the session when a PRIORITY frame has been been received for this
  // stream. This method will only be called for server streams.
  void OnPriorityFrame(spdy::SpdyPriority priority);

  // Override the base class to not discard response when receiving
  // QUIC_STREAM_NO_ERROR.
  void OnStreamReset(const QuicRstStreamFrame& frame) override;

  // Called by the sequencer when new data is available. Decodes the data and
  // calls OnBodyAvailable() to pass to the upper layer.
  void OnDataAvailable() override;

  // Called in OnDataAvailable() after it finishes the decoding job.
  virtual void OnBodyAvailable() = 0;

  // Writes the headers contained in |header_block| to the dedicated
  // headers stream.
  virtual size_t WriteHeaders(
      spdy::SpdyHeaderBlock header_block,
      bool fin,
      QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener);

  // Sends |data| to the peer, or buffers if it can't be sent immediately.
  void WriteOrBufferBody(QuicStringPiece data, bool fin);

  // Writes the trailers contained in |trailer_block| to the dedicated
  // headers stream. Trailers will always have the FIN set.
  virtual size_t WriteTrailers(
      spdy::SpdyHeaderBlock trailer_block,
      QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener);

  // Override to report newly acked bytes via ack_listener_.
  bool OnStreamFrameAcked(QuicStreamOffset offset,
                          QuicByteCount data_length,
                          bool fin_acked,
                          QuicTime::Delta ack_delay_time,
                          QuicByteCount* newly_acked_length) override;

  // Override to report bytes retransmitted via ack_listener_.
  void OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                  QuicByteCount data_length,
                                  bool fin_retransmitted) override;

  // Does the same thing as WriteOrBufferBody except this method takes iovec
  // as the data input. Right now it only calls WritevData.
  // TODO(renjietang): Write data frame header before writing body.
  QuicConsumedData WritevBody(const struct iovec* iov, int count, bool fin);

  // Does the same thing as WriteOrBufferBody except this method takes
  // memslicespan as the data input. Right now it only calls WriteMemSlices.
  // TODO(renjietang): Write data frame header before writing body.
  QuicConsumedData WriteBodySlices(QuicMemSliceSpan slices, bool fin);

  // Marks the trailers as consumed. This applies to the case where this object
  // receives headers and trailers as QuicHeaderLists via calls to
  // OnStreamHeaderList().
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
  bool ParseHeaderStatusCode(const spdy::SpdyHeaderBlock& header,
                             int* status_code) const;

  // Returns true when all data has been read from the peer, including the fin.
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

  // Returns true if trailers have been fully read and consumed, or FIN has
  // been received and there are no trailers.
  bool FinishedReadingTrailers() const;

  // Called when owning session is getting deleted to avoid subsequent
  // use of the spdy_session_ member.
  void ClearSession();

  // Returns true if the sequencer has delivered the FIN, and no more body bytes
  // will be available.
  bool IsClosed() { return sequencer()->IsClosed(); }

  void OnDataFrameStart(Http3FrameLengths frame_lengths);
  void OnDataFramePayload(QuicStringPiece payload);
  void OnDataFrameEnd();

  using QuicStream::CloseWriteSide;

 protected:
  virtual void OnInitialHeadersComplete(bool fin,
                                        size_t frame_len,
                                        const QuicHeaderList& header_list);
  virtual void OnTrailingHeadersComplete(bool fin,
                                         size_t frame_len,
                                         const QuicHeaderList& header_list);
  virtual size_t WriteHeadersImpl(
      spdy::SpdyHeaderBlock header_block,
      bool fin,
      QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener);

  QuicSpdySession* spdy_session() const { return spdy_session_; }
  Visitor* visitor() { return visitor_; }

  void set_headers_decompressed(bool val) { headers_decompressed_ = val; }

  void set_ack_listener(
      QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
    ack_listener_ = std::move(ack_listener);
  }

  const QuicIntervalSet<QuicStreamOffset>& unacked_frame_headers_offsets() {
    return unacked_frame_headers_offsets_;
  }

 private:
  friend class test::QuicSpdyStreamPeer;
  friend class test::QuicStreamPeer;
  friend class QuicStreamUtils;
  class HttpDecoderVisitor;

  // Given the interval marked by [|offset|, |offset| + |data_length|), return
  // the number of frame header bytes contained in it.
  QuicByteCount GetNumFrameHeadersInInterval(QuicStreamOffset offset,
                                             QuicByteCount data_length) const;

  QuicSpdySession* spdy_session_;

  Visitor* visitor_;
  // True if the headers have been completely decompressed.
  bool headers_decompressed_;
  // Contains a copy of the decompressed header (name, value) pairs until they
  // are consumed via Readv.
  QuicHeaderList header_list_;

  // True if the trailers have been completely decompressed.
  bool trailers_decompressed_;
  // True if the trailers have been consumed.
  bool trailers_consumed_;
  // The parsed trailers received from the peer.
  spdy::SpdyHeaderBlock received_trailers_;

  // Http encoder for writing streams.
  HttpEncoder encoder_;
  // Http decoder for processing raw incoming stream frames.
  HttpDecoder decoder_;
  // Visitor of the HttpDecoder.
  std::unique_ptr<HttpDecoderVisitor> http_decoder_visitor_;
  // Buffer that contains decoded data of the stream.
  QuicSpdyStreamBodyBuffer body_buffer_;

  // Ack listener of this stream, and it is notified when any of written bytes
  // are acked or retransmitted.
  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener_;

  // Offset of unacked frame headers.
  QuicIntervalSet<QuicStreamOffset> unacked_frame_headers_offsets_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_STREAM_H_
