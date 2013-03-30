// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_data.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

namespace webkit_blob {

BlobData::BlobData() {}

BlobData::~BlobData() {}

void BlobData::AppendData(const char* data, size_t length) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToBytes(data, length);
}

void BlobData::AppendFile(const base::FilePath& file_path,
                          uint64 offset, uint64 length,
                          const base::Time& expected_modification_time) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToFilePathRange(file_path, offset, length,
                                   expected_modification_time);
}

void BlobData::AppendBlob(const GURL& blob_url, uint64 offset, uint64 length) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToBlobUrlRange(blob_url, offset, length);
}

void BlobData::AppendFileSystemFile(
    const GURL& url, uint64 offset,
    uint64 length,
    const base::Time& expected_modification_time) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToFileSystemUrlRange(url, offset, length,
                                        expected_modification_time);
}

int64 BlobData::GetMemoryUsage() const {
  int64 memory = 0;
  for (std::vector<Item>::const_iterator iter = items_.begin();
       iter != items_.end(); ++iter) {
    if (iter->type() == Item::TYPE_BYTES)
      memory += iter->length();
  }
  return memory;
}

}  // namespace webkit_blob
