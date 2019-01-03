// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_decoder.h"

#include "base/logging.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/http2/decoder/decode_buffer.h"
#include "net/third_party/quiche/src/http2/decoder/decode_status.h"

namespace quic {

QpackDecoder::ProgressiveDecoder::ProgressiveDecoder(
    QuicStreamId stream_id,
    QpackHeaderTable* header_table,
    QpackDecoder::HeadersHandlerInterface* handler)
    : stream_id_(stream_id),
      prefix_decoder_(
          QuicMakeUnique<QpackInstructionDecoder>(QpackPrefixLanguage(), this)),
      instruction_decoder_(QpackRequestStreamLanguage(), this),
      header_table_(header_table),
      handler_(handler),
      largest_reference_(0),
      base_index_(0),
      prefix_decoded_(false),
      decoding_(true),
      error_detected_(false) {}

void QpackDecoder::ProgressiveDecoder::Decode(QuicStringPiece data) {
  DCHECK(decoding_);

  if (data.empty() || error_detected_) {
    return;
  }

  // Decode prefix byte by byte until the first (and only) instruction is
  // decoded.
  while (!prefix_decoded_) {
    prefix_decoder_->Decode(data.substr(0, 1));
    data = data.substr(1, QuicStringPiece::npos);
    if (data.empty()) {
      return;
    }
  }

  instruction_decoder_.Decode(data);
}

void QpackDecoder::ProgressiveDecoder::EndHeaderBlock() {
  DCHECK(decoding_);
  decoding_ = false;

  if (error_detected_) {
    return;
  }

  if (!instruction_decoder_.AtInstructionBoundary()) {
    OnError("Incomplete header block.");
    return;
  }

  if (!prefix_decoded_) {
    OnError("Incomplete header data prefix.");
    return;
  }

  handler_->OnDecodingCompleted();
}

bool QpackDecoder::ProgressiveDecoder::OnInstructionDecoded(
    const QpackInstruction* instruction) {
  if (instruction == QpackPrefixInstruction()) {
    DCHECK(!prefix_decoded_);

    largest_reference_ = instruction_decoder_.varint();
    base_index_ = instruction_decoder_.varint2();
    prefix_decoded_ = true;

    return true;
  }

  if (instruction == QpackIndexedHeaderFieldInstruction()) {
    if (!instruction_decoder_.s_bit()) {
      // TODO(bnc): Implement.
      OnError("Indexed Header Field with dynamic entry not implemented.");
      return false;
    }

    auto entry = header_table_->LookupEntry(/* is_static = */ true,
                                            instruction_decoder_.varint());
    if (!entry) {
      OnError("Invalid static table index.");
      return false;
    }

    handler_->OnHeaderDecoded(entry->name(), entry->value());
    return true;
  }

  if (instruction == QpackIndexedHeaderFieldPostBaseInstruction()) {
    // TODO(bnc): Implement.
    OnError("Indexed Header Field With Post-Base Index not implemented.");
    return false;
  }

  if (instruction == QpackLiteralHeaderFieldNameReferenceInstruction()) {
    if (!instruction_decoder_.s_bit()) {
      // TODO(bnc): Implement.
      OnError(
          "Literal Header Field With Name Reference with dynamic entry not "
          "implemented.");
      return false;
    }

    auto entry = header_table_->LookupEntry(/* is_static = */ true,
                                            instruction_decoder_.varint());
    if (!entry) {
      OnError(
          "Invalid static table index in Literal Header Field With Name "
          "Reference instruction.");
      return false;
    }

    handler_->OnHeaderDecoded(entry->name(), instruction_decoder_.value());
    return true;
  }

  if (instruction == QpackLiteralHeaderFieldPostBaseInstruction()) {
    // TODO(bnc): Implement.
    OnError(
        "Literal Header Field With Post-Base Name Reference not "
        "implemented.");
    return false;
  }

  DCHECK_EQ(instruction, QpackLiteralHeaderFieldInstruction());

  handler_->OnHeaderDecoded(instruction_decoder_.name(),
                            instruction_decoder_.value());

  return true;
}

void QpackDecoder::ProgressiveDecoder::OnError(QuicStringPiece error_message) {
  DCHECK(!error_detected_);

  error_detected_ = true;
  handler_->OnDecodingErrorDetected(error_message);
}

std::unique_ptr<QpackDecoder::ProgressiveDecoder>
QpackDecoder::DecodeHeaderBlock(
    QuicStreamId stream_id,
    QpackDecoder::HeadersHandlerInterface* handler) {
  return QuicMakeUnique<ProgressiveDecoder>(stream_id, &header_table_, handler);
}

}  // namespace quic
