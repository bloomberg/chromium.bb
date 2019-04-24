// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_decoder_test_utils.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"

namespace quic {
namespace test {

void NoopEncoderStreamErrorDelegate::OnEncoderStreamError(
    QuicStringPiece error_message) {}

void NoopDecoderStreamSenderDelegate::WriteDecoderStreamData(
    QuicStringPiece data) {}

TestHeadersHandler::TestHeadersHandler()
    : decoding_completed_(false), decoding_error_detected_(false) {}

void TestHeadersHandler::OnHeaderDecoded(QuicStringPiece name,
                                         QuicStringPiece value) {
  ASSERT_FALSE(decoding_completed_);
  ASSERT_FALSE(decoding_error_detected_);

  header_list_.AppendValueOrAddHeader(name, value);
}

void TestHeadersHandler::OnDecodingCompleted() {
  ASSERT_FALSE(decoding_completed_);
  ASSERT_FALSE(decoding_error_detected_);

  decoding_completed_ = true;
}

void TestHeadersHandler::OnDecodingErrorDetected(
    QuicStringPiece error_message) {
  ASSERT_FALSE(decoding_completed_);
  ASSERT_FALSE(decoding_error_detected_);

  decoding_error_detected_ = true;
}

spdy::SpdyHeaderBlock TestHeadersHandler::ReleaseHeaderList() {
  DCHECK(decoding_completed_);
  DCHECK(!decoding_error_detected_);

  return std::move(header_list_);
}

bool TestHeadersHandler::decoding_completed() const {
  return decoding_completed_;
}

bool TestHeadersHandler::decoding_error_detected() const {
  return decoding_error_detected_;
}

void QpackDecode(
    QpackDecoder::EncoderStreamErrorDelegate* encoder_stream_error_delegate,
    QpackDecoderStreamSender::Delegate* decoder_stream_sender_delegate,
    QpackProgressiveDecoder::HeadersHandlerInterface* handler,
    const FragmentSizeGenerator& fragment_size_generator,
    QuicStringPiece data) {
  QpackDecoder decoder(encoder_stream_error_delegate,
                       decoder_stream_sender_delegate);
  auto progressive_decoder =
      decoder.DecodeHeaderBlock(/* stream_id = */ 1, handler);
  while (!data.empty()) {
    size_t fragment_size = std::min(fragment_size_generator(), data.size());
    progressive_decoder->Decode(data.substr(0, fragment_size));
    data = data.substr(fragment_size);
  }
  progressive_decoder->EndHeaderBlock();
}

}  // namespace test
}  // namespace quic
