// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_store.h"

#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/attachments/attachment_store_handle.h"
#include "sync/internal_api/public/attachments/in_memory_attachment_store.h"

namespace syncer {

AttachmentStoreBase::AttachmentStoreBase() {}
AttachmentStoreBase::~AttachmentStoreBase() {}

AttachmentStore::AttachmentStore() {}
AttachmentStore::~AttachmentStore() {}

scoped_refptr<AttachmentStore> AttachmentStore::CreateInMemoryStore() {
  // Both frontend and backend of attachment store will live on current thread.
  scoped_ptr<AttachmentStoreBase> backend(
      new InMemoryAttachmentStore(base::ThreadTaskRunnerHandle::Get()));
  return scoped_refptr<AttachmentStore>(new AttachmentStoreHandle(
      backend.Pass(), base::ThreadTaskRunnerHandle::Get()));
}

}  // namespace syncer
