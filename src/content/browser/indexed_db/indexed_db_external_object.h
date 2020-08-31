// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

class CONTENT_EXPORT IndexedDBExternalObject {
 public:
  // Used for files with unknown size.
  const static int64_t kUnknownSize = -1;

  // Partially converts a list of |objects| to their mojo representation. The
  // mojo representation won't be complete until later
  // IndexedDBDispatcherHost::CreateAllExternalObjects is also called with the
  // same parameters.
  static void ConvertToMojo(
      const std::vector<IndexedDBExternalObject>& objects,
      std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects);

  IndexedDBExternalObject();
  // These two are used for Blobs.
  IndexedDBExternalObject(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
                          const std::string& uuid,
                          const base::string16& type,
                          int64_t size);
  IndexedDBExternalObject(const base::string16& type,
                          int64_t size,
                          int64_t blob_number);
  // These two are used for Files.
  // The |last_modified| time here is stored in two places - first in the
  // leveldb database, and second as the last_modified time of the file written
  // to disk. If these don't match, then something modified the file on disk and
  // it should be considered corrupt.
  IndexedDBExternalObject(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
                          const std::string& uuid,
                          const base::string16& file_name,
                          const base::string16& type,
                          const base::Time& last_modified,
                          const int64_t size);
  IndexedDBExternalObject(int64_t blob_number,
                          const base::string16& type,
                          const base::string16& file_name,
                          const base::Time& last_modified,
                          const int64_t size);
  // These are for Native File System handles.
  IndexedDBExternalObject(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken>
          token_remote);
  IndexedDBExternalObject(std::vector<uint8_t> native_file_system_token);

  IndexedDBExternalObject(const IndexedDBExternalObject& other);
  ~IndexedDBExternalObject();
  IndexedDBExternalObject& operator=(const IndexedDBExternalObject& other);

  // These values are serialized to disk.
  enum class ObjectType : uint8_t {
    kBlob = 0,
    kFile = 1,
    kNativeFileSystemHandle = 2,
    kMaxValue = kNativeFileSystemHandle
  };
  ObjectType object_type() const { return object_type_; }
  bool is_remote_valid() const { return blob_remote_.is_bound(); }
  const std::string& uuid() const {
    DCHECK(is_remote_valid());
    return uuid_;
  }
  void Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) const;
  mojo::SharedRemote<blink::mojom::Blob> remote() const { return blob_remote_; }
  const base::string16& type() const { return type_; }
  int64_t size() const { return size_; }
  const base::string16& file_name() const { return file_name_; }
  const base::FilePath indexed_db_file_path() const {
    return indexed_db_file_path_;
  }
  int64_t blob_number() const { return blob_number_; }
  const base::Time& last_modified() const { return last_modified_; }
  const std::vector<uint8_t> native_file_system_token() const {
    return native_file_system_token_;
  }
  bool is_native_file_system_remote_valid() const {
    return token_remote_.is_bound();
  }
  blink::mojom::NativeFileSystemTransferToken* native_file_system_token_remote()
      const {
    return token_remote_.get();
  }
  const base::RepeatingClosure& mark_used_callback() const {
    return mark_used_callback_;
  }
  const base::RepeatingClosure& release_callback() const {
    return release_callback_;
  }

  void set_size(int64_t size);
  void set_indexed_db_file_path(const base::FilePath& file_path);
  void set_last_modified(const base::Time& time);
  void set_native_file_system_token(std::vector<uint8_t> token);
  void set_blob_number(int64_t blob_number);
  void set_mark_used_callback(base::RepeatingClosure mark_used_callback);
  void set_release_callback(base::RepeatingClosure release_callback);

 private:
  ObjectType object_type_;

  // Always for Blob; sometimes for File.
  mojo::SharedRemote<blink::mojom::Blob> blob_remote_;
  // If blob_remote_ is true, this is the blob's uuid.
  std::string uuid_;
  // Mime type.
  base::string16 type_;
  // This is the path of the file that was copied into the IndexedDB system.
  // Only populated when reading from the database.
  base::FilePath indexed_db_file_path_;
  // -1 if unknown for File.
  int64_t size_ = kUnknownSize;
  // Only for File.
  base::string16 file_name_;
  // Only for File; valid only if size is.
  base::Time last_modified_;

  // Only for Native File System handle.
  mojo::SharedRemote<blink::mojom::NativeFileSystemTransferToken> token_remote_;
  std::vector<uint8_t> native_file_system_token_;

  // Valid only when this comes out of the database. Only for Blob and File.
  int64_t blob_number_ = DatabaseMetaDataKey::kInvalidBlobNumber;
  base::RepeatingClosure mark_used_callback_;
  base::RepeatingClosure release_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXTERNAL_OBJECT_H_
