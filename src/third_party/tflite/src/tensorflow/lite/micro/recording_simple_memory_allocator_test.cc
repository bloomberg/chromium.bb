/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/recording_simple_memory_allocator.h"

#include <cstdint>

#include "tensorflow/lite/micro/test_helpers.h"
#include "tensorflow/lite/micro/testing/micro_test.h"

TF_LITE_MICRO_TESTS_BEGIN

TF_LITE_MICRO_TEST(TestRecordsTailAllocations) {
  constexpr size_t arena_size = 1024;
  uint8_t arena[arena_size];
  tflite::RecordingSimpleMemoryAllocator allocator(micro_test::reporter, arena,
                                                   arena_size);

  uint8_t* result = allocator.AllocateFromTail(/*size=*/10, /*alignment=*/1);
  TF_LITE_MICRO_EXPECT_NE(result, nullptr);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetUsedBytes(), 10);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 10);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 1);

  result = allocator.AllocateFromTail(/*size=*/20, /*alignment=*/1);
  TF_LITE_MICRO_EXPECT_NE(result, nullptr);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetUsedBytes(), 30);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 30);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 2);
}

TF_LITE_MICRO_TEST(TestRecordsMisalignedTailAllocations) {
  constexpr size_t arena_size = 1024;
  uint8_t arena[arena_size];
  tflite::RecordingSimpleMemoryAllocator allocator(micro_test::reporter, arena,
                                                   arena_size);

  uint8_t* result = allocator.AllocateFromTail(/*size=*/10, /*alignment=*/12);
  TF_LITE_MICRO_EXPECT_NE(result, nullptr);
  // Validate used bytes in 8 byte range that can included alignment of 12:
  TF_LITE_MICRO_EXPECT_GE(allocator.GetUsedBytes(), 10);
  TF_LITE_MICRO_EXPECT_LE(allocator.GetUsedBytes(), 20);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 10);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 1);
}

TF_LITE_MICRO_TEST(TestDoesNotRecordFailedTailAllocations) {
  constexpr size_t arena_size = 1024;
  uint8_t arena[arena_size];
  tflite::RecordingSimpleMemoryAllocator allocator(micro_test::reporter, arena,
                                                   arena_size);

  uint8_t* result = allocator.AllocateFromTail(/*size=*/2048, /*alignment=*/1);
  TF_LITE_MICRO_EXPECT_EQ(result, nullptr);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetUsedBytes(), 0);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 0);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 0);
}

TF_LITE_MICRO_TEST(TestRecordsHeadAllocations) {
  constexpr size_t arena_size = 1024;
  uint8_t arena[arena_size];
  tflite::RecordingSimpleMemoryAllocator allocator(micro_test::reporter, arena,
                                                   arena_size);

  uint8_t* result = allocator.AllocateFromHead(/*size=*/5, /*alignment=*/1);
  TF_LITE_MICRO_EXPECT_NE(result, nullptr);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetUsedBytes(), 5);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 5);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 1);

  result = allocator.AllocateFromTail(/*size=*/15, /*alignment=*/1);
  TF_LITE_MICRO_EXPECT_NE(result, nullptr);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetUsedBytes(), 20);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 20);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 2);
}

TF_LITE_MICRO_TEST(TestRecordsMisalignedHeadAllocations) {
  constexpr size_t arena_size = 1024;
  uint8_t arena[arena_size];
  tflite::RecordingSimpleMemoryAllocator allocator(micro_test::reporter, arena,
                                                   arena_size);

  uint8_t* result = allocator.AllocateFromHead(/*size=*/10, /*alignment=*/12);
  TF_LITE_MICRO_EXPECT_NE(result, nullptr);
  // Validate used bytes in 8 byte range that can included alignment of 12:
  TF_LITE_MICRO_EXPECT_GE(allocator.GetUsedBytes(), 10);
  TF_LITE_MICRO_EXPECT_LE(allocator.GetUsedBytes(), 20);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 10);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 1);
}

TF_LITE_MICRO_TEST(TestDoesNotRecordFailedTailAllocations) {
  constexpr size_t arena_size = 1024;
  uint8_t arena[arena_size];
  tflite::RecordingSimpleMemoryAllocator allocator(micro_test::reporter, arena,
                                                   arena_size);

  uint8_t* result = allocator.AllocateFromHead(/*size=*/2048, /*alignment=*/1);
  TF_LITE_MICRO_EXPECT_EQ(result, nullptr);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetUsedBytes(), 0);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetRequestedBytes(), 0);
  TF_LITE_MICRO_EXPECT_EQ(allocator.GetAllocatedCount(), 0);
}

TF_LITE_MICRO_TESTS_END
