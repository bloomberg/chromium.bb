// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "remoting/base/compressor_zlib.h"
#include "remoting/base/decompressor_zlib.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static void GenerateTestData(uint8* data, int size, int seed) {
  srand(seed);
  for (int i = 0; i < size; ++i)
    data[i] = rand() % 256;
}

// Keep compressing |input_data| into |output_data| until the last
// bytes is consumed.
//
// The compressed size is written to |compressed_size|.
static void Compress(remoting::Compressor* compressor,
                     const uint8* input_data, int input_size,
                     uint8* output_data, int output_size,
                     int* compressed_size) {
  *compressed_size = 0;
  while (true) {
    int consumed, written;
    bool ret = compressor->Process(
        input_data, input_size, output_data, output_size,
        input_size == 0 ?
            Compressor::CompressorFinish : Compressor::CompressorNoFlush,
        &consumed, &written);
    input_data += consumed;
    input_size -= consumed;
    output_data += written;
    output_size -= written;
    *compressed_size += written;

    if (!ret)
      break;
  }
}

// The decompressed size is written to |decompressed_size|.
static void Decompress(remoting::Decompressor* decompressor,
                       const uint8* input_data, int input_size,
                       uint8* output_data, int output_size,
                       int* decompressed_size) {
  *decompressed_size = 0;
  while (true) {
    int consumed, written;
    bool ret = decompressor->Process(input_data, input_size,
                                     output_data, output_size,
                                     &consumed, &written);
    input_data += consumed;
    input_size -= consumed;
    output_data += written;
    output_size -= written;
    *decompressed_size += written;

    if (!ret)
      break;
  }
}

TEST(DecompressorZlibTest, CompressAndDecompress) {
  static const int kRawDataSize = 1024 * 128;
  static const int kCompressedDataSize = 2 * kRawDataSize;
  static const int kDecompressedDataSize = kRawDataSize;

  uint8 raw_data[kRawDataSize];
  uint8 compressed_data[kCompressedDataSize];
  uint8 decompressed_data[kDecompressedDataSize];

  // Generate the test data.g
  GenerateTestData(raw_data, kRawDataSize, 99);

  // Then use the compressor.
  remoting::CompressorZlib compressor;
  int compressed_size = 0;
  Compress(&compressor, raw_data, kRawDataSize,
           compressed_data, kCompressedDataSize,
           &compressed_size);

  // Then use the decompressor.
  remoting::DecompressorZlib decompressor;
  int decompressed_size = 0;
  Decompress(&decompressor, compressed_data, compressed_size,
             decompressed_data, kDecompressedDataSize,
             &decompressed_size);

  EXPECT_EQ(kRawDataSize, decompressed_size);
  EXPECT_EQ(0, memcmp(raw_data, decompressed_data, decompressed_size));
}

// Checks that zlib can work with a small output buffer by limiting
// number of bytes it gets from the input.
TEST(DecompressorZlibTest, SmallOutputBuffer) {
  static const int kRawDataSize = 1024 * 128;
  static const int kCompressedDataSize = 2 * kRawDataSize;

  uint8 raw_data[kRawDataSize];
  uint8 compressed_data[kCompressedDataSize];

  // Generate the test data.
  GenerateTestData(raw_data, kRawDataSize, 99);

  // Then use the compressor to compress.
  remoting::CompressorZlib compressor;
  int compressed_size = 0;
  Compress(&compressor, raw_data, kRawDataSize,
           compressed_data, kCompressedDataSize,
           &compressed_size);

  // Then use the decompressor. We decompress into a 1 byte buffer
  // every time.
  remoting::DecompressorZlib decompressor;
  uint8* input_data = compressed_data;
  int input_size = compressed_size;
  int decompressed_size = 0;
  while (true) {
    int consumed, written;
    uint8 output_data;
    bool ret = decompressor.Process(input_data, input_size,
                                    &output_data, 1,
                                    &consumed, &written);
    input_data += consumed;
    input_size -= consumed;

    // Expect that there's only one byte written.
    EXPECT_EQ(1, written);
    decompressed_size += written;

    if (!ret)
      break;
  }
  EXPECT_EQ(kRawDataSize, decompressed_size);
}

}  // namespace remoting
