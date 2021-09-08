// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/gzipper.h"

#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace data_decoder {

namespace {

void CopyResultCallback(absl::optional<mojo_base::BigBuffer>& output_result,
                        absl::optional<mojo_base::BigBuffer> result) {
  output_result = std::move(result);
}

using GzipperTest = testing::Test;

}  // namespace

TEST_F(GzipperTest, CompressAndUncompress) {
  Gzipper gzipper;
  std::vector<uint8_t> input = {0x01, 0x01, 0x01, 0x02, 0x02, 0x02};
  absl::optional<mojo_base::BigBuffer> compressed;
  gzipper.Compress(input,
                   base::BindOnce(&CopyResultCallback, std::ref(compressed)));
  ASSERT_TRUE(compressed.has_value());
  EXPECT_THAT(base::make_span(*compressed),
              testing::Not(testing::ElementsAreArray(base::make_span(input))));

  absl::optional<mojo_base::BigBuffer> uncompressed;
  gzipper.Uncompress(
      std::move(*compressed),
      base::BindOnce(&CopyResultCallback, std::ref(uncompressed)));
  ASSERT_TRUE(uncompressed.has_value());
  EXPECT_THAT(base::make_span(*uncompressed),
              testing::ElementsAreArray(base::make_span(input)));
}

}  // namespace data_decoder
