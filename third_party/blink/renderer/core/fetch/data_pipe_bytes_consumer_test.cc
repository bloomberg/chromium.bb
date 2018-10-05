// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/data_pipe_bytes_consumer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
class DataPipeBytesConsumerTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(IntSize()); }
};

TEST_F(DataPipeBytesConsumerTest, TwoPhaseRead) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  const std::string kData = "Such hospitality. I'm underwhelmed.";
  uint32_t write_size = kData.size();

  MojoResult rv = pipe.producer_handle->WriteData(kData.c_str(), &write_size,
                                                  MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(kData.size(), write_size);

  // Close the producer so the consumer will reach the kDone state.
  pipe.producer_handle.reset();

  BytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(consumer))->Run();
  EXPECT_EQ(BytesConsumer::Result::kDone, result.first);
  EXPECT_EQ(
      kData,
      BytesConsumerTestUtil::CharVectorToString(result.second).Utf8().data());
}

}  // namespace blink
