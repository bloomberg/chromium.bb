// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_write_buffer.h"

#include <algorithm>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace platform {
namespace {

class MockObserver : public TlsWriteBuffer::Observer {
 public:
  MOCK_METHOD1(NotifyWriteBufferFill, void(double));
};

}  // namespace

TEST(TlsWriteBufferTest, CheckBasicFunctionality) {
  MockObserver observer;
  TlsWriteBuffer buffer(&observer);
  constexpr size_t write_size = TlsWriteBuffer::kBufferSizeBytes / 2;
  uint8_t write_buffer[write_size];
  std::fill_n(write_buffer, write_size, uint8_t{1});

  EXPECT_CALL(observer, NotifyWriteBufferFill(0.5)).Times(1);
  EXPECT_EQ(buffer.Write(write_buffer, write_size), write_size);

  absl::Span<const uint8_t> readable_data = buffer.GetReadableRegion();
  EXPECT_EQ(readable_data.size(), write_size);
  for (size_t i = 0; i < readable_data.size(); i++) {
    EXPECT_EQ(readable_data[i], 1);
  }

  EXPECT_CALL(observer, NotifyWriteBufferFill(0.25)).Times(1);
  buffer.Consume(write_size / 2);

  readable_data = buffer.GetReadableRegion();
  EXPECT_EQ(readable_data.size(), write_size / 2);
  for (size_t i = 0; i < readable_data.size(); i++) {
    EXPECT_EQ(readable_data[i], 1);
  }

  EXPECT_CALL(observer, NotifyWriteBufferFill(0)).Times(1);
  buffer.Consume(write_size / 2);

  readable_data = buffer.GetReadableRegion();
  EXPECT_EQ(readable_data.size(), size_t{0});
}

TEST(TlsWriteBufferTest, TestWrapAround) {
  MockObserver observer;
  TlsWriteBuffer buffer(&observer);
  constexpr size_t write_size = TlsWriteBuffer::kBufferSizeBytes;
  uint8_t write_buffer[write_size];

  EXPECT_CALL(observer, NotifyWriteBufferFill(0.75)).Times(1);
  constexpr size_t partial_write_size = write_size * 3 / 4;
  EXPECT_EQ(buffer.Write(write_buffer, partial_write_size), partial_write_size);

  EXPECT_CALL(observer, NotifyWriteBufferFill(0.25)).Times(1);
  buffer.Consume(write_size / 2);

  EXPECT_CALL(observer, NotifyWriteBufferFill(0.75)).Times(1);
  EXPECT_EQ(buffer.Write(write_buffer, write_size / 2), write_size / 2);

  absl::Span<const uint8_t> readable_data = buffer.GetReadableRegion();

  EXPECT_CALL(observer, NotifyWriteBufferFill(0.25)).Times(1);
  buffer.Consume(write_size / 2);

  readable_data = buffer.GetReadableRegion();
  EXPECT_EQ(readable_data.size(), write_size / 4);

  EXPECT_CALL(observer, NotifyWriteBufferFill(0)).Times(1);
  buffer.Consume(write_size / 4);
}

}  // namespace platform
}  // namespace openscreen
