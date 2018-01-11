// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/interfaces/data_pipe_getter.mojom.h"
#include "url/gurl.h"

namespace network {

// Represents a base Web data element. This could be either one of
// bytes, file or blob data.
class DataElement {
 public:
  static const uint64_t kUnknownSize = std::numeric_limits<uint64_t>::max();

  enum Type {
    TYPE_UNKNOWN = -1,

    // Only used for Upload with Network Service as of now:
    TYPE_DATA_PIPE,
    TYPE_RAW_FILE,

    // Only used for Blob:
    TYPE_BYTES_DESCRIPTION,
    TYPE_DISK_CACHE_ENTRY,  // Only used by CacheStorage
    TYPE_FILE_FILESYSTEM,

    // Commonly used for Blob, and also for Upload when Network Service is
    // disabled:
    TYPE_BLOB,  // Used old IPC codepath only.
    TYPE_FILE,

    // Commonly used in every case:
    TYPE_BYTES,
  };

  DataElement();
  ~DataElement();

  DataElement(const DataElement&) = delete;
  void operator=(const DataElement&) = delete;
  DataElement(DataElement&& other);
  DataElement& operator=(DataElement&& other);

  Type type() const { return type_; }
  const char* bytes() const { return bytes_ ? bytes_ : buf_.data(); }
  const base::FilePath& path() const { return path_; }
  const base::File& file() const { return file_; }
  const GURL& filesystem_url() const { return filesystem_url_; }
  const std::string& blob_uuid() const { return blob_uuid_; }
  const mojom::DataPipeGetterPtr& data_pipe() const {
    return data_pipe_getter_;
  }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }
  const base::Time& expected_modification_time() const {
    return expected_modification_time_;
  }

  // For use with SetToAllocatedBytes. Should only be used after calling
  // SetToAllocatedBytes.
  char* mutable_bytes() { return &buf_[0]; }

  // Sets TYPE_BYTES data. This copies the given data into the element.
  void SetToBytes(const char* bytes, int bytes_len) {
    type_ = TYPE_BYTES;
    bytes_ = nullptr;
    buf_.assign(bytes, bytes + bytes_len);
    length_ = buf_.size();
  }

  // Sets TYPE_BYTES data, and clears the internal bytes buffer.
  // For use with AppendBytes.
  void SetToEmptyBytes() {
    type_ = TYPE_BYTES;
    buf_.clear();
    length_ = 0;
    bytes_ = nullptr;
  }

  // Copies and appends the given data into the element. SetToEmptyBytes or
  // SetToBytes must be called before this method.
  void AppendBytes(const char* bytes, int bytes_len) {
    DCHECK_EQ(type_, TYPE_BYTES);
    DCHECK_NE(length_, std::numeric_limits<uint64_t>::max());
    DCHECK(!bytes_);
    buf_.insert(buf_.end(), bytes, bytes + bytes_len);
    length_ = buf_.size();
  }

  void SetToBytesDescription(size_t bytes_len) {
    type_ = TYPE_BYTES_DESCRIPTION;
    bytes_ = nullptr;
    length_ = bytes_len;
  }

  // Sets TYPE_BYTES data. This does NOT copy the given data and the caller
  // should make sure the data is alive when this element is accessed.
  // You cannot use AppendBytes with this method.
  void SetToSharedBytes(const char* bytes, int bytes_len) {
    type_ = TYPE_BYTES;
    bytes_ = bytes;
    length_ = bytes_len;
  }

  // Sets TYPE_BYTES data. This allocates the space for the bytes in the
  // internal vector but does not populate it with anything.  The caller can
  // then use the bytes() method to access this buffer and populate it.
  void SetToAllocatedBytes(size_t bytes_len) {
    type_ = TYPE_BYTES;
    bytes_ = nullptr;
    buf_.resize(bytes_len);
    length_ = bytes_len;
  }

  // Sets TYPE_FILE data.
  void SetToFilePath(const base::FilePath& path) {
    SetToFilePathRange(path, 0, std::numeric_limits<uint64_t>::max(),
                       base::Time());
  }

  // Sets TYPE_BLOB data.
  void SetToBlob(const std::string& uuid) {
    SetToBlobRange(uuid, 0, std::numeric_limits<uint64_t>::max());
  }

  // Sets TYPE_FILE data with range.
  void SetToFilePathRange(const base::FilePath& path,
                          uint64_t offset,
                          uint64_t length,
                          const base::Time& expected_modification_time);

  // Sets TYPE_RAW_FILE data with range. |file| must be open for asynchronous
  // reading on Windows. It's recommended it also be opened with
  // File::FLAG_DELETE_ON_CLOSE, since there's often no way to wait on the
  // consumer to close the file.
  void SetToFileRange(base::File file,
                      const base::FilePath& path,
                      uint64_t offset,
                      uint64_t length,
                      const base::Time& expected_modification_time);

  // Sets TYPE_BLOB data with range.
  void SetToBlobRange(const std::string& blob_uuid,
                      uint64_t offset,
                      uint64_t length);

  // Sets TYPE_FILE_FILESYSTEM with range.
  void SetToFileSystemUrlRange(const GURL& filesystem_url,
                               uint64_t offset,
                               uint64_t length,
                               const base::Time& expected_modification_time);

  // Sets to TYPE_DISK_CACHE_ENTRY with range.
  void SetToDiskCacheEntryRange(uint64_t offset, uint64_t length);

  // Sets TYPE_DATA_PIPE data.
  void SetToDataPipe(mojom::DataPipeGetterPtr data_pipe_getter);

  // Takes ownership of the File, if this is of TYPE_RAW_FILE. The file is open
  // for reading (asynchronous reading on Windows).
  base::File ReleaseFile();

  // Takes ownership of the DataPipeGetter, if this is of TYPE_DATA_PIPE.
  mojom::DataPipeGetterPtr ReleaseDataPipeGetter();

 private:
  FRIEND_TEST_ALL_PREFIXES(BlobAsyncTransportStrategyTest, TestInvalidParams);
  friend void PrintTo(const DataElement& x, ::std::ostream* os);
  Type type_;
  std::vector<char> buf_;  // For TYPE_BYTES.
  const char* bytes_;      // For TYPE_BYTES.
  base::FilePath path_;    // For TYPE_FILE and TYPE_RAW_FILE.
  base::File file_;        // For TYPE_RAW_FILE.
  GURL filesystem_url_;    // For TYPE_FILE_FILESYSTEM.
  std::string blob_uuid_;  // For TYPE_BLOB.
  mojom::DataPipeGetterPtr data_pipe_getter_;  // For TYPE_DATA_PIPE.
  uint64_t offset_;
  uint64_t length_;
  base::Time expected_modification_time_;
};

bool operator==(const DataElement& a, const DataElement& b);
bool operator!=(const DataElement& a, const DataElement& b);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DATA_ELEMENT_H_
