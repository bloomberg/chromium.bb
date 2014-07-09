// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_service_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "sync/api/attachments/attachment.h"
#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"
#include "sync/internal_api/public/attachments/fake_attachment_store.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"

namespace syncer {

// GetOrDownloadAttachments starts multiple parallel DownloadAttachment calls.
// GetOrDownloadState tracks completion of these calls and posts callback for
// consumer once all attachments are either retrieved or reported unavailable.
class AttachmentServiceImpl::GetOrDownloadState
    : public base::RefCounted<GetOrDownloadState>,
      public base::NonThreadSafe {
 public:
  // GetOrDownloadState gets parameter from values passed to
  // AttachmentService::GetOrDownloadAttachments.
  // |attachment_ids| is a list of attachmens to retrieve.
  // |callback| will be posted on current thread when all attachments retrieved
  // or confirmed unavailable.
  GetOrDownloadState(const AttachmentIdList& attachment_ids,
                     const GetOrDownloadCallback& callback);

  // Attachment was just retrieved. Add it to retrieved attachments.
  void AddAttachment(const Attachment& attachment);

  // Both reading from local store and downloading attachment failed.
  // Add it to unavailable set.
  void AddUnavailableAttachmentId(const AttachmentId& attachment_id);

 private:
  friend class base::RefCounted<GetOrDownloadState>;
  virtual ~GetOrDownloadState();

  // If all attachment requests completed then post callback to consumer with
  // results.
  void PostResultIfAllRequestsCompleted();

  GetOrDownloadCallback callback_;

  // Requests for these attachments are still in progress.
  AttachmentIdSet in_progress_attachments_;

  AttachmentIdSet unavailable_attachments_;
  scoped_ptr<AttachmentMap> retrieved_attachments_;

  DISALLOW_COPY_AND_ASSIGN(GetOrDownloadState);
};

AttachmentServiceImpl::GetOrDownloadState::GetOrDownloadState(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback)
    : callback_(callback), retrieved_attachments_(new AttachmentMap()) {
  std::copy(
      attachment_ids.begin(),
      attachment_ids.end(),
      std::inserter(in_progress_attachments_, in_progress_attachments_.end()));
  PostResultIfAllRequestsCompleted();
}

AttachmentServiceImpl::GetOrDownloadState::~GetOrDownloadState() {
  DCHECK(CalledOnValidThread());
}

void AttachmentServiceImpl::GetOrDownloadState::AddAttachment(
    const Attachment& attachment) {
  DCHECK(CalledOnValidThread());
  DCHECK(retrieved_attachments_->find(attachment.GetId()) ==
         retrieved_attachments_->end());
  retrieved_attachments_->insert(
      std::make_pair(attachment.GetId(), attachment));
  DCHECK(in_progress_attachments_.find(attachment.GetId()) !=
         in_progress_attachments_.end());
  in_progress_attachments_.erase(attachment.GetId());
  PostResultIfAllRequestsCompleted();
}

void AttachmentServiceImpl::GetOrDownloadState::AddUnavailableAttachmentId(
    const AttachmentId& attachment_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(unavailable_attachments_.find(attachment_id) ==
         unavailable_attachments_.end());
  unavailable_attachments_.insert(attachment_id);
  DCHECK(in_progress_attachments_.find(attachment_id) !=
         in_progress_attachments_.end());
  in_progress_attachments_.erase(attachment_id);
  PostResultIfAllRequestsCompleted();
}

void
AttachmentServiceImpl::GetOrDownloadState::PostResultIfAllRequestsCompleted() {
  if (in_progress_attachments_.empty()) {
    // All requests completed. Let's notify consumer.
    GetOrDownloadResult result =
        unavailable_attachments_.empty() ? GET_SUCCESS : GET_UNSPECIFIED_ERROR;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback_, result, base::Passed(&retrieved_attachments_)));
  }
}

AttachmentServiceImpl::AttachmentServiceImpl(
    scoped_ptr<AttachmentStore> attachment_store,
    scoped_ptr<AttachmentUploader> attachment_uploader,
    scoped_ptr<AttachmentDownloader> attachment_downloader,
    Delegate* delegate)
    : attachment_store_(attachment_store.Pass()),
      attachment_uploader_(attachment_uploader.Pass()),
      attachment_downloader_(attachment_downloader.Pass()),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_store_);
}

AttachmentServiceImpl::~AttachmentServiceImpl() {
  DCHECK(CalledOnValidThread());
}

// Static.
scoped_ptr<syncer::AttachmentService> AttachmentServiceImpl::CreateForTest() {
  scoped_ptr<syncer::AttachmentStore> attachment_store(
      new syncer::FakeAttachmentStore(base::ThreadTaskRunnerHandle::Get()));
  scoped_ptr<AttachmentUploader> attachment_uploader(
      new FakeAttachmentUploader);
  scoped_ptr<AttachmentDownloader> attachment_downloader(
      new FakeAttachmentDownloader());
  scoped_ptr<syncer::AttachmentService> attachment_service(
      new syncer::AttachmentServiceImpl(attachment_store.Pass(),
                                        attachment_uploader.Pass(),
                                        attachment_downloader.Pass(),
                                        NULL));
  return attachment_service.Pass();
}

void AttachmentServiceImpl::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<GetOrDownloadState> state(
      new GetOrDownloadState(attachment_ids, callback));
  attachment_store_->Read(attachment_ids,
                          base::Bind(&AttachmentServiceImpl::ReadDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     state));
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
  if (attachment_uploader_.get()) {
    for (AttachmentList::const_iterator iter = attachments.begin();
         iter != attachments.end();
         ++iter) {
      attachment_uploader_->UploadAttachment(
          *iter,
          base::Bind(&AttachmentServiceImpl::UploadDone,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void AttachmentServiceImpl::ReadDone(
    const scoped_refptr<GetOrDownloadState>& state,
    const AttachmentStore::Result& result,
    scoped_ptr<AttachmentMap> attachments,
    scoped_ptr<AttachmentIdList> unavailable_attachment_ids) {
  // Add read attachments to result.
  for (AttachmentMap::const_iterator iter = attachments->begin();
       iter != attachments->end();
       ++iter) {
    state->AddAttachment(iter->second);
  }

  AttachmentIdList::const_iterator iter = unavailable_attachment_ids->begin();
  AttachmentIdList::const_iterator end = unavailable_attachment_ids->end();
  if (attachment_downloader_.get()) {
    // Try to download locally unavailable attachments.
    for (; iter != end; ++iter) {
      attachment_downloader_->DownloadAttachment(
          *iter,
          base::Bind(&AttachmentServiceImpl::DownloadDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     state,
                     *iter));
    }
  } else {
    // No downloader so all locally unavailable attachments are unavailable.
    for (; iter != end; ++iter) {
      state->AddUnavailableAttachmentId(*iter);
    }
  }
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

void AttachmentServiceImpl::DownloadDone(
    const scoped_refptr<GetOrDownloadState>& state,
    const AttachmentId& attachment_id,
    const AttachmentDownloader::DownloadResult& result,
    scoped_ptr<Attachment> attachment) {
  if (result == AttachmentDownloader::DOWNLOAD_SUCCESS) {
    state->AddAttachment(*attachment.get());
  } else {
    state->AddUnavailableAttachmentId(attachment_id);
  }
}

}  // namespace syncer
