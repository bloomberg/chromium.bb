// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"

#include "net/third_party/quic/core/quic_error_codes.h"

namespace blink {

P2PQuicStreamImpl::P2PQuicStreamImpl(quic::QuicStreamId id,
                                     quic::QuicSession* session,
                                     uint32_t write_buffer_size)
    : quic::QuicStream(id, session, /*is_static=*/false, quic::BIDIRECTIONAL),
      write_buffer_size_(write_buffer_size) {
  DCHECK_GT(write_buffer_size_, 0u);
}

P2PQuicStreamImpl::~P2PQuicStreamImpl() {}

void P2PQuicStreamImpl::OnDataAvailable() {
  // We just drop the data by marking all data as immediately consumed.
  sequencer()->MarkConsumed(sequencer()->ReadableBytes());
  if (sequencer()->IsClosed()) {
    // This means all data has been consumed up to the FIN bit.
    OnFinRead();
  }
}

void P2PQuicStreamImpl::OnStreamDataConsumed(size_t bytes_consumed) {
  DCHECK(delegate_);
  // We should never consume more than has been written.
  DCHECK_GE(write_buffered_amount_, bytes_consumed);
  QuicStream::OnStreamDataConsumed(bytes_consumed);
  if (bytes_consumed > 0) {
    write_buffered_amount_ -= bytes_consumed;
    delegate_->OnWriteDataConsumed(bytes_consumed);
  }
}

void P2PQuicStreamImpl::Reset() {
  if (rst_sent()) {
    // No need to reset twice. This could have already been sent as consequence
    // of receiving a RST_STREAM frame.
    return;
  }
  quic::QuicStream::Reset(quic::QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
}

void P2PQuicStreamImpl::WriteData(std::vector<uint8_t> data, bool fin) {
  // It is up to the delegate to not write more data than the
  // |write_buffer_size_|.
  DCHECK_GE(write_buffer_size_, data.size() + write_buffered_amount_);
  write_buffered_amount_ += data.size();
  QuicStream::WriteOrBufferData(
      quic::QuicStringPiece(reinterpret_cast<const char*>(data.data()),
                            data.size()),
      fin, nullptr);
}

void P2PQuicStreamImpl::SetDelegate(P2PQuicStream::Delegate* delegate) {
  delegate_ = delegate;
}

void P2PQuicStreamImpl::OnStreamReset(const quic::QuicRstStreamFrame& frame) {
  DCHECK(delegate_);
  // Calling this on the QuicStream will ensure that the stream is closed
  // for reading and writing and we send a RST frame to the remote side if
  // we have not already.
  quic::QuicStream::OnStreamReset(frame);
  delegate_->OnRemoteReset();
}

void P2PQuicStreamImpl::OnFinRead() {
  // TODO(https://crbug.com/874296): If we get an incoming stream we need to
  // make sure that the delegate is set before we have incoming data.
  DCHECK(delegate_);
  // Calling this on the QuicStream ensures that the stream is closed
  // for reading.
  quic::QuicStream::OnFinRead();
  delegate_->OnRemoteFinish();
}

void P2PQuicStreamImpl::OnClose() {
  closed_ = true;
  quic::QuicStream::OnClose();
}

bool P2PQuicStreamImpl::IsClosedForTesting() {
  return closed_;
}

}  // namespace blink
