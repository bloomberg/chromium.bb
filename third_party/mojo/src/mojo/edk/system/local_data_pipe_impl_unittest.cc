// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/local_data_pipe_impl.h"

#include <string.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/system/data_pipe.h"
#include "mojo/edk/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions));

// Validate options.
TEST(LocalDataPipeImplTest, Creation) {
  // Create using default options.
  {
    // Get default options.
    MojoCreateDataPipeOptions default_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                  NullUserPointer(), &default_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(default_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }

  // Create using non-default options.
  {
    const MojoCreateDataPipeOptions options = {
        kSizeOfOptions,                           // |struct_size|.
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
        1,                                        // |element_num_bytes|.
        1000                                      // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(MakeUserPointer(&options),
                                              &validated_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  {
    const MojoCreateDataPipeOptions options = {
        kSizeOfOptions,                           // |struct_size|.
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
        4,                                        // |element_num_bytes|.
        4000                                      // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(MakeUserPointer(&options),
                                              &validated_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  // Default capacity.
  {
    const MojoCreateDataPipeOptions options = {
        kSizeOfOptions,                           // |struct_size|.
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
        100,                                      // |element_num_bytes|.
        0                                         // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(MakeUserPointer(&options),
                                              &validated_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
}

// Tests that |ProducerWriteData()| and |ConsumerReadData()| writes and reads,
// respectively, as much as possible, even if it has to "wrap around" the
// internal circular buffer. (Note that the two-phase write and read do not do
// this.)
TEST(LocalDataPipeImplTest, WrapAround) {
  unsigned char test_data[1000];
  for (size_t i = 0; i < arraysize(test_data); i++)
    test_data[i] = static_cast<unsigned char>(i);

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1u,                                       // |element_num_bytes|.
      100u                                      // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));
  // This test won't be valid if |ValidateCreateOptions()| decides to give the
  // pipe more space.
  ASSERT_EQ(100u, validated_options.capacity_num_bytes);

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

  // Write 20 bytes.
  uint32_t num_bytes = 20u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(&test_data[0]),
                                  MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(20u, num_bytes);

  // Read 10 bytes.
  unsigned char read_buffer[1000] = {0};
  num_bytes = 10u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(read_buffer),
                                 MakeUserPointer(&num_bytes), false, false));
  EXPECT_EQ(10u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[0], 10u));

  // Check that a two-phase write can now only write (at most) 80 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  void* write_buffer_ptr = nullptr;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(write_buffer_ptr);
  EXPECT_EQ(80u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(0u));

  // Write as much data as we can (using |ProducerWriteData()|). We should write
  // 90 bytes.
  num_bytes = 200u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(&test_data[20]),
                                  MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(90u, num_bytes);

  // Check that a two-phase read can now only read (at most) 90 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  const void* read_buffer_ptr = nullptr;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(read_buffer_ptr);
  EXPECT_EQ(90u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(0u));

  // Read as much as possible (using |ConsumerReadData()|). We should read 100
  // bytes.
  num_bytes =
      static_cast<uint32_t>(arraysize(read_buffer) * sizeof(read_buffer[0]));
  memset(read_buffer, 0, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(read_buffer),
                                 MakeUserPointer(&num_bytes), false, false));
  EXPECT_EQ(100u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[10], 100u));

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Tests the behavior of closing the producer or consumer with respect to
// writes and reads (simple and two-phase).
TEST(LocalDataPipeImplTest, CloseWriteRead) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1u,                                       // |element_num_bytes|.
      1000u                                     // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  // Close producer first, then consumer.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Write it again, so we'll have something left over.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(write_buffer_ptr);
    EXPECT_GT(num_bytes, 0u);

    // Start two-phase read.
    const void* read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer_ptr);
    EXPECT_EQ(2u * kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // The consumer can finish its two-phase read.
    EXPECT_EQ(0, memcmp(read_buffer_ptr, kTestData, kTestDataSize));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(kTestDataSize));

    // And start another.
    read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer_ptr);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer, which cancels the two-phase read.
    dp->ConsumerClose();
  }

  // Close consumer first, then producer.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(write_buffer_ptr);
    ASSERT_GT(num_bytes, kTestDataSize);

    // Start two-phase read.
    const void* read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer_ptr);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer.
    dp->ConsumerClose();

    // Actually write some data. (Note: Premature freeing of the buffer would
    // probably only be detected under ASAN or similar.)
    memcpy(write_buffer_ptr, kTestData, kTestDataSize);
    // Note: Even though the consumer has been closed, ending the two-phase
    // write will report success.
    EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(kTestDataSize));

    // But trying to write should result in failure.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));

    // As will trying to start another two-phase write.
    write_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));

    dp->ProducerClose();
  }

  // Test closing the consumer first, then the producer, with an active
  // two-phase write.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Start two-phase write.
    void* write_buffer_ptr = nullptr;
    uint32_t num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(write_buffer_ptr);
    ASSERT_GT(num_bytes, kTestDataSize);

    dp->ConsumerClose();
    dp->ProducerClose();
  }

  // Test closing the producer and then trying to read (with no data).
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // Peek that data.
    char buffer[1000];
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(buffer),
                                   MakeUserPointer(&num_bytes), false, true));
    EXPECT_EQ(kTestDataSize, num_bytes);
    EXPECT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

    // Read that data.
    memset(buffer, 0, 1000);
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(buffer),
                                   MakeUserPointer(&num_bytes), false, false));
    EXPECT_EQ(kTestDataSize, num_bytes);
    EXPECT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

    // A second read should fail.
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerReadData(UserPointer<void>(buffer),
                                   MakeUserPointer(&num_bytes), false, false));

    // A two-phase read should also fail.
    const void* read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));

    // Ditto for discard.
    num_bytes = 10u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), false));

    dp->ConsumerClose();
  }
}

TEST(LocalDataPipeImplTest, TwoPhaseMoreInvalidArguments) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      10 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

  // No data.
  uint32_t num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Try "ending" a two-phase write when one isn't active.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ProducerEndWriteData(1u * sizeof(int32_t)));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (too much).
  num_bytes = 0u;
  void* write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ProducerEndWriteData(num_bytes +
                                     static_cast<uint32_t>(sizeof(int32_t))));

  // But the two-phase write still ended.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, dp->ProducerEndWriteData(0u));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_GE(num_bytes, 1u);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dp->ProducerEndWriteData(1u));

  // But the two-phase write still ended.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, dp->ProducerEndWriteData(0u));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Now write some data, so we'll be able to try reading.
  int32_t element = 123;
  num_bytes = 1u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(&element),
                                  MakeUserPointer(&num_bytes), false));

  // One element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try "ending" a two-phase read when one isn't active.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerEndReadData(1u * sizeof(int32_t)));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (too much).
  num_bytes = 0u;
  const void* read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerEndReadData(num_bytes +
                                    static_cast<uint32_t>(sizeof(int32_t))));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);
  EXPECT_EQ(123, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dp->ConsumerEndReadData(1u));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  dp->ProducerClose();
  dp->ConsumerClose();
}

}  // namespace
}  // namespace system
}  // namespace mojo
