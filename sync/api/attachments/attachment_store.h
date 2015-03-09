// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_metadata.h"
#include "sync/base/sync_export.h"

namespace base {
class FilePath;
class RefCountedMemory;
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

class Attachment;
class AttachmentId;

// AttachmentStore is a place to locally store and access Attachments.
//
// AttachmentStore class is an interface exposed to data type and
// AttachmentService code.
// It also contains factory methods for default attachment store
// implementations.
// Destroying this object does not necessarily cancel outstanding async
// operations. If you need cancel like semantics, use WeakPtr in the callbacks.
class SYNC_EXPORT AttachmentStore
    : public base::RefCountedThreadSafe<AttachmentStore> {
 public:
  // TODO(maniscalco): Consider udpating Read and Write methods to support
  // resumable transfers (bug 353292).

  // The result status of an attachment store operation.
  // Do not re-order or delete these entries; they are used in a UMA histogram.
  enum Result {
    SUCCESS = 0,            // No error, all completed successfully.
    UNSPECIFIED_ERROR = 1,  // An unspecified error occurred for >= 1 item.
    STORE_INITIALIZATION_FAILED = 2,  // AttachmentStore initialization failed.
    // When adding a value here, you must increment RESULT_SIZE below.
  };
  static const int RESULT_SIZE =
      10;  // Size of the Result enum; used for histograms.

  typedef base::Callback<void(const Result&)> InitCallback;
  typedef base::Callback<void(const Result&,
                              scoped_ptr<AttachmentMap>,
                              scoped_ptr<AttachmentIdList>)> ReadCallback;
  typedef base::Callback<void(const Result&)> WriteCallback;
  typedef base::Callback<void(const Result&)> DropCallback;
  typedef base::Callback<void(const Result&,
                              scoped_ptr<AttachmentMetadataList>)>
      ReadMetadataCallback;

  AttachmentStore();

  // Asynchronously initializes attachment store.
  //
  // This method should not be called by consumer of this interface. It is
  // called by factory methods in AttachmentStore class. When initialization is
  // complete |callback| is invoked with result, in case of failure result is
  // UNSPECIFIED_ERROR.
  virtual void Init(const InitCallback& callback) = 0;

  // Asynchronously reads the attachments identified by |ids|.
  //
  // |callback| will be invoked when finished. AttachmentStore will attempt to
  // read all attachments specified in ids. If any of the attachments do not
  // exist or could not be read, |callback|'s Result will be UNSPECIFIED_ERROR.
  // Callback's AttachmentMap will contain all attachments that were
  // successfully read, AttachmentIdList will contain attachment ids of
  // attachments that are unavailable in attachment store, these need to be
  // downloaded from server.
  //
  // Reads on individual attachments are treated atomically; |callback| will not
  // read only part of an attachment.
  virtual void Read(const AttachmentIdList& ids,
                    const ReadCallback& callback) = 0;

  // Asynchronously writes |attachments| to the store.
  //
  // Will not overwrite stored attachments. Attempting to overwrite an
  // attachment that already exists is not an error.
  //
  // |callback| will be invoked when finished. If any of the attachments could
  // not be written |callback|'s Result will be UNSPECIFIED_ERROR. When this
  // happens, some or none of the attachments may have been written
  // successfully.
  virtual void Write(const AttachmentList& attachments,
                     const WriteCallback& callback) = 0;

  // Asynchronously drops |attchments| from this store.
  //
  // This does not remove attachments from the server.
  //
  // |callback| will be invoked when finished. Attempting to drop an attachment
  // that does not exist is not an error. If any of the existing attachment
  // could not be dropped, |callback|'s Result will be UNSPECIFIED_ERROR. When
  // this happens, some or none of the attachments may have been dropped
  // successfully.
  virtual void Drop(const AttachmentIdList& ids,
                    const DropCallback& callback) = 0;

  // Asynchronously reads metadata for the attachments identified by |ids|.
  //
  // |callback| will be invoked when finished. AttachmentStore will attempt to
  // read metadata for all attachments specified in ids. If any of the
  // metadata entries do not exist or could not be read, |callback|'s Result
  // will be UNSPECIFIED_ERROR.
  virtual void ReadMetadata(const AttachmentIdList& ids,
                            const ReadMetadataCallback& callback) = 0;

  // Asynchronously reads metadata for all attachments in the store.
  //
  // |callback| will be invoked when finished. If any of the metadata entries
  // could not be read, |callback|'s Result will be UNSPECIFIED_ERROR.
  virtual void ReadAllMetadata(const ReadMetadataCallback& callback) = 0;

  // Creates an AttachmentStoreHandle backed by in-memory implementation of
  // attachment store. For now frontend lives on the same thread as backend.
  static scoped_refptr<AttachmentStore> CreateInMemoryStore();

  // Creates an AttachmentStoreHandle backed by on-disk implementation of
  // attachment store. Opens corresponding leveldb database located at |path|.
  // All backend operations are scheduled to |backend_task_runner|. Opening
  // attachment store is asynchronous, once it finishes |callback| will be
  // called on the thread that called CreateOnDiskStore. Calling Read/Write/Drop
  // before initialization completed is allowed.  Later if initialization fails
  // these operations will fail with STORE_INITIALIZATION_FAILED error.
  static scoped_refptr<AttachmentStore> CreateOnDiskStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
      const InitCallback& callback);

 protected:
  friend class base::RefCountedThreadSafe<AttachmentStore>;
  virtual ~AttachmentStore();
};

// Interface for AttachmentStore backends.
//
// AttachmentStoreBackend provides interface for different backends (on-disk,
// in-memory). Factory methods in AttachmentStore create corresponding backend
// and pass reference to AttachmentStoreHandle.
// All functions in AttachmentStoreBackend mirror corresponding functions in
// AttachmentStore.
// All callbacks and result codes are used directly from AttachmentStore.
// AttachmentStoreHandle only passes callbacks and results, there is no need to
// declare separate set.
class SYNC_EXPORT AttachmentStoreBackend {
 public:
  explicit AttachmentStoreBackend(
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner);
  virtual ~AttachmentStoreBackend();
  virtual void Init(const AttachmentStore::InitCallback& callback) = 0;
  virtual void Read(const AttachmentIdList& ids,
                    const AttachmentStore::ReadCallback& callback) = 0;
  virtual void Write(const AttachmentList& attachments,
                     const AttachmentStore::WriteCallback& callback) = 0;
  virtual void Drop(const AttachmentIdList& ids,
                    const AttachmentStore::DropCallback& callback) = 0;
  virtual void ReadMetadata(
      const AttachmentIdList& ids,
      const AttachmentStore::ReadMetadataCallback& callback) = 0;
  virtual void ReadAllMetadata(
      const AttachmentStore::ReadMetadataCallback& callback) = 0;

 protected:
  // Helper function to post callback on callback_task_runner.
  void PostCallback(const base::Closure& callback);

 private:
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentStoreBackend);
};

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_
