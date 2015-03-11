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
#include "sync/internal_api/public/attachments/attachment_store_frontend.h"
#include "sync/internal_api/public/attachments/in_memory_attachment_store.h"
#include "sync/internal_api/public/attachments/on_disk_attachment_store.h"

namespace syncer {

AttachmentStore::AttachmentStore(
    const scoped_refptr<AttachmentStoreFrontend>& frontend,
    AttachmentReferrer referrer)
    : frontend_(frontend), referrer_(referrer) {
}

AttachmentStore::~AttachmentStore() {
}

void AttachmentStore::Read(const AttachmentIdList& ids,
                           const ReadCallback& callback) {
  frontend_->Read(ids, callback);
}

void AttachmentStore::Write(const AttachmentList& attachments,
                            const WriteCallback& callback) {
  frontend_->Write(referrer_, attachments, callback);
}

void AttachmentStore::Drop(const AttachmentIdList& ids,
                           const DropCallback& callback) {
  frontend_->Drop(referrer_, ids, callback);
}

void AttachmentStore::ReadMetadata(const AttachmentIdList& ids,
                                   const ReadMetadataCallback& callback) {
  frontend_->ReadMetadata(ids, callback);
}

void AttachmentStore::ReadAllMetadata(const ReadMetadataCallback& callback) {
  frontend_->ReadAllMetadata(referrer_, callback);
}

scoped_ptr<AttachmentStore> AttachmentStore::CreateAttachmentStoreForSync()
    const {
  scoped_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(frontend_, SYNC));
  return attachment_store.Pass();
}

scoped_ptr<AttachmentStore> AttachmentStore::CreateInMemoryStore() {
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
  scoped_refptr<AttachmentStoreFrontend> frontend(
      new AttachmentStoreFrontend(backend.Pass(), runner));
  scoped_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(frontend, MODEL_TYPE));
  return attachment_store.Pass();
}

scoped_ptr<AttachmentStore> AttachmentStore::CreateOnDiskStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const InitCallback& callback) {
  scoped_ptr<OnDiskAttachmentStore> backend(
      new OnDiskAttachmentStore(base::ThreadTaskRunnerHandle::Get(), path));

  scoped_refptr<AttachmentStoreFrontend> frontend =
      new AttachmentStoreFrontend(backend.Pass(), backend_task_runner);
  scoped_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(frontend, MODEL_TYPE));
  frontend->Init(callback);

  return attachment_store.Pass();
}

scoped_ptr<AttachmentStore> AttachmentStore::CreateMockStoreForTest(
    scoped_ptr<AttachmentStoreBackend> backend) {
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_refptr<AttachmentStoreFrontend> attachment_store_frontend(
      new AttachmentStoreFrontend(backend.Pass(), runner));
  scoped_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(attachment_store_frontend, MODEL_TYPE));
  return attachment_store.Pass();
}

}  // namespace syncer
