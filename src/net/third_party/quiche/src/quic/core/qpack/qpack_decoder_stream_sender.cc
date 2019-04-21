// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/qpack_decoder_stream_sender.h"

#include <cstddef>
#include <limits>
#include <string>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"

namespace quic {

QpackDecoderStreamSender::QpackDecoderStreamSender(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

void QpackDecoderStreamSender::SendInsertCountIncrement(uint64_t increment) {
  instruction_encoder_.set_varint(increment);

  instruction_encoder_.Encode(InsertCountIncrementInstruction());

  std::string output;

  instruction_encoder_.Next(std::numeric_limits<size_t>::max(), &output);
  DCHECK(!instruction_encoder_.HasNext());

  delegate_->WriteDecoderStreamData(output);
}

void QpackDecoderStreamSender::SendHeaderAcknowledgement(
    QuicStreamId stream_id) {
  instruction_encoder_.set_varint(stream_id);

  instruction_encoder_.Encode(HeaderAcknowledgementInstruction());

  std::string output;

  instruction_encoder_.Next(std::numeric_limits<size_t>::max(), &output);
  DCHECK(!instruction_encoder_.HasNext());

  delegate_->WriteDecoderStreamData(output);
}

void QpackDecoderStreamSender::SendStreamCancellation(QuicStreamId stream_id) {
  instruction_encoder_.set_varint(stream_id);

  instruction_encoder_.Encode(StreamCancellationInstruction());

  std::string output;

  instruction_encoder_.Next(std::numeric_limits<size_t>::max(), &output);
  DCHECK(!instruction_encoder_.HasNext());

  delegate_->WriteDecoderStreamData(output);
}

}  // namespace quic
