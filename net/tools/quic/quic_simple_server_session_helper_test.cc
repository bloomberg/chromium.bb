// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_crypto_server_stream_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(QuicSimpleCryptoServerStreamHelperTest, GenerateConnectionIdForReject) {
  quic::test::MockRandom random;
  quic::QuicSimpleCryptoServerStreamHelper helper(&random);

  EXPECT_EQ(quic::QuicUtils::CreateRandomConnectionId(&random),
            // N.B., version number and ID are ignored in the helper.
            helper.GenerateConnectionIdForReject(
                quic::QUIC_VERSION_46, quic::test::TestConnectionId(42)));
}

}  // namespace net
