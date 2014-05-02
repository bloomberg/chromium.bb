// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/fake_attachment_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/fake_attachment_store.h"

namespace syncer {

FakeAttachmentService::FakeAttachmentService(
    scoped_ptr<AttachmentStore> attachment_store)
    : attachment_store_(attachment_store.Pass()), weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_store_);
}

FakeAttachmentService::~FakeAttachmentService() {
  DCHECK(CalledOnValidThread());
}

// Static.
scoped_ptr<syncer::AttachmentService> FakeAttachmentService::CreateForTest() {
  scoped_ptr<syncer::AttachmentStore> attachment_store(
      new syncer::FakeAttachmentStore(base::MessageLoopProxy::current()));
  scoped_ptr<syncer::AttachmentService> attachment_service(
      new syncer::FakeAttachmentService(attachment_store.Pass()));
  return attachment_service.Pass();
}

void FakeAttachmentService::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  attachment_store_->Read(attachment_ids,
                          base::Bind(&FakeAttachmentService::ReadDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback));
}

void FakeAttachmentService::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  attachment_store_->Drop(attachment_ids,
                          base::Bind(&FakeAttachmentService::DropDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback));
}

void FakeAttachmentService::StoreAttachments(const AttachmentList& attachments,
                                             const StoreCallback& callback) {
  DCHECK(CalledOnValidThread());
  attachment_store_->Write(attachments,
                           base::Bind(&FakeAttachmentService::WriteDone,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      callback));
  // TODO(maniscalco): Ensure the linked attachments are schedule for upload to
  // the server (bug 356351).
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

void FakeAttachmentService::ReadDone(const GetOrDownloadCallback& callback,
                                     const AttachmentStore::Result& result,
                                     scoped_ptr<AttachmentMap> attachments) {
  AttachmentService::GetOrDownloadResult get_result =
      AttachmentService::GET_UNSPECIFIED_ERROR;
  if (result == AttachmentStore::SUCCESS) {
    get_result = AttachmentService::GET_SUCCESS;
  }
  // TODO(maniscalco): Deal with case where an error occurred (bug 361251).
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, get_result, base::Passed(&attachments)));
}

void FakeAttachmentService::DropDone(const DropCallback& callback,
                                     const AttachmentStore::Result& result) {
  AttachmentService::DropResult drop_result =
      AttachmentService::DROP_UNSPECIFIED_ERROR;
  if (result == AttachmentStore::SUCCESS) {
    drop_result = AttachmentService::DROP_SUCCESS;
  }
  // TODO(maniscalco): Deal with case where an error occurred (bug 361251).
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, drop_result));
}

void FakeAttachmentService::WriteDone(const StoreCallback& callback,
                                      const AttachmentStore::Result& result) {
  AttachmentService::StoreResult store_result =
      AttachmentService::STORE_UNSPECIFIED_ERROR;
  if (result == AttachmentStore::SUCCESS) {
    store_result = AttachmentService::STORE_SUCCESS;
  }
  // TODO(maniscalco): Deal with case where an error occurred (bug 361251).
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, store_result));
}

}  // namespace syncer
