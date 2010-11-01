// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "remoting/base/multiple_array_input_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// TODO(sergeyu): Add SCOPED_TRACE() for ReadFromInput() and ReadString().

static size_t ReadFromInput(MultipleArrayInputStream* input,
                         void* data, size_t size) {
  uint8* out = reinterpret_cast<uint8*>(data);
  int out_size = size;

  const void* in;
  int in_size = 0;

  while (true) {
    if (!input->Next(&in, &in_size)) {
      return size - out_size;
    }
    EXPECT_GT(in_size, -1);

    if (out_size <= in_size) {
      memcpy(out, in, out_size);
      if (in_size > out_size) {
        input->BackUp(in_size - out_size);
      }
      return size;  // Copied all of it.
    }

    memcpy(out, in, in_size);
    out += in_size;
    out_size -= in_size;
  }
}

static void ReadString(MultipleArrayInputStream* input,
                       const std::string& str) {
  scoped_array<char> buffer(new char[str.size() + 1]);
  buffer[str.size()] = '\0';
  EXPECT_EQ(ReadFromInput(input, buffer.get(), str.size()), str.size());
  EXPECT_STREQ(str.c_str(), buffer.get());
}

// Construct and prepare data in the |output_stream|.
static void PrepareData(scoped_ptr<MultipleArrayInputStream>* stream) {
  static const std::string kTestData =
      "Hello world!"
      "This is testing"
      "MultipleArrayInputStream"
      "for Chromoting";

  // Determine how many segments to split kTestData. We split the data in
  // 1 character, 2 characters, 1 character, 2 characters ...
  int segments = (kTestData.length() / 3) * 2;
  int remaining_chars = kTestData.length() % 3;
  if (remaining_chars) {
    if (remaining_chars == 1)
      ++segments;
    else
      segments += 2;
  }

  MultipleArrayInputStream* mstream = new MultipleArrayInputStream();
  const char* data = kTestData.c_str();
  for (int i = 0; i < segments; ++i) {
    int size = i % 2 == 0 ? 1 : 2;
    mstream->AddBuffer(new net::StringIOBuffer(std::string(data, size)), size);
    data += size;
  }
  stream->reset(mstream);
}

TEST(MultipleArrayInputStreamTest, BasicOperations) {
  scoped_ptr<MultipleArrayInputStream> stream;
  PrepareData(&stream);

  ReadString(stream.get(), "Hello world!");
  ReadString(stream.get(), "This ");
  ReadString(stream.get(), "is test");
  EXPECT_TRUE(stream->Skip(3));
  ReadString(stream.get(), "MultipleArrayInput");
  EXPECT_TRUE(stream->Skip(6));
  ReadString(stream.get(), "f");
  ReadString(stream.get(), "o");
  ReadString(stream.get(), "r");
  ReadString(stream.get(), " ");
  ReadString(stream.get(), "Chromoting");
}

}  // namespace remoting
