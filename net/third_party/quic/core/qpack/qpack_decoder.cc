// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_decoder.h"

#include "base/logging.h"
#include "net/third_party/http2/decoder/decode_buffer.h"
#include "net/third_party/http2/decoder/decode_status.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"

namespace quic {

QpackDecoder::ProgressiveDecoder::ProgressiveDecoder(
    QpackDecoder::HeadersHandlerInterface* handler)
    : handler_(handler),
      state_(State::kParseOpcode),
      decoding_(true),
      error_detected_(false),
      name_length_(0),
      value_length_(0),
      is_huffman_(false) {}

void QpackDecoder::ProgressiveDecoder::Decode(QuicStringPiece data) {
  DCHECK(decoding_);

  if (data.empty() || error_detected_) {
    return;
  }

  size_t bytes_consumed = 0;
  while (true) {
    switch (state_) {
      case State::kParseOpcode:
        bytes_consumed = DoParseOpcode(data);
        break;
      case State::kNameLengthStart:
        bytes_consumed = DoNameLengthStart(data);
        break;
      case State::kNameLengthResume:
        bytes_consumed = DoNameLengthResume(data);
        break;
      case State::kNameLengthDone:
        DoNameLengthDone();
        bytes_consumed = 0;
        break;
      case State::kNameString:
        bytes_consumed = DoNameString(data);
        break;
      case State::kValueLengthStart:
        bytes_consumed = DoValueLengthStart(data);
        break;
      case State::kValueLengthResume:
        bytes_consumed = DoValueLengthResume(data);
        break;
      case State::kValueLengthDone:
        DoValueLengthDone();
        bytes_consumed = 0;
        break;
      case State::kValueString:
        bytes_consumed = DoValueString(data);
        break;
      case State::kDone:
        DoDone();
        bytes_consumed = 0;
        break;
    }

    if (error_detected_) {
      return;
    }

    DCHECK_LE(bytes_consumed, data.size());

    data = QuicStringPiece(data.data() + bytes_consumed,
                           data.size() - bytes_consumed);

    // Stop processing if no more data but next state would require it.
    if (data.empty() && (state_ != State::kNameLengthDone) &&
        (state_ != State::kValueLengthDone) && (state_ != State::kDone)) {
      return;
    }
  }
}

void QpackDecoder::ProgressiveDecoder::EndHeaderBlock() {
  DCHECK(decoding_);
  decoding_ = false;

  if (error_detected_) {
    return;
  }

  if (state_ == State::kParseOpcode) {
    handler_->OnDecodingCompleted();
  } else {
    OnError("Incomplete header block.");
  }
}

// This method always returns 0 since some bits of the byte containing the
// opcode must be parsed by other methods.
size_t QpackDecoder::ProgressiveDecoder::DoParseOpcode(QuicStringPiece data) {
  DCHECK(!data.empty());

  if ((data[0] & kIndexedHeaderFieldOpcodeMask) == kIndexedHeaderFieldOpcode) {
    // TODO(bnc): Implement.
    OnError("Indexed Header Field not implemented.");
    return 0;
  }

  if ((data[0] & kIndexedHeaderFieldPostBaseOpcodeMask) ==
      kIndexedHeaderFieldPostBaseOpcode) {
    // TODO(bnc): Implement.
    OnError("Indexed Header Field With Post-Base Index not implemented.");
    return 0;
  }

  if ((data[0] & kLiteralHeaderFieldNameReferenceOpcodeMask) ==
      kLiteralHeaderFieldNameReferenceOpcode) {
    // TODO(bnc): Implement.
    OnError("Literal Header Field With Name Reference not implemented.");
    return 0;
  }

  if ((data[0] & kLiteralHeaderFieldPostBaseOpcodeMask) ==
      kLiteralHeaderFieldPostBaseOpcode) {
    // TODO(bnc): Implement.
    OnError(
        "Literal Header Field With Post-Base Name Reference not implemented.");
    return 0;
  }

  DCHECK_EQ(kLiteralHeaderFieldOpcode, data[0] & kLiteralHeaderFieldOpcodeMask);
  state_ = State::kNameLengthStart;
  return 0;
}

size_t QpackDecoder::ProgressiveDecoder::DoNameLengthStart(
    QuicStringPiece data) {
  DCHECK(!data.empty());

  is_huffman_ = (data[0] & kLiteralNameHuffmanMask) == kLiteralNameHuffmanMask;

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralHeaderFieldPrefixLength, &buffer);

  size_t bytes_consumed = 1 + buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kNameLengthDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      state_ = State::kNameLengthResume;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("NameLen too large in literal header field without reference.");
      return bytes_consumed;
  }
}

size_t QpackDecoder::ProgressiveDecoder::DoNameLengthResume(
    QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data);
  http2::DecodeStatus status = varint_decoder_.Resume(&buffer);

  size_t bytes_consumed = buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kNameLengthDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK_EQ(bytes_consumed, data.size());
      DCHECK(buffer.Empty());
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("NameLen too large in literal header field without reference.");
      return bytes_consumed;
  }
}

void QpackDecoder::ProgressiveDecoder::DoNameLengthDone() {
  name_.clear();
  name_length_ = varint_decoder_.value();

  if (name_length_ == 0) {
    state_ = State::kValueLengthStart;
    return;
  }

  name_.reserve(name_length_);
  state_ = State::kNameString;
}

size_t QpackDecoder::ProgressiveDecoder::DoNameString(QuicStringPiece data) {
  DCHECK(!data.empty());
  DCHECK_LT(name_.size(), name_length_);

  size_t bytes_consumed = std::min(name_length_ - name_.size(), data.size());
  name_.append(data.data(), bytes_consumed);

  if (name_.size() < name_length_) {
    return bytes_consumed;
  }

  DCHECK_EQ(name_.size(), name_length_);

  if (is_huffman_) {
    huffman_decoder_.Reset();
    // HpackHuffmanDecoder::Decode() cannot perform in-place decoding.
    QuicString decoded_name;
    huffman_decoder_.Decode(name_, &decoded_name);
    if (!huffman_decoder_.InputProperlyTerminated()) {
      OnError("Error in Huffman-encoded name.");
      return bytes_consumed;
    }
    name_ = decoded_name;
  }

  state_ = State::kValueLengthStart;
  return bytes_consumed;
}

size_t QpackDecoder::ProgressiveDecoder::DoValueLengthStart(
    QuicStringPiece data) {
  DCHECK(!data.empty());

  is_huffman_ =
      (data[0] & kLiteralValueHuffmanMask) == kLiteralValueHuffmanMask;

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralValuePrefixLength, &buffer);

  size_t bytes_consumed = 1 + buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kValueLengthDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      state_ = State::kValueLengthResume;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("ValueLen too large in literal header field without reference.");
      return bytes_consumed;
  }
}

size_t QpackDecoder::ProgressiveDecoder::DoValueLengthResume(
    QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data);
  http2::DecodeStatus status = varint_decoder_.Resume(&buffer);

  size_t bytes_consumed = buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kValueLengthDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK_EQ(bytes_consumed, data.size());
      DCHECK(buffer.Empty());
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("ValueLen too large in literal header field without reference.");
      return bytes_consumed;
  }
}

void QpackDecoder::ProgressiveDecoder::DoValueLengthDone() {
  value_.clear();
  value_length_ = varint_decoder_.value();

  if (value_length_ == 0) {
    state_ = State::kDone;
    return;
  }

  value_.reserve(value_length_);
  state_ = State::kValueString;
}

size_t QpackDecoder::ProgressiveDecoder::DoValueString(QuicStringPiece data) {
  DCHECK(!data.empty());
  DCHECK_LT(value_.size(), value_length_);

  size_t bytes_consumed = std::min(value_length_ - value_.size(), data.size());
  value_.append(data.data(), bytes_consumed);

  if (value_.size() < value_length_) {
    return bytes_consumed;
  }

  DCHECK_EQ(value_.size(), value_length_);

  if (is_huffman_) {
    huffman_decoder_.Reset();
    // HpackHuffmanDecoder::Decode() cannot perform in-place decoding.
    QuicString decoded_value;
    huffman_decoder_.Decode(value_, &decoded_value);
    if (!huffman_decoder_.InputProperlyTerminated()) {
      OnError("Error in Huffman-encoded value.");
      return bytes_consumed;
    }
    value_ = decoded_value;
  }

  state_ = State::kDone;
  return bytes_consumed;
}

void QpackDecoder::ProgressiveDecoder::DoDone() {
  handler_->OnHeaderDecoded(name_, value_);
  state_ = State::kParseOpcode;
}

void QpackDecoder::ProgressiveDecoder::OnError(QuicStringPiece error_message) {
  DCHECK(!error_detected_);

  error_detected_ = true;
  handler_->OnDecodingErrorDetected(error_message);
}

std::unique_ptr<QpackDecoder::ProgressiveDecoder>
QpackDecoder::DecodeHeaderBlock(
    QpackDecoder::HeadersHandlerInterface* handler) {
  return std::make_unique<ProgressiveDecoder>(handler);
}

}  // namespace quic
