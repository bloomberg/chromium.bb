// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_FILE_STREAM_READER_TEST_UTILS_H_
#define STORAGE_BROWSER_FILEAPI_FILE_STREAM_READER_TEST_UTILS_H_

#include <string>

namespace storage {

class FileStreamReader;

// Reads upto |size| bytes of data from |reader|, an initialized
// FileStreamReader. The read bytes will be written to |data| and the actual
// number of bytes or the error code will be written to |result|.
void ReadFromReader(FileStreamReader* reader,
                    std::string* data,
                    size_t size,
                    int* result);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_FILE_STREAM_READER_TEST_UTILS_H_