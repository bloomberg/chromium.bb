// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_BUILDER_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_BUILDER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/storage_browser_export.h"

namespace storage {
class BlobStorageContext;

class STORAGE_EXPORT BlobDataBuilder {
 public:
  explicit BlobDataBuilder(const std::string& uuid);
  ~BlobDataBuilder();

  const std::string& uuid() const { return uuid_; }

  void AppendData(const std::string& data) {
    AppendData(data.c_str(), data.size());
  }

  void AppendData(const char* data, size_t length);

  // You must know the length of the file, you cannot use kuint64max to specify
  // the whole file.  This method creates a ShareableFileReference to the given
  // file, which is stored in this builder.
  void AppendFile(const base::FilePath& file_path,
                  uint64_t offset,
                  uint64_t length,
                  const base::Time& expected_modification_time);

  void AppendBlob(const std::string& uuid, uint64_t offset, uint64_t length);

  void AppendBlob(const std::string& uuid);

  void AppendFileSystemFile(const GURL& url,
                            uint64_t offset,
                            uint64_t length,
                            const base::Time& expected_modification_time);

  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

 private:
  friend class BlobStorageContext;
  friend bool operator==(const BlobDataBuilder& a, const BlobDataBuilder& b);
  friend bool operator==(const BlobDataSnapshot& a, const BlobDataBuilder& b);

  std::string uuid_;
  std::string content_type_;
  std::string content_disposition_;
  std::vector<scoped_refptr<BlobDataItem>> items_;

  DISALLOW_COPY_AND_ASSIGN(BlobDataBuilder);
};

#if defined(UNIT_TEST)
inline bool operator==(const BlobDataBuilder& a, const BlobDataBuilder& b) {
  if (a.content_type_ != b.content_type_)
    return false;
  if (a.content_disposition_ != b.content_disposition_)
    return false;
  if (a.items_.size() != b.items_.size())
    return false;
  for (size_t i = 0; i < a.items_.size(); ++i) {
    if (a.items_[i] != b.items_[i])
      return false;
  }
  return true;
}

inline bool operator==(const BlobDataSnapshot& a, const BlobDataBuilder& b) {
  if (a.content_type() != b.content_type_) {
    return false;
  }
  if (a.content_disposition() != b.content_disposition_) {
    return false;
  }
  if (a.items().size() != b.items_.size()) {
    return false;
  }
  for (size_t i = 0; i < a.items().size(); ++i) {
    if (*(a.items()[i]) != *(b.items_[i]))
      return false;
  }
  return true;
}

inline bool operator!=(const BlobDataSnapshot& a, const BlobDataBuilder& b) {
  return !(a == b);
}

inline bool operator!=(const BlobDataBuilder& a, const BlobDataBuilder& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_BUILDER_H_
