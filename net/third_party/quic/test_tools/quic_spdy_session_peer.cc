// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"

#include "net/third_party/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quic/core/quic_utils.h"

namespace quic {
namespace test {

// static
QuicHeadersStream* QuicSpdySessionPeer::GetHeadersStream(
    QuicSpdySession* session) {
  return session->headers_stream_.get();
}

// static
void QuicSpdySessionPeer::SetHeadersStream(QuicSpdySession* session,
                                           QuicHeadersStream* headers_stream) {
  session->headers_stream_.reset(headers_stream);
  if (headers_stream != nullptr) {
    session->RegisterStaticStream(headers_stream->id(), headers_stream);
  }
}

// static
const spdy::SpdyFramer& QuicSpdySessionPeer::GetSpdyFramer(
    QuicSpdySession* session) {
  return session->spdy_framer_;
}

void QuicSpdySessionPeer::SetHpackEncoderDebugVisitor(
    QuicSpdySession* session,
    std::unique_ptr<QuicHpackDebugVisitor> visitor) {
  session->SetHpackEncoderDebugVisitor(std::move(visitor));
}

void QuicSpdySessionPeer::SetHpackDecoderDebugVisitor(
    QuicSpdySession* session,
    std::unique_ptr<QuicHpackDebugVisitor> visitor) {
  session->SetHpackDecoderDebugVisitor(std::move(visitor));
}

void QuicSpdySessionPeer::SetMaxUncompressedHeaderBytes(
    QuicSpdySession* session,
    size_t set_max_uncompressed_header_bytes) {
  session->set_max_uncompressed_header_bytes(set_max_uncompressed_header_bytes);
}

// static
size_t QuicSpdySessionPeer::WriteHeadersImpl(
    QuicSpdySession* session,
    QuicStreamId id,
    spdy::SpdyHeaderBlock headers,
    bool fin,
    int weight,
    QuicStreamId parent_stream_id,
    bool exclusive,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  return session->WriteHeadersImpl(id, std::move(headers), fin, weight,
                                   parent_stream_id, exclusive,
                                   std::move(ack_listener));
}

//  static
QuicStreamId QuicSpdySessionPeer::StreamIdDelta(
    const QuicSpdySession& session) {
  return QuicUtils::StreamIdDelta(session.connection()->transport_version());
}

//  static
QuicStreamId QuicSpdySessionPeer::GetNthClientInitiatedBidirectionalStreamId(
    const QuicSpdySession& session,
    int n) {
  return QuicUtils::GetFirstBidirectionalStreamId(
             session.connection()->transport_version(),
             Perspective::IS_CLIENT) +
         // + 1 because spdy_session contains headers stream.
         QuicSpdySessionPeer::StreamIdDelta(session) * (n + 1);
}

//  static
QuicStreamId QuicSpdySessionPeer::GetNthServerInitiatedBidirectionalStreamId(
    const QuicSpdySession& session,
    int n) {
  return QuicUtils::GetFirstBidirectionalStreamId(
             session.connection()->transport_version(),
             Perspective::IS_SERVER) +
         QuicSpdySessionPeer::StreamIdDelta(session) * n;
}

//  static
QuicStreamId QuicSpdySessionPeer::GetNthServerInitiatedUnidirectionalStreamId(
    const QuicSpdySession& session,
    int n) {
  return QuicUtils::GetFirstUnidirectionalStreamId(
             session.connection()->transport_version(),
             Perspective::IS_SERVER) +
         QuicSpdySessionPeer::StreamIdDelta(session) * n;
}

}  // namespace test
}  // namespace quic
