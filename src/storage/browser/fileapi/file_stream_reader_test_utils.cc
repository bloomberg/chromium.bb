// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_stream_reader_test_utils.h"

#include <string>

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

void ReadFromReader(FileStreamReader* reader,
                    std::string* data,
                    size_t size,
                    int* result) {
  ASSERT_TRUE(reader != nullptr);
  ASSERT_TRUE(result != nullptr);
  *result = net::OK;
  net::TestCompletionCallback callback;
  size_t total_bytes_read = 0;
  while (total_bytes_read < size) {
    scoped_refptr<net::IOBufferWithSize> buf(
        base::MakeRefCounted<net::IOBufferWithSize>(size - total_bytes_read));
    int rv = reader->Read(buf.get(), buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      *result = rv;
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data->append(buf->data(), rv);
  }
}

}  // namespace storage
