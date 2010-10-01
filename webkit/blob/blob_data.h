// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_DATA_H_
#define WEBKIT_BLOB_BLOB_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace WebKit {
class WebBlobData;
}

namespace webkit_blob {

class BlobData : public base::RefCounted<BlobData> {
 public:
  enum Type {
    TYPE_DATA,
    TYPE_FILE,
    TYPE_BLOB
  };

  class Item {
   public:
    Item();
    ~Item();

    Type type() const { return type_; }
    const std::string& data() const { return data_; }
    const FilePath& file_path() const { return file_path_; }
    const GURL& blob_url() const { return blob_url_; }
    uint64 offset() const { return offset_; }
    uint64 length() const { return length_; }
    const base::Time& expected_modification_time() const {
      return expected_modification_time_;
    }

    void SetToData(const std::string& data) {
      SetToData(data, 0, static_cast<uint32>(data.size()));
    }

    void SetToData(const std::string& data, uint32 offset, uint32 length) {
      // TODO(jianli): Need to implement ref-counting vector storage.
      type_ = TYPE_DATA;
      data_ = data;
      offset_ = offset;
      length_ = length;
    }

    void SetToFile(const FilePath& file_path, uint64 offset, uint64 length,
                   const base::Time& expected_modification_time) {
      type_ = TYPE_FILE;
      file_path_ = file_path;
      offset_ = offset;
      length_ = length;
      expected_modification_time_ = expected_modification_time;
    }

    void SetToBlob(const GURL& blob_url, uint64 offset, uint64 length) {
      type_ = TYPE_BLOB;
      blob_url_ = blob_url;
      offset_ = offset;
      length_ = length;
    }

   private:
    Type type_;

    // For Data type.
    std::string data_;

    // For File type.
    FilePath file_path_;

    // For Blob typ.
    GURL blob_url_;

    uint64 offset_;
    uint64 length_;
    base::Time expected_modification_time_;
  };

  BlobData();
  explicit BlobData(const WebKit::WebBlobData& data);

  void AppendData(const std::string& data) {
    // TODO(jianli): Consider writing the big data to the disk.
    AppendData(data, 0, static_cast<uint32>(data.size()));
  }

  void AppendData(const std::string& data, uint32 offset, uint32 length) {
    if (length > 0) {
      items_.push_back(Item());
      items_.back().SetToData(data, offset, length);
    }
  }

  void AppendFile(const FilePath& file_path, uint64 offset, uint64 length,
                  const base::Time& expected_modification_time) {
    items_.push_back(Item());
    items_.back().SetToFile(file_path, offset, length,
                            expected_modification_time);
  }

  void AppendBlob(const GURL& blob_url, uint64 offset, uint64 length) {
    items_.push_back(Item());
    items_.back().SetToBlob(blob_url, offset, length);
  }

  const std::vector<Item>& items() const { return items_; }
  void set_items(const std::vector<Item>& items) {
    items_ = items;
  }
  void swap_items(std::vector<Item>* items) {
    items_.swap(*items);
  }

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

 private:
  friend class base::RefCounted<BlobData>;

  virtual ~BlobData();

  std::string content_type_;
  std::string content_disposition_;
  std::vector<Item> items_;
};

#if defined(UNIT_TEST)
inline bool operator==(const BlobData::Item& a, const BlobData::Item& b) {
  if (a.type() != b.type())
    return false;
  if (a.type() == BlobData::TYPE_DATA) {
    return a.data() == b.data() &&
           a.offset() == b.offset() &&
           a.length() == b.length();
  }
  if (a.type() == BlobData::TYPE_FILE) {
    return a.file_path() == b.file_path() &&
           a.offset() == b.offset() &&
           a.length() == b.length() &&
           a.expected_modification_time() == b.expected_modification_time();
  }
  if (a.type() == BlobData::TYPE_BLOB) {
    return a.blob_url() == b.blob_url() &&
           a.offset() == b.offset() &&
           a.length() == b.length();
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
