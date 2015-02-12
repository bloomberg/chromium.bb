// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_STORE_HANDLE_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_STORE_HANDLE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/threading/non_thread_safe.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/base/sync_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace sync_pb {
class AttachmentId;
}  // namespace sync_pb

namespace syncer {

class Attachment;

// AttachmentStoreHandle is helper to post AttachmentStore calls to backend on
// different thread. Backend is expected to know on which thread to post
// callbacks with results.
// AttachmentStoreHandle takes ownership of backend. Backend is deleted on
// backend thread.
// AttachmentStoreHandle is not thread safe, it should only be accessed from
// model thread.
class SYNC_EXPORT AttachmentStoreHandle : public AttachmentStore,
                                          public base::NonThreadSafe {
 public:
  AttachmentStoreHandle(
      scoped_ptr<AttachmentStoreBackend> backend,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner);

  // AttachmentStore implementation.
  void Init(const InitCallback& callback) override;
  void Read(const AttachmentIdList& id, const ReadCallback& callback) override;
  void Write(const AttachmentList& attachments,
             const WriteCallback& callback) override;
  void Drop(const AttachmentIdList& id, const DropCallback& callback) override;
  void ReadMetadata(const AttachmentIdList& ids,
                    const ReadMetadataCallback& callback) override;
  void ReadAllMetadata(const ReadMetadataCallback& callback) override;

 private:
  ~AttachmentStoreHandle() override;

  // AttachmentStoreHandle controls backend's lifetime. It is safe for
  // AttachmentStoreHandle to bind backend through base::Unretained for posts.
  // Backend is deleted on backend_task_runner, after that backend_ pointer is
  // invalid.
  scoped_ptr<AttachmentStoreBackend> backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentStoreHandle);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_STORE_HANDLE_H_
