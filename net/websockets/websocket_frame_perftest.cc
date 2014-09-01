// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include <algorithm>
#include <vector>

#include "base/macros.h"
#include "base/test/perf_time_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kIterations = 100000;
const int kLongPayloadSize = 1 << 16;
const char kMaskingKey[] = "\xFE\xED\xBE\xEF";

COMPILE_ASSERT(arraysize(kMaskingKey) ==
                   WebSocketFrameHeader::kMaskingKeyLength + 1,
               incorrect_masking_key_size);

class WebSocketFrameTestMaskBenchmark : public ::testing::Test {
 protected:
  void Benchmark(const char* const name,
                 const char* const payload,
                 size_t size) {
    std::vector<char> scratch(payload, payload + size);
    WebSocketMaskingKey masking_key;
    std::copy(kMaskingKey,
              kMaskingKey + WebSocketFrameHeader::kMaskingKeyLength,
              masking_key.key);
    base::PerfTimeLogger timer(name);
    for (int x = 0; x < kIterations; ++x) {
      MaskWebSocketFramePayload(
          masking_key, x % size, &scratch.front(), scratch.size());
    }
    timer.Done();
  }
};

TEST_F(WebSocketFrameTestMaskBenchmark, BenchmarkMaskShortPayload) {
  static const char kShortPayload[] = "Short Payload";
  Benchmark(
      "Frame_mask_short_payload", kShortPayload, arraysize(kShortPayload));
}

TEST_F(WebSocketFrameTestMaskBenchmark, BenchmarkMaskLongPayload) {
  std::vector<char> payload(kLongPayloadSize, 'a');
  Benchmark("Frame_mask_long_payload", &payload.front(), payload.size());
}

}  // namespace

}  // namespace net
