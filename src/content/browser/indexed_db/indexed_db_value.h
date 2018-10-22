// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT IndexedDBValue {
  IndexedDBValue();
  IndexedDBValue(const std::string& input_bits,
                 const std::vector<IndexedDBBlobInfo>& input_blob_info);
  IndexedDBValue(const IndexedDBValue& other);
  ~IndexedDBValue();
  IndexedDBValue& operator=(const IndexedDBValue& other);

  void swap(IndexedDBValue& value) {
    bits.swap(value.bits);
    blob_info.swap(value.blob_info);
  }

  bool empty() const { return bits.empty(); }
  void clear() {
    bits.clear();
    blob_info.clear();
  }

  size_t SizeEstimate() const {
    return bits.size() + blob_info.size() * sizeof(IndexedDBBlobInfo);
  }

  std::string bits;
  std::vector<IndexedDBBlobInfo> blob_info;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_
