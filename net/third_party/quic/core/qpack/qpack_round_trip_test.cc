// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "net/third_party/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/spdy/core/spdy_header_block.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Combine;
using ::testing::Values;

namespace quic {
namespace test {
namespace {

class QpackRoundTripTest
    : public QuicTestWithParam<std::tuple<FragmentMode, FragmentMode>> {
 public:
  QpackRoundTripTest()
      : encoding_fragment_mode_(std::get<0>(GetParam())),
        decoding_fragment_mode_(std::get<1>(GetParam())) {}

  spdy::SpdyHeaderBlock EncodeThenDecode(
      const spdy::SpdyHeaderBlock& header_list) {
    QuicString encoded_header_block = QpackTestUtils::Encode(
        QpackTestUtils::FragmentModeToFragmentSizeGenerator(
            encoding_fragment_mode_),
        &header_list);

    TestHeadersHandler handler;
    QpackTestUtils::Decode(&handler,
                           QpackTestUtils::FragmentModeToFragmentSizeGenerator(
                               decoding_fragment_mode_),
                           encoded_header_block);

    EXPECT_TRUE(handler.decoding_completed());
    EXPECT_FALSE(handler.decoding_error_detected());

    return handler.ReleaseHeaderList();
  }

 private:
  const FragmentMode encoding_fragment_mode_;
  const FragmentMode decoding_fragment_mode_;
};

INSTANTIATE_TEST_CASE_P(
    ,
    QpackRoundTripTest,
    Combine(Values(FragmentMode::kSingleChunk, FragmentMode::kOctetByOctet),
            Values(FragmentMode::kSingleChunk, FragmentMode::kOctetByOctet)));

TEST_P(QpackRoundTripTest, Empty) {
  spdy::SpdyHeaderBlock header_list;
  spdy::SpdyHeaderBlock output = EncodeThenDecode(header_list);
  EXPECT_EQ(header_list, output);
}

TEST_P(QpackRoundTripTest, EmptyName) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "bar";
  header_list[""] = "bar";

  spdy::SpdyHeaderBlock output = EncodeThenDecode(header_list);
  EXPECT_EQ(header_list, output);
}

TEST_P(QpackRoundTripTest, EmptyValue) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "";
  header_list[""] = "";

  spdy::SpdyHeaderBlock output = EncodeThenDecode(header_list);
  EXPECT_EQ(header_list, output);
}

TEST_P(QpackRoundTripTest, MultipleWithLongEntries) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "bar";
  header_list[":path"] = "/";
  header_list["foobaar"] = QuicString(127, 'Z');
  header_list[QuicString(1000, 'b')] = QuicString(1000, 'c');

  spdy::SpdyHeaderBlock output = EncodeThenDecode(header_list);
  EXPECT_EQ(header_list, output);
}

}  // namespace
}  // namespace test
}  // namespace quic
