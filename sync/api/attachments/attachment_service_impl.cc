// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_service_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "sync/api/attachments/attachment.h"
#include "sync/internal_api/public/attachments/fake_attachment_store.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"

namespace syncer {

AttachmentServiceImpl::AttachmentServiceImpl(
    scoped_ptr<AttachmentStore> attachment_store,
    scoped_ptr<AttachmentUploader> attachment_uploader,
    Delegate* delegate)
    : attachment_store_(attachment_store.Pass()),
      attachment_uploader_(attachment_uploader.Pass()),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_store_);
  DCHECK(attachment_uploader_);
}

AttachmentServiceImpl::~AttachmentServiceImpl() {
  DCHECK(CalledOnValidThread());
}

// Static.
scoped_ptr<syncer::AttachmentService> AttachmentServiceImpl::CreateForTest() {
  scoped_ptr<syncer::AttachmentStore> attachment_store(
      new syncer::FakeAttachmentStore(base::MessageLoopProxy::current()));
  scoped_ptr<AttachmentUploader> attachment_uploader(
      new FakeAttachmentUploader);
  scoped_ptr<syncer::AttachmentService> attachment_service(
      new syncer::AttachmentServiceImpl(
          attachment_store.Pass(), attachment_uploader.Pass(), NULL));
  return attachment_service.Pass();
}

void AttachmentServiceImpl::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  attachment_store_->Read(attachment_ids,
                          base::Bind(&AttachmentServiceImpl::ReadDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback));
}

void AttachmentServiceImpl::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  attachment_store_->Drop(attachment_ids,
                          base::Bind(&AttachmentServiceImpl::DropDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback));
}

void AttachmentServiceImpl::StoreAttachments(const AttachmentList& attachments,
                                             const StoreCallback& callback) {
  DCHECK(CalledOnValidThread());
  attachment_store_->Write(attachments,
                           base::Bind(&AttachmentServiceImpl::WriteDone,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      callback));
  for (AttachmentList::const_iterator iter = attachments.begin();
       iter != attachments.end();
       ++iter) {
    attachment_uploader_->UploadAttachment(
        *iter,
        base::Bind(&AttachmentServiceImpl::UploadDone,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void AttachmentServiceImpl::OnSyncDataDelete(const SyncData& sync_data) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): One or more of sync_data's attachments may no longer be
  // referenced anywhere. We should probably delete them at this point (bug
  // 356351).
}

void AttachmentServiceImpl::OnSyncDataUpdate(
    const AttachmentIdList& old_attachment_ids,
    const SyncData& updated_sync_data) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): At this point we need to ensure we write all new
  // attachments referenced by updated_sync_data to local storage and schedule
  // them up upload to the server. We also need to remove any no unreferenced
  // attachments from local storage (bug 356351).
}

void AttachmentServiceImpl::ReadDone(const GetOrDownloadCallback& callback,
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

void AttachmentServiceImpl::DropDone(const DropCallback& callback,
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

void AttachmentServiceImpl::WriteDone(const StoreCallback& callback,
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

void AttachmentServiceImpl::UploadDone(
    const AttachmentUploader::UploadResult& result,
    const AttachmentId& attachment_id) {
  // TODO(pavely): crbug/372622: Deal with UploadAttachment failures.
  if (result != AttachmentUploader::UPLOAD_SUCCESS)
    return;
  if (delegate_) {
    delegate_->OnAttachmentUploaded(attachment_id);
  }
}

}  // namespace syncer
