// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder.h"

#include "base/logging.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quic/core/qpack/qpack_instruction_encoder.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_string_utils.h"

namespace quic {
namespace {

// An implementation of ProgressiveEncoder interface that encodes a single
// header block.
class QpackProgressiveEncoder : public spdy::HpackEncoder::ProgressiveEncoder {
 public:
  QpackProgressiveEncoder() = delete;
  QpackProgressiveEncoder(QpackHeaderTable* header_table,
                          const spdy::SpdyHeaderBlock* header_list);
  QpackProgressiveEncoder(const QpackProgressiveEncoder&) = delete;
  QpackProgressiveEncoder& operator=(const QpackProgressiveEncoder&) = delete;
  ~QpackProgressiveEncoder() = default;

  // Returns true iff more remains to encode.
  bool HasNext() const override;

  // Encodes up to |max_encoded_bytes| octets, appending to |output|.
  void Next(size_t max_encoded_bytes, QuicString* output) override;

 private:
  QpackInstructionEncoder instruction_encoder_;
  const QpackHeaderTable* const header_table_;
  const spdy::SpdyHeaderBlock* const header_list_;
  // Header field currently being encoded.
  spdy::SpdyHeaderBlock::const_iterator header_list_iterator_;
};

QpackProgressiveEncoder::QpackProgressiveEncoder(
    QpackHeaderTable* header_table,
    const spdy::SpdyHeaderBlock* header_list)
    : header_table_(header_table),
      header_list_(header_list),
      header_list_iterator_(header_list_->begin()) {}

bool QpackProgressiveEncoder::HasNext() const {
  return header_list_iterator_ != header_list_->end();
}

void QpackProgressiveEncoder::Next(size_t max_encoded_bytes,
                                   QuicString* output) {
  DCHECK_NE(0u, max_encoded_bytes);
  DCHECK(HasNext());

  // Since QpackInstructionEncoder::HasNext() does not indicate the number of
  // bytes written, save the maximum new size of |*output|.
  const size_t max_length = output->size() + max_encoded_bytes;

  DCHECK_LT(output->size(), max_length);

  do {
    // Call QpackInstructionEncoder::Encode for |*header_list_iterator_| if it
    // has not been called yet.
    if (!instruction_encoder_.HasNext()) {
      // Even after |name| and |value| go out of scope, copies of these
      // QuicStringPieces retained by QpackInstructionEncoder are still valid as
      // long as |header_list_| is valid.
      QuicStringPiece name = header_list_iterator_->first;
      QuicStringPiece value = header_list_iterator_->second;

      // |index| is saved by QpackInstructionEncoder by value, there are no
      // lifetime concerns.
      size_t index;

      auto match_type = header_table_->FindHeaderField(name, value, &index);

      switch (match_type) {
        case QpackHeaderTable::MatchType::kNameAndValue:
          instruction_encoder_.set_is_static(true);
          instruction_encoder_.set_varint(index);

          instruction_encoder_.Encode(QpackIndexedHeaderFieldInstruction());

          break;
        case QpackHeaderTable::MatchType::kName:
          instruction_encoder_.set_is_static(true);
          instruction_encoder_.set_varint(index);
          instruction_encoder_.set_value(value);

          instruction_encoder_.Encode(
              QpackLiteralHeaderFieldNameReferenceInstruction());

          break;
        case QpackHeaderTable::MatchType::kNoMatch:
          instruction_encoder_.set_name(name);
          instruction_encoder_.set_value(value);

          instruction_encoder_.Encode(QpackLiteralHeaderFieldInstruction());

          break;
      }
    }

    DCHECK(instruction_encoder_.HasNext());

    instruction_encoder_.Next(max_length - output->size(), output);

    if (instruction_encoder_.HasNext()) {
      // There was not enough room to completely encode current header field.
      DCHECK_EQ(output->size(), max_length);

      return;
    }

    // It is possible that the output buffer was just large enough for encoding
    // the current header field, hence equality is allowed here.
    DCHECK_LE(output->size(), max_length);

    // Move on to the next header field.
    ++header_list_iterator_;
  } while (HasNext() && output->size() < max_length);
}

}  // namespace

std::unique_ptr<spdy::HpackEncoder::ProgressiveEncoder>
QpackEncoder::EncodeHeaderList(const spdy::SpdyHeaderBlock* header_list) {
  return std::make_unique<QpackProgressiveEncoder>(&header_table_, header_list);
}

}  // namespace quic
