// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_DATA_H_
#define WEBKIT_BLOB_BLOB_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/blob/blob_export.h"
#include "webkit/blob/shareable_file_reference.h"

namespace WebKit {
class WebBlobData;
}

namespace webkit_blob {

class BLOB_EXPORT BlobData : public base::RefCounted<BlobData> {
 public:
  enum Type {
    TYPE_DATA,
    TYPE_DATA_EXTERNAL,
    TYPE_FILE,
    TYPE_BLOB
  };

  struct BLOB_EXPORT Item {
    Item();
    ~Item();

    void SetToData(const std::string& data) {
      SetToData(data.c_str(), data.size());
    }

    void SetToData(const char* data, size_t length) {
      type = TYPE_DATA;
      this->data.assign(data, length);
      this->offset = 0;
      this->length = length;
    }

    void SetToDataExternal(const char* data, size_t length) {
      type = TYPE_DATA_EXTERNAL;
      this->data_external = data;
      this->offset = 0;
      this->length = length;
    }

    void SetToFile(const FilePath& file_path, uint64 offset, uint64 length,
                   const base::Time& expected_modification_time) {
      type = TYPE_FILE;
      this->file_path = file_path;
      this->offset = offset;
      this->length = length;
      this->expected_modification_time = expected_modification_time;
    }

    void SetToBlob(const GURL& blob_url, uint64 offset, uint64 length) {
      type = TYPE_BLOB;
      this->blob_url = blob_url;
      this->offset = offset;
      this->length = length;
    }

    Type type;
    std::string data;  // For Data type.
    const char* data_external;  // For DataExternal type.
    GURL blob_url;  // For Blob type.
    FilePath file_path;  // For File type.
    base::Time expected_modification_time;  // Also for File type.
    uint64 offset;
    uint64 length;
  };

  BlobData();
  explicit BlobData(const WebKit::WebBlobData& data);

  void AppendData(const std::string& data) {
    AppendData(data.c_str(), data.size());
  }

  void AppendData(const char* data, size_t length);

  void AppendFile(const FilePath& file_path, uint64 offset, uint64 length,
                  const base::Time& expected_modification_time);

  void AppendBlob(const GURL& blob_url, uint64 offset, uint64 length);

  void AttachShareableFileReference(ShareableFileReference* reference) {
    shareable_files_.push_back(reference);
  }

  const std::vector<Item>& items() const { return items_; }

  const std::string& content_type() const { return content_type_; }
  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  const std::string& content_disposition() const {
    return content_disposition_;
  }
  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

  int64 GetMemoryUsage() const;

 private:
  friend class base::RefCounted<BlobData>;

  virtual ~BlobData();

  std::string content_type_;
  std::string content_disposition_;
  std::vector<Item> items_;
  std::vector<scoped_refptr<ShareableFileReference> > shareable_files_;

  DISALLOW_COPY_AND_ASSIGN(BlobData);
};

#if defined(UNIT_TEST)
inline bool operator==(const BlobData::Item& a, const BlobData::Item& b) {
  if (a.type != b.type)
    return false;
  if (a.type == BlobData::TYPE_DATA) {
    return a.data == b.data &&
           a.offset == b.offset &&
           a.length == b.length;
  }
  if (a.type == BlobData::TYPE_FILE) {
    return a.file_path == b.file_path &&
           a.offset == b.offset &&
           a.length == b.length &&
           a.expected_modification_time == b.expected_modification_time;
  }
  if (a.type == BlobData::TYPE_BLOB) {
    return a.blob_url == b.blob_url &&
           a.offset == b.offset &&
           a.length == b.length;
  }
  return false;
}

inline  bool operator!=(const BlobData::Item& a, const BlobData::Item& b) {
  return !(a == b);
}

inline bool operator==(const BlobData& a, const BlobData& b) {
  if (a.content_type() != b.content_type())
    return false;
  if (a.content_disposition() != b.content_disposition())
    return false;
  if (a.items().size() != b.items().size())
    return false;
  for (size_t i = 0; i < a.items().size(); ++i) {
    if (a.items()[i] != b.items()[i])
      return false;
  }
  return true;
}

inline bool operator!=(const BlobData& a, const BlobData& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_BLOB_DATA_H_
