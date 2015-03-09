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
#include "sync/api/attachments/attachment_store_backend.h"
#include "sync/base/sync_export.h"

namespace attachment_store_pb {
class RecordMetadata;
}  // namespace attachment_store_pb

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace leveldb {
class DB;
}  // namespace leveldb

namespace syncer {

// On-disk implementation of AttachmentStore. Stores attachments in leveldb
// database in |path| directory.
class SYNC_EXPORT OnDiskAttachmentStore : public AttachmentStoreBackend,
                                          public base::NonThreadSafe {
 public:
  // Constructs attachment store.
  OnDiskAttachmentStore(
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
      const base::FilePath& path);
  ~OnDiskAttachmentStore() override;

  // AttachmentStoreBackend implementation.
  void Init(const AttachmentStore::InitCallback& callback) override;
  void Read(const AttachmentIdList& ids,
            const AttachmentStore::ReadCallback& callback) override;
  void Write(AttachmentStore::AttachmentReferrer referrer,
             const AttachmentList& attachments,
             const AttachmentStore::WriteCallback& callback) override;
  void Drop(AttachmentStore::AttachmentReferrer referrer,
            const AttachmentIdList& ids,
            const AttachmentStore::DropCallback& callback) override;
  void ReadMetadata(
      const AttachmentIdList& ids,
      const AttachmentStore::ReadMetadataCallback& callback) override;
  void ReadAllMetadata(
      AttachmentStore::AttachmentReferrer referrer,
      const AttachmentStore::ReadMetadataCallback& callback) override;

 private:
  friend class OnDiskAttachmentStoreSpecificTest;

  // Opens leveldb database at |path|, creating it if needed. In the future
  // upgrade code will be invoked from OpenOrCreate as well. If open fails
  // result is UNSPECIFIED_ERROR.
  AttachmentStore::Result OpenOrCreate(const base::FilePath& path);
  // Reads single attachment from store. Returns nullptr in case of errors.
  scoped_ptr<Attachment> ReadSingleAttachment(
      const AttachmentId& attachment_id);
  // Writes single attachment to store. Returns false in case of errors.
  bool WriteSingleAttachment(const Attachment& attachment);
  // Reads single store_pb::RecordMetadata from levelDB into the provided
  // buffer. Returns false in case of an error.
  bool ReadSingleRecordMetadata(
      const AttachmentId& attachment_id,
      attachment_store_pb::RecordMetadata* record_metadata);

  static std::string MakeDataKeyFromAttachmentId(
      const AttachmentId& attachment_id);
  static std::string MakeMetadataKeyFromAttachmentId(
      const AttachmentId& attachment_id);
  static AttachmentMetadata MakeAttachmentMetadata(
      const AttachmentId& attachment_id,
      const attachment_store_pb::RecordMetadata& record_metadata);

  const base::FilePath path_;
  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(OnDiskAttachmentStore);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ON_DISK_ATTACHMENT_STORE_H_
