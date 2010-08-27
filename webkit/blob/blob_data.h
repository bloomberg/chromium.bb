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
    Item()
      : type_(TYPE_DATA),
        offset_(0),
        length_(0) {
    }

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

#if defined(UNIT_TEST)
    bool operator==(const Item& other) const {
      if (type_ != other.type_)
        return false;
      if (type_ == TYPE_DATA) {
        return data_ == other.data_ &&
               offset_ == other.offset_ &&
               length_ == other.length_;
      }
      if (type_ == TYPE_FILE) {
        return file_path_ == other.file_path_ &&
               offset_ == other.offset_ &&
               length_ == other.length_ &&
               expected_modification_time_ == other.expected_modification_time_;
      }
      if (type_ == TYPE_BLOB) {
        return blob_url_ == other.blob_url_ &&
               offset_ == other.offset_ &&
               length_ == other.length_;
      }
      return false;
    }

    bool operator!=(const Item& other) const {
      return !(*this == other);
    }
#endif  // defined(UNIT_TEST)

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

  BlobData() { }
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

#if defined(UNIT_TEST)
  bool operator==(const BlobData& other) const {
    if (content_type_ != other.content_type_)
      return false;
    if (content_disposition_ != other.content_disposition_)
      return false;
    if (items_.size() != other.items_.size())
      return false;
    for (size_t i = 0; i < items_.size(); ++i) {
      if (items_[i] != other.items_[i])
        return false;
    }
    return true;
  }
#endif  // defined(UNIT_TEST)

 private:
  friend class base::RefCounted<BlobData>;

  virtual ~BlobData() { }

  std::string content_type_;
  std::string content_disposition_;
  std::vector<Item> items_;
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_BLOB_DATA_H_
