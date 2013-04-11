// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/media/picasa/pmp_column_reader.h"
#include "webkit/fileapi/media/picasa/pmp_constants.h"
#include "webkit/fileapi/media/picasa/pmp_test_helper.h"

namespace {

using picasaimport::PmpColumnReader;
using picasaimport::PmpTestHelper;

// Overridden version of Read method to make test code templatable.
bool DoRead(const PmpColumnReader* reader, uint32 row, std::string* target) {
  return reader->ReadString(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, uint32* target) {
  return reader->ReadUInt32(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, double* target) {
  return reader->ReadDouble64(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, uint8* target) {
  return reader->ReadUInt8(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, uint64* target) {
  return reader->ReadUInt64(row, target);
}

// TestValid
template<class T>
void TestValid(const picasaimport::PmpFieldType field_type,
               const std::vector<T>& elems) {
  PmpTestHelper test_helper;
  ASSERT_TRUE(test_helper.Init());

  PmpColumnReader reader;

  uint32 rows_read = 0xFF;

  std::vector<uint8> data =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size(), elems);
  ASSERT_TRUE(test_helper.InitColumnReaderFromBytes(&reader, data, &rows_read));
  EXPECT_EQ(elems.size(), rows_read);

  for (uint32 i = 0; i < elems.size() && i < rows_read; i++) {
    T target;
    EXPECT_TRUE(DoRead(&reader, i, &target));
    EXPECT_EQ(elems[i], target);
  }
}

template<class T>
void TestMalformed(const picasaimport::PmpFieldType field_type,
                   const std::vector<T>& elems) {
  PmpTestHelper test_helper;
  ASSERT_TRUE(test_helper.Init());

  PmpColumnReader reader1, reader2, reader3, reader4;

  std::vector<uint8> data_too_few_declared_rows =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size()-1, elems);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader1, data_too_few_declared_rows, NULL));

  std::vector<uint8> data_too_many_declared_rows =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size()+1, elems);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader2, data_too_many_declared_rows, NULL));

  std::vector<uint8> data_truncated =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size(), elems);
  data_truncated.resize(data_truncated.size()-10);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader3, data_truncated, NULL));

  std::vector<uint8> data_padded =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size(), elems);
  data_padded.resize(data_padded.size()+10);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader4, data_padded, NULL));
}

template<class T>
void TestPrimitive(const picasaimport::PmpFieldType field_type) {
  // Make an ascending vector of the primitive.
  uint32 n = 100;
  std::vector<T> data(n, 0);
  for (uint32 i = 0; i < n; i++) {
    data[i] = i*3;
  }

  TestValid<T>(field_type, data);
  TestMalformed<T>(field_type, data);
}


TEST(PmpColumnReaderTest, HeaderParsingAndValidation) {
  PmpTestHelper test_helper;
  ASSERT_TRUE(test_helper.Init());

  PmpColumnReader reader1, reader2, reader3, reader4, reader5;

  // Good header.
  uint32 rows_read = 0xFF;
  std::vector<uint8> good_header = PmpTestHelper::MakeHeader(
      picasaimport::PMP_TYPE_STRING, 0);
  ASSERT_TRUE(test_helper.InitColumnReaderFromBytes(
      &reader1, good_header, &rows_read));
  EXPECT_EQ(0U, rows_read) << "Read non-zero rows from header-only data.";

  // Botch up elements of the header.
  std::vector<uint8> bad_magic_byte = PmpTestHelper::MakeHeader(
      picasaimport::PMP_TYPE_STRING, 0);
  bad_magic_byte[0] = 0xff;
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader2, bad_magic_byte, NULL));

  // Corrupt means the type fields don't agree.
  std::vector<uint8> corrupt_type = PmpTestHelper::MakeHeader(
      picasaimport::PMP_TYPE_STRING, 0);
  corrupt_type[picasaimport::kPmpFieldType1Offset] = 0xff;
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader3, corrupt_type, NULL));

  std::vector<uint8> invalid_type = PmpTestHelper::MakeHeader(
      picasaimport::PMP_TYPE_STRING, 0);
  invalid_type[picasaimport::kPmpFieldType1Offset] = 0xff;
  invalid_type[picasaimport::kPmpFieldType2Offset] = 0xff;
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader4, invalid_type, NULL));

  std::vector<uint8> incomplete_header = PmpTestHelper::MakeHeader(
      picasaimport::PMP_TYPE_STRING, 0);
  incomplete_header.resize(10);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader5, incomplete_header, NULL));
}

TEST(PmpColumnReaderTest, StringParsing) {
  std::vector<std::string> empty_strings(100, "");

  // Test empty strings read okay.
  TestValid(picasaimport::PMP_TYPE_STRING, empty_strings);

  std::vector<std::string> mixed_strings;
  mixed_strings.push_back("");
  mixed_strings.push_back("Hello");
  mixed_strings.push_back("World");
  mixed_strings.push_back("");
  mixed_strings.push_back("123123");
  mixed_strings.push_back("Q");
  mixed_strings.push_back("");

  // Test that a mixed set of strings read correctly.
  TestValid(picasaimport::PMP_TYPE_STRING, mixed_strings);

  // Test with the data messed up in a variety of ways.
  TestMalformed(picasaimport::PMP_TYPE_STRING, mixed_strings);
}

TEST(PmpColumnReaderTest, PrimitiveParsing) {
  TestPrimitive<uint32>(picasaimport::PMP_TYPE_UINT32);
  TestPrimitive<double>(picasaimport::PMP_TYPE_DOUBLE64);
  TestPrimitive<uint8>(picasaimport::PMP_TYPE_UINT8);
  TestPrimitive<uint64>(picasaimport::PMP_TYPE_UINT64);
}

}  // namespace
