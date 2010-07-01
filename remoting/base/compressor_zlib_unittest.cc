// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "remoting/base/compressor_zlib.h"
#include "testing/gtest/include/gtest/gtest.h"

static void GenerateTestData(uint8* data, int size, int seed) {
  srand(seed);
  for (int i = 0; i < size; ++i)
    data[i] = rand() % 256;
}

// Keep compressing |input_data| into |output_data| until the last
// bytes is consumed.
static void Compress(remoting::Compressor* compressor,
                     const uint8* input_data, int input_size,
                     uint8* output_data, int output_size) {

  // Feed data into the compress until the end.
  // This loop will rewrite |output_data| continuously.
  while (input_size) {
    int consumed, written;
    compressor->Write(input_data, input_size,
                      output_data, output_size,
                      &consumed, &written);
    input_data += consumed;
    input_size -= consumed;
  }

  // And then flush the remaining data from the compressor.
  int written;
  while (compressor->Flush(output_data, output_size, &written)) {
  }
}

TEST(CompressorZlibTest, SimpleCompress) {
  static const int kRawDataSize = 1024 * 128;
  static const int kCompressedDataSize = 256;
  uint8 raw_data[kRawDataSize];
  uint8 compressed_data[kCompressedDataSize];

  // Generate the test data.g
  GenerateTestData(raw_data, kRawDataSize, 99);

  // Then use the compressor to compress.
  remoting::CompressorZlib compressor;
  Compress(&compressor, raw_data, kRawDataSize,
           compressed_data, kCompressedDataSize);
}
