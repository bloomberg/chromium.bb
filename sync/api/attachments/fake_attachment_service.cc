// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/fake_attachment_service.h"

#include "base/test/test_simple_task_runner.h"
#include "sync/api/attachments/fake_attachment_store.h"

namespace syncer {

FakeAttachmentService::FakeAttachmentService(
    scoped_ptr<AttachmentStore> attachment_store)
    : attachment_store_(attachment_store.Pass()) {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_store_);
}

FakeAttachmentService::~FakeAttachmentService() {
  DCHECK(CalledOnValidThread());
}

// Static.
scoped_ptr<syncer::AttachmentService> FakeAttachmentService::CreateForTest() {
  scoped_ptr<syncer::AttachmentStore> attachment_store(
      new syncer::FakeAttachmentStore(scoped_refptr<base::SequencedTaskRunner>(
          new base::TestSimpleTaskRunner)));
  scoped_ptr<syncer::AttachmentService> attachment_service(
      new syncer::FakeAttachmentService(attachment_store.Pass()));
  return attachment_service.Pass();
}

void FakeAttachmentService::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): Fire off a bunch of AttachmentStore operations.  Invoke
  // callback after all have completed (bug 356351).
}

void FakeAttachmentService::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): Fire off a bunch of AttachmentStore operations.  Invoke
  // callback after all have completed (bug 356351).
}

void FakeAttachmentService::OnSyncDataAdd(const SyncData& sync_data) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): Ensure the linked attachments get persisted in local
  // storage and schedule them for upload to the server (bug 356351).
}

void FakeAttachmentService::OnSyncDataDelete(const SyncData& sync_data) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): One or more of sync_data's attachments may no longer be
  // referenced anywhere. We should probably delete them at this point (bug
  // 356351).
}

void FakeAttachmentService::OnSyncDataUpdate(
    const AttachmentIdList& old_attachment_ids,
    const SyncData& updated_sync_data) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): At this point we need to ensure we write all new
  // attachments referenced by updated_sync_data to local storage and schedule
  // them up upload to the server. We also need to remove any no unreferenced
  // attachments from local storage (bug 356351).
}

}  // namespace syncer
