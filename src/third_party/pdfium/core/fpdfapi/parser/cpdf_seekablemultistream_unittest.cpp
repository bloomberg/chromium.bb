// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_seekablemultistream.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CPDFSeekableMultiStreamTest, NoStreams) {
  std::vector<const CPDF_Stream*> streams;
  auto fileread = pdfium::MakeRetain<CPDF_SeekableMultiStream>(streams);

  uint8_t output_buffer[16];
  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_FALSE(fileread->ReadBlockAtOffset(output_buffer, 0, 0));
  EXPECT_EQ(0xbd, output_buffer[0]);
}

TEST(CXFAFileReadTest, EmptyStreams) {
  std::vector<const CPDF_Stream*> streams;
  auto stream1 = pdfium::MakeRetain<CPDF_Stream>();
  streams.push_back(stream1.Get());
  auto fileread = pdfium::MakeRetain<CPDF_SeekableMultiStream>(streams);

  uint8_t output_buffer[16];
  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_FALSE(fileread->ReadBlockAtOffset(output_buffer, 0, 0));
  EXPECT_EQ(0xbd, output_buffer[0]);
}

TEST(CXFAFileReadTest, NormalStreams) {
  std::vector<const CPDF_Stream*> streams;
  auto stream1 = pdfium::MakeRetain<CPDF_Stream>();
  auto stream2 = pdfium::MakeRetain<CPDF_Stream>();
  auto stream3 = pdfium::MakeRetain<CPDF_Stream>();

  // 16 chars total.
  stream1->InitStream(ByteStringView("one t").raw_span(),
                      pdfium::MakeRetain<CPDF_Dictionary>());
  stream2->InitStream(ByteStringView("wo ").raw_span(),
                      pdfium::MakeRetain<CPDF_Dictionary>());
  stream3->InitStream(ByteStringView("three!!!").raw_span(),
                      pdfium::MakeRetain<CPDF_Dictionary>());

  streams.push_back(stream1.Get());
  streams.push_back(stream2.Get());
  streams.push_back(stream3.Get());
  auto fileread = pdfium::MakeRetain<CPDF_SeekableMultiStream>(streams);

  uint8_t output_buffer[16];
  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_TRUE(fileread->ReadBlockAtOffset(output_buffer, 0, 0));
  EXPECT_EQ(0xbd, output_buffer[0]);

  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_TRUE(fileread->ReadBlockAtOffset(output_buffer, 1, 0));
  EXPECT_EQ(0xbd, output_buffer[0]);

  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_TRUE(fileread->ReadBlockAtOffset(output_buffer, 0, 1));
  EXPECT_EQ(0, memcmp(output_buffer, "o", 1));
  EXPECT_EQ(0xbd, output_buffer[1]);

  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_TRUE(
      fileread->ReadBlockAtOffset(output_buffer, 0, sizeof(output_buffer)));
  EXPECT_EQ(0, memcmp(output_buffer, "one two three!!!", 16));

  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_TRUE(fileread->ReadBlockAtOffset(output_buffer, 2, 10));
  EXPECT_EQ(0, memcmp(output_buffer, "e two thre", 10));
  EXPECT_EQ(0xbd, output_buffer[11]);

  memset(output_buffer, 0xbd, sizeof(output_buffer));
  EXPECT_FALSE(
      fileread->ReadBlockAtOffset(output_buffer, 1, sizeof(output_buffer)));
  EXPECT_EQ(0, memcmp(output_buffer, "ne two three!!!", 15));
  EXPECT_EQ(0xbd, output_buffer[15]);
}
