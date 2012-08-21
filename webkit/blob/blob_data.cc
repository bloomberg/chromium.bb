// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_data.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

namespace webkit_blob {

BlobData::Item::Item()
    : type(TYPE_DATA),
      data_external(NULL),
      offset(0),
      length(0) {
}

BlobData::Item::~Item() {}

BlobData::BlobData() {}

BlobData::~BlobData() {}

void BlobData::AppendData(const char* data, size_t length) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToData(data, length);
}

void BlobData::AppendFile(const FilePath& file_path, uint64 offset,
                          uint64 length,
                          const base::Time& expected_modification_time) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToFile(file_path, offset, length,
                          expected_modification_time);
}

void BlobData::AppendBlob(const GURL& blob_url, uint64 offset, uint64 length) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToBlob(blob_url, offset, length);
}

int64 BlobData::GetMemoryUsage() const {
  int64 memory = 0;
  for (std::vector<Item>::const_iterator iter = items_.begin();
       iter != items_.end(); ++iter) {
    if (iter->type == TYPE_DATA)
      memory += iter->data.size();
  }
  return memory;
}

}  // namespace webkit_blob
