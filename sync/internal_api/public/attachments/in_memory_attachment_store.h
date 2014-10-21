// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_IN_MEMORY_ATTACHMENT_STORE_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_IN_MEMORY_ATTACHMENT_STORE_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/base/sync_export.h"

namespace syncer {

// An in-memory implementation of AttachmentStore used for testing.
// InMemoryAttachmentStore is not threadsafe, it lives on backend thread and
// posts callbacks with results on |callback_task_runner|.
class SYNC_EXPORT InMemoryAttachmentStore : public AttachmentStoreBase,
                                            public base::NonThreadSafe {
 public:
  InMemoryAttachmentStore(
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner);
  ~InMemoryAttachmentStore() override;

  // AttachmentStoreBase implementation.
  void Read(const AttachmentIdList& ids, const ReadCallback& callback) override;
  void Write(const AttachmentList& attachments,
             const WriteCallback& callback) override;
  void Drop(const AttachmentIdList& ids, const DropCallback& callback) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  AttachmentMap attachments_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_IN_MEMORY_ATTACHMENT_STORE_H_
