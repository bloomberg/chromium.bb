// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/crypto/quic_random.h"

#include "net/third_party/quic/platform/api/quic_test.h"

namespace net {
namespace test {

class QuicRandomTest : public QuicTest {};

TEST_F(QuicRandomTest, RandBytes) {
  unsigned char buf1[16];
  unsigned char buf2[16];
  memset(buf1, 0xaf, sizeof(buf1));
  memset(buf2, 0xaf, sizeof(buf2));
  ASSERT_EQ(0, memcmp(buf1, buf2, sizeof(buf1)));

  QuicRandom* rng = QuicRandom::GetInstance();
  rng->RandBytes(buf1, sizeof(buf1));
  EXPECT_NE(0, memcmp(buf1, buf2, sizeof(buf1)));
}

TEST_F(QuicRandomTest, RandUint64) {
  QuicRandom* rng = QuicRandom::GetInstance();
  uint64_t value1 = rng->RandUint64();
  uint64_t value2 = rng->RandUint64();
  EXPECT_NE(value1, value2);
}

TEST_F(QuicRandomTest, Reseed) {
  char buf[1024];
  memset(buf, 0xaf, sizeof(buf));

  QuicRandom* rng = QuicRandom::GetInstance();
  rng->Reseed(buf, sizeof(buf));
}

}  // namespace test
}  // namespace net
