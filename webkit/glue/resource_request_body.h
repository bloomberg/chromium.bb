// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_RESOURCE_REQUEST_BODY_H_
#define WEBKIT_GLUE_RESOURCE_REQUEST_BODY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue_export.h"

namespace net {
class UploadData;
class UploadElement;
}

namespace webkit_blob {
class BlobStorageController;
}

namespace webkit_glue {

// A struct used to represent upload data. The data field is populated by
// WebURLLoader from the data given as WebHTTPBody.
// TODO(kinuko): This is basically a duplicate of net::UploadData but
// with support for higher-level abstraction data. We should reduce the
// code duplicate by sharing code for similar data structs:
// ResourceRequestBody::Element and BlobData::Item.
class WEBKIT_GLUE_EXPORT ResourceRequestBody
    : public base::RefCounted<ResourceRequestBody>,
      public base::SupportsUserData {
 public:
  enum Type {
    TYPE_BYTES,
    TYPE_FILE,
    TYPE_BLOB,
  };

  class WEBKIT_GLUE_EXPORT Element {
   public:
    Element();
    ~Element();

    Type type() const { return type_; }
    // Explicitly sets the type of this Element. Used during IPC
    // marshalling.
    void set_type(Type type) {
      type_ = type;
    }

    const char* bytes() const { return bytes_start_ ? bytes_start_ : &buf_[0]; }
    uint64 bytes_length() const { return buf_.size() + bytes_length_; }
    const FilePath& file_path() const { return file_path_; }
    uint64 file_range_offset() const { return file_range_offset_; }
    uint64 file_range_length() const { return file_range_length_; }
    // If NULL time is returned, we do not do the check.
    const base::Time& expected_file_modification_time() const {
      return expected_file_modification_time_;
    }
    const GURL& blob_url() const { return blob_url_; }

    void SetToBytes(const char* bytes, int bytes_len) {
      type_ = TYPE_BYTES;
      buf_.assign(bytes, bytes + bytes_len);
    }

    // This does not copy the given data and the caller should make sure
    // the data is secured somewhere else (e.g. by attaching the data
    // using SetUserData).
    void SetToSharedBytes(const char* bytes, int bytes_len) {
      type_ = TYPE_BYTES;
      bytes_start_ = bytes;
      bytes_length_ = bytes_len;
    }

    void SetToFilePath(const FilePath& path) {
      SetToFilePathRange(path, 0, kuint64max, base::Time());
    }

    // If expected_modification_time is NULL, we do not check for the file
    // change. Also note that the granularity for comparison is time_t, not
    // the full precision.
    void SetToFilePathRange(const FilePath& path,
                            uint64 offset, uint64 length,
                            const base::Time& expected_modification_time) {
      type_ = TYPE_FILE;
      file_path_ = path;
      file_range_offset_ = offset;
      file_range_length_ = length;
      expected_file_modification_time_ = expected_modification_time;
    }

    void SetToBlobUrl(const GURL& blob_url) {
      type_ = TYPE_BLOB;
      blob_url_ = blob_url;
    }

   private:
    Type type_;
    std::vector<char> buf_;
    const char* bytes_start_;
    uint64 bytes_length_;
    FilePath file_path_;
    uint64 file_range_offset_;
    uint64 file_range_length_;
    base::Time expected_file_modification_time_;
    GURL blob_url_;
  };

  ResourceRequestBody();

  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const FilePath& file_path,
                       uint64 offset, uint64 length,
                       const base::Time& expected_modification_time);
  void AppendBlob(const GURL& blob_url);

  // Creates a new UploadData from this request body. This also resolves
  // any blob references using given |blob_controller|.
  // TODO(kinuko): Clean up this hack.
  net::UploadData* ResolveElementsAndCreateUploadData(
      webkit_blob::BlobStorageController* blob_controller);

  const std::vector<Element>* elements() const {
    return &elements_;
  }

  std::vector<Element>* elements_mutable() {
    return &elements_;
  }

  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64 id) { identifier_ = id; }
  int64 identifier() const { return identifier_; }

 private:
  friend class base::RefCounted<ResourceRequestBody>;
  virtual ~ResourceRequestBody();

  // Resolves the |blob_url| using |blob_controller| and appends resolved
  // items to |elements|.
  void ResolveBlobReference(webkit_blob::BlobStorageController* blob_controller,
                            const GURL& blob_url,
                            std::vector<net::UploadElement>* elements);

  std::vector<Element> elements_;
  int64 identifier_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

#if defined(UNIT_TEST)
inline bool operator==(const ResourceRequestBody::Element& a,
                       const ResourceRequestBody::Element& b) {
  if (a.type() != b.type())
    return false;
  if (a.type() == ResourceRequestBody::TYPE_BYTES)
    return a.bytes_length() == b.bytes_length() &&
           memcmp(a.bytes(), b.bytes(), b.bytes_length()) == 0;
  if (a.type() == ResourceRequestBody::TYPE_FILE) {
    return a.file_path() == b.file_path() &&
           a.file_range_offset() == b.file_range_offset() &&
           a.file_range_length() == b.file_range_length() &&
           a.expected_file_modification_time() ==
              b.expected_file_modification_time();
  }
  if (a.type() == ResourceRequestBody::TYPE_BLOB)
    return a.blob_url() == b.blob_url();
  return false;
}

inline bool operator!=(const ResourceRequestBody::Element& a,
                       const ResourceRequestBody::Element& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_RESOURCE_REQUEST_BODY_H_
