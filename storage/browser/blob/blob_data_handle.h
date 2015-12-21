// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_HANDLE_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_HANDLE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

class BlobDataSnapshot;
class BlobReader;
class BlobStorageContext;
class FileSystemContext;

// BlobDataHandle ensures that the underlying blob (keyed by the uuid) remains
// in the BlobStorageContext's collection while this object is alive. Anything
// that needs to keep a blob alive needs to store this handle.
// When the blob data itself is needed, clients must call the CreateSnapshot()
// method on the IO thread to create a snapshot of the blob data.  This snapshot
// is not intended to be persisted, and serves to ensure that the backing
// resources remain around for the duration of reading the blob.  This snapshot
// can be read on any thread, but it must be destructed on the IO thread.
// This object has delete semantics and may be deleted on any thread.
class STORAGE_EXPORT BlobDataHandle
    : public base::SupportsUserData::Data {
 public:
  BlobDataHandle(const BlobDataHandle& other);  // May be copied on any thread.
  ~BlobDataHandle() override;                   // May be deleted on any thread.

  // A BlobReader is used to read the data from the blob.  This object is
  // intended to be transient and should not be stored for any extended period
  // of time.
  scoped_ptr<BlobReader> CreateReader(
      FileSystemContext* file_system_context,
      base::SequencedTaskRunner* file_task_runner) const;

  // May be accessed on any thread.
  const std::string& uuid() const;
  // May be accessed on any thread.
  const std::string& content_type() const;
  // May be accessed on any thread.
  const std::string& content_disposition() const;

  // This call and the destruction of the returned snapshot must be called
  // on the IO thread.
  // TODO(dmurph): Make this protected, where only the BlobReader can call it.
  scoped_ptr<BlobDataSnapshot> CreateSnapshot() const;

 private:
  // Internal class whose destructor is guarenteed to be called on the IO
  // thread.
  class BlobDataHandleShared
      : public base::RefCountedThreadSafe<BlobDataHandleShared> {
   public:
    BlobDataHandleShared(const std::string& uuid,
                         const std::string& content_type,
                         const std::string& content_disposition,
                         BlobStorageContext* context);

    scoped_ptr<BlobDataSnapshot> CreateSnapshot() const;

   private:
    friend class base::DeleteHelper<BlobDataHandleShared>;
    friend class base::RefCountedThreadSafe<BlobDataHandleShared>;
    friend class BlobDataHandle;

    virtual ~BlobDataHandleShared();

    const std::string uuid_;
    const std::string content_type_;
    const std::string content_disposition_;
    base::WeakPtr<BlobStorageContext> context_;

    DISALLOW_COPY_AND_ASSIGN(BlobDataHandleShared);
  };

  friend class BlobStorageContext;
  BlobDataHandle(const std::string& uuid,
                 const std::string& content_type,
                 const std::string& content_disposition,
                 BlobStorageContext* context,
                 base::SequencedTaskRunner* io_task_runner);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<BlobDataHandleShared> shared_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_HANDLE_H_
