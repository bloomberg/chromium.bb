// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder.h"

#include "base/logging.h"
#include "net/third_party/http2/hpack/huffman/hpack_huffman_encoder.h"
#include "net/third_party/http2/hpack/varint/hpack_varint_encoder.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_string_utils.h"

namespace quic {
namespace {

// An implementation of ProgressiveEncoder interface that encodes a single
// header block.
class QpackProgressiveEncoder : public spdy::HpackEncoder::ProgressiveEncoder {
 public:
  explicit QpackProgressiveEncoder(const spdy::SpdyHeaderBlock* header_list);
  QpackProgressiveEncoder(const QpackProgressiveEncoder&) = delete;
  QpackProgressiveEncoder& operator=(const QpackProgressiveEncoder&) = delete;
  ~QpackProgressiveEncoder() = default;

  // Returns true iff more remains to encode.
  bool HasNext() const override;

  // Encodes up to |max_encoded_bytes| octets, appending to |output|.
  void Next(size_t max_encoded_bytes, QuicString* output) override;

 private:
  enum class State {
    kNameStart,
    kNameLength,
    kNameString,
    kValueStart,
    kValueLength,
    kValueString
  };

  // One method for each state.  They each encode up to |max_encoded_bytes|
  // octets, appending to |output|.
  size_t DoNameStart(size_t max_encoded_bytes, QuicString* output);
  size_t DoNameLength(size_t max_encoded_bytes, QuicString* output);
  size_t DoNameString(size_t max_encoded_bytes, QuicString* output);
  size_t DoValueStart(size_t max_encoded_bytes, QuicString* output);
  size_t DoValueLength(size_t max_encoded_bytes, QuicString* output);
  size_t DoValueString(size_t max_encoded_bytes, QuicString* output);

  const spdy::SpdyHeaderBlock* const header_list_;
  spdy::SpdyHeaderBlock::const_iterator header_list_iterator_;
  State state_;

  // While encoding a string literal, |string_to_write_| points to the substring
  // that remains to be written.  This may be a substring of a string owned by
  // |header_list_|, or a substring of |huffman_encoded_string_|.
  QuicStringPiece string_to_write_;

  // Storage for the Huffman encoded string literal to be written if Huffman
  // encoding is used.
  QuicString huffman_encoded_string_;

  http2::HpackVarintEncoder varint_encoder_;
};

QpackProgressiveEncoder::QpackProgressiveEncoder(
    const spdy::SpdyHeaderBlock* header_list)
    : header_list_(header_list),
      header_list_iterator_(header_list_->begin()),
      state_(State::kNameStart) {}

bool QpackProgressiveEncoder::HasNext() const {
  return header_list_iterator_ != header_list_->end();
}

void QpackProgressiveEncoder::Next(size_t max_encoded_bytes,
                                   QuicString* output) {
  DCHECK(HasNext());
  DCHECK_NE(0u, max_encoded_bytes);

  while (max_encoded_bytes > 0 && HasNext()) {
    State old_state = state_;
    size_t encoded_bytes = 0;

    switch (state_) {
      case State::kNameStart:
        encoded_bytes = DoNameStart(max_encoded_bytes, output);
        break;
      case State::kNameLength:
        encoded_bytes = DoNameLength(max_encoded_bytes, output);
        break;
      case State::kNameString:
        encoded_bytes = DoNameString(max_encoded_bytes, output);
        break;
      case State::kValueStart:
        encoded_bytes = DoValueStart(max_encoded_bytes, output);
        break;
      case State::kValueLength:
        encoded_bytes = DoValueLength(max_encoded_bytes, output);
        break;
      case State::kValueString:
        encoded_bytes = DoValueString(max_encoded_bytes, output);
        break;
    }

    DCHECK_LE(encoded_bytes, max_encoded_bytes);

    if (state_ == old_state) {
      // Encoding stopped either because it has completed or the buffer is full.
      DCHECK(encoded_bytes == max_encoded_bytes || !HasNext());

      return;
    }

    max_encoded_bytes -= encoded_bytes;
  }
}

size_t QpackProgressiveEncoder::DoNameStart(size_t max_encoded_bytes,
                                            QuicString* output) {
  string_to_write_ = header_list_iterator_->first;
  http2::HuffmanEncode(string_to_write_, &huffman_encoded_string_);

  // Use Huffman encoding if it cuts down on size.
  if (huffman_encoded_string_.size() < string_to_write_.size()) {
    string_to_write_ = huffman_encoded_string_;
    output->push_back(varint_encoder_.StartEncoding(
        kLiteralHeaderFieldOpcode | kLiteralNameHuffmanMask,
        kLiteralHeaderFieldPrefixLength, string_to_write_.size()));
  } else {
    output->push_back(varint_encoder_.StartEncoding(
        kLiteralHeaderFieldOpcode, kLiteralHeaderFieldPrefixLength,
        string_to_write_.size()));
  }

  if (varint_encoder_.IsEncodingInProgress()) {
    state_ = State::kNameLength;
  } else if (string_to_write_.empty()) {
    state_ = State::kValueStart;
  } else {
    state_ = State::kNameString;
  }

  return 1;
}

size_t QpackProgressiveEncoder::DoNameLength(size_t max_encoded_bytes,
                                             QuicString* output) {
  DCHECK(varint_encoder_.IsEncodingInProgress());
  DCHECK(!string_to_write_.empty());

  const size_t encoded_bytes =
      varint_encoder_.ResumeEncoding(max_encoded_bytes, output);
  if (varint_encoder_.IsEncodingInProgress()) {
    DCHECK_EQ(encoded_bytes, max_encoded_bytes);
  } else {
    DCHECK_LE(encoded_bytes, max_encoded_bytes);
    state_ = State::kNameString;
  }

  return encoded_bytes;
}

size_t QpackProgressiveEncoder::DoNameString(size_t max_encoded_bytes,
                                             QuicString* output) {
  if (max_encoded_bytes < string_to_write_.size()) {
    const size_t encoded_bytes = max_encoded_bytes;
    QuicStrAppend(output, string_to_write_.substr(0, encoded_bytes));
    string_to_write_ = string_to_write_.substr(encoded_bytes);
    return encoded_bytes;
  }

  const size_t encoded_bytes = string_to_write_.size();
  QuicStrAppend(output, string_to_write_);
  state_ = State::kValueStart;
  return encoded_bytes;
}

size_t QpackProgressiveEncoder::DoValueStart(size_t max_encoded_bytes,
                                             QuicString* output) {
  string_to_write_ = header_list_iterator_->second;
  http2::HuffmanEncode(string_to_write_, &huffman_encoded_string_);

  // Use Huffman encoding if it cuts down on size.
  if (huffman_encoded_string_.size() < string_to_write_.size()) {
    string_to_write_ = huffman_encoded_string_;
    output->push_back(varint_encoder_.StartEncoding(kLiteralValueHuffmanMask,
                                                    kLiteralValuePrefixLength,
                                                    string_to_write_.size()));
  } else {
    output->push_back(varint_encoder_.StartEncoding(
        kLiteralValueWithoutHuffmanEncoding, kLiteralValuePrefixLength,
        string_to_write_.size()));
  }

  if (varint_encoder_.IsEncodingInProgress()) {
    state_ = State::kValueLength;
  } else if (string_to_write_.empty()) {
    ++header_list_iterator_;
    state_ = State::kNameStart;
  } else {
    state_ = State::kValueString;
  }

  return 1;
}

size_t QpackProgressiveEncoder::DoValueLength(size_t max_encoded_bytes,
                                              QuicString* output) {
  DCHECK(varint_encoder_.IsEncodingInProgress());
  DCHECK(!string_to_write_.empty());

  const size_t encoded_bytes =
      varint_encoder_.ResumeEncoding(max_encoded_bytes, output);
  if (varint_encoder_.IsEncodingInProgress()) {
    DCHECK_EQ(encoded_bytes, max_encoded_bytes);
  } else {
    DCHECK_LE(encoded_bytes, max_encoded_bytes);
    state_ = State::kValueString;
  }

  return encoded_bytes;
}

size_t QpackProgressiveEncoder::DoValueString(size_t max_encoded_bytes,
                                              QuicString* output) {
  if (max_encoded_bytes < string_to_write_.size()) {
    const size_t encoded_bytes = max_encoded_bytes;
    QuicStrAppend(output, string_to_write_.substr(0, encoded_bytes));
    string_to_write_ = string_to_write_.substr(encoded_bytes);
    return encoded_bytes;
  }

  const size_t encoded_bytes = string_to_write_.size();
  QuicStrAppend(output, string_to_write_);
  state_ = State::kNameStart;
  ++header_list_iterator_;
  return encoded_bytes;
}

}  // namespace

std::unique_ptr<spdy::HpackEncoder::ProgressiveEncoder>
QpackEncoder::EncodeHeaderSet(const spdy::SpdyHeaderBlock* header_list) {
  return std::make_unique<QpackProgressiveEncoder>(header_list);
}

}  // namespace quic
