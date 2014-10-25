// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ON_DISK_ATTACHMENT_STORE_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ON_DISK_ATTACHMENT_STORE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/base/sync_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace leveldb {
class DB;
}  // namespace leveldb

namespace syncer {

// On-disk implementation of AttachmentStore. Stores attachments in leveldb
// database in |path| directory.
class SYNC_EXPORT OnDiskAttachmentStore : public AttachmentStoreBase,
                                          public base::NonThreadSafe {
 public:
  // Constructs attachment store.
  OnDiskAttachmentStore(
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner);
  ~OnDiskAttachmentStore() override;

  // AttachmentStoreBase implementation.
  // Load opens database, creating it if needed. In the future upgrade code will
  // be invoked from Load as well. If loading fails it posts |callback| with
  // UNSPECIFIED_ERROR.
  void Read(const AttachmentIdList& ids, const ReadCallback& callback) override;
  void Write(const AttachmentList& attachments,
             const WriteCallback& callback) override;
  void Drop(const AttachmentIdList& ids, const DropCallback& callback) override;

  // Opens leveldb database at |path|. It will create directory and database if
  // it doesn't exist.
  Result OpenOrCreate(const base::FilePath& path);

 private:
  std::string MakeDataKeyFromAttachmentId(const AttachmentId& attachment_id);

  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
  scoped_ptr<leveldb::DB> db_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ON_DISK_ATTACHMENT_STORE_H_
