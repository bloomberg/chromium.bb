// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/attachments/attachment_store_handle.h"
#include "sync/internal_api/public/attachments/in_memory_attachment_store.h"
#include "sync/internal_api/public/attachments/on_disk_attachment_store.h"

namespace syncer {

AttachmentStore::AttachmentStore() {}
AttachmentStore::~AttachmentStore() {}

scoped_refptr<AttachmentStore> AttachmentStore::CreateInMemoryStore() {
  // Both frontend and backend of attachment store will live on current thread.
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    runner = base::ThreadTaskRunnerHandle::Get();
  } else {
    // Dummy runner for tests that don't have MessageLoop.
    base::MessageLoop loop;
    // This works because |runner| takes a ref to the proxy.
    runner = base::ThreadTaskRunnerHandle::Get();
  }
  scoped_ptr<AttachmentStoreBackend> backend(
      new InMemoryAttachmentStore(runner));
  return scoped_refptr<AttachmentStore>(
      new AttachmentStoreHandle(backend.Pass(), runner));
}

scoped_refptr<AttachmentStore> AttachmentStore::CreateOnDiskStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const InitCallback& callback) {
  scoped_ptr<OnDiskAttachmentStore> backend(
      new OnDiskAttachmentStore(base::ThreadTaskRunnerHandle::Get(), path));

  scoped_refptr<AttachmentStore> attachment_store =
      new AttachmentStoreHandle(backend.Pass(), backend_task_runner);
  attachment_store->Init(callback);

  return attachment_store;
}

AttachmentStoreBackend::AttachmentStoreBackend(
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner)
    : callback_task_runner_(callback_task_runner) {
}

AttachmentStoreBackend::~AttachmentStoreBackend() {
}

void AttachmentStoreBackend::PostCallback(const base::Closure& callback) {
  callback_task_runner_->PostTask(FROM_HERE, callback);
}

}  // namespace syncer
