// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QPACK_QPACK_ENCODER_TEST_UTILS_H_
#define QUICHE_QUIC_CORE_QPACK_QPACK_ENCODER_TEST_UTILS_H_

#include <string>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"

namespace quic {
namespace test {

// QpackEncoder::DecoderStreamErrorDelegate implementation that does nothing.
class NoopDecoderStreamErrorDelegate
    : public QpackEncoder::DecoderStreamErrorDelegate {
 public:
  ~NoopDecoderStreamErrorDelegate() override = default;

  void OnDecoderStreamError(QuicStringPiece error_message) override;
};

// Mock QpackEncoder::DecoderStreamErrorDelegate implementation.
class MockDecoderStreamErrorDelegate
    : public QpackEncoder::DecoderStreamErrorDelegate {
 public:
  ~MockDecoderStreamErrorDelegate() override = default;

  MOCK_METHOD1(OnDecoderStreamError, void(QuicStringPiece error_message));
};

}  // namespace test
}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QPACK_QPACK_ENCODER_TEST_UTILS_H_
