// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_HTTP_TOOLS_QUIC_HTTP_RANDOM_UTIL_H_
#define NET_THIRD_PARTY_QUIC_HTTP_TOOLS_QUIC_HTTP_RANDOM_UTIL_H_

#include <stddef.h>

#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test_random.h"

namespace net {
namespace test {

// Returns a random integer in the range [lo, hi).
size_t GenerateUniformInRange(size_t lo, size_t hi, QuicTestRandomBase* rng);

// Generate a string with the web-safe string character set of specified len.
QuicString GenerateWebSafeString(size_t len, QuicTestRandomBase* rng);

// Generate a string with the web-safe string character set of length
// [lo, hi).
QuicString GenerateWebSafeString(size_t lo, size_t hi, QuicTestRandomBase* rng);

// Returns a random integer in the range [0, max], with a bias towards producing
// lower numbers.
size_t GenerateRandomSizeSkewedLow(size_t max, QuicTestRandomBase* rng);

}  // namespace test
}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_HTTP_TOOLS_QUIC_HTTP_RANDOM_UTIL_H_
