// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_service_proxy.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"

namespace syncer {

namespace {

// These ProxyFooCallback functions are used to invoke a callback in a specific
// thread.

// Invokes |callback| with |result| and |attachments| in the |task_runner|
// thread.
void ProxyGetOrDownloadCallback(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const AttachmentService::GetOrDownloadCallback& callback,
    const AttachmentService::GetOrDownloadResult& result,
    scoped_ptr<AttachmentMap> attachments) {
  task_runner->PostTask(
      FROM_HERE, base::Bind(callback, result, base::Passed(&attachments)));
}

// Invokes |callback| with |result| and |attachments| in the |task_runner|
// thread.
void ProxyDropCallback(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const AttachmentService::DropCallback& callback,
    const AttachmentService::DropResult& result) {
  task_runner->PostTask(FROM_HERE, base::Bind(callback, result));
}

}  // namespace

AttachmentServiceProxy::AttachmentServiceProxy() {
}

AttachmentServiceProxy::AttachmentServiceProxy(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    const base::WeakPtr<syncer::AttachmentService>& wrapped)
    : wrapped_task_runner_(wrapped_task_runner), core_(new Core(wrapped)) {
  DCHECK(wrapped_task_runner_.get());
}

AttachmentServiceProxy::AttachmentServiceProxy(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    const scoped_refptr<Core>& core)
    : wrapped_task_runner_(wrapped_task_runner), core_(core) {
  DCHECK(wrapped_task_runner_.get());
  DCHECK(core_.get());
}

AttachmentServiceProxy::~AttachmentServiceProxy() {
}

AttachmentStore* AttachmentServiceProxy::GetStore() {
  return NULL;
}

void AttachmentServiceProxy::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(wrapped_task_runner_.get());
  GetOrDownloadCallback proxy_callback =
      base::Bind(&ProxyGetOrDownloadCallback,
                 base::ThreadTaskRunnerHandle::Get(),
                 callback);
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::GetOrDownloadAttachments,
                 core_,
                 attachment_ids,
                 proxy_callback));
}

void AttachmentServiceProxy::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const DropCallback& callback) {
  DCHECK(wrapped_task_runner_.get());
  DropCallback proxy_callback = base::Bind(
      &ProxyDropCallback, base::ThreadTaskRunnerHandle::Get(), callback);
  wrapped_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&AttachmentService::DropAttachments,
                                            core_,
                                            attachment_ids,
                                            proxy_callback));
}

void AttachmentServiceProxy::UploadAttachments(
    const AttachmentIdSet& attachment_ids) {
  DCHECK(wrapped_task_runner_.get());
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::UploadAttachments, core_, attachment_ids));
}

AttachmentServiceProxy::Core::Core(
    const base::WeakPtr<syncer::AttachmentService>& wrapped)
    : wrapped_(wrapped) {
}

AttachmentServiceProxy::Core::~Core() {
}

AttachmentStore* AttachmentServiceProxy::Core::GetStore() {
  return NULL;
}

void AttachmentServiceProxy::Core::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  if (!wrapped_) {
    return;
  }
  wrapped_->GetOrDownloadAttachments(attachment_ids, callback);
}

void AttachmentServiceProxy::Core::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const DropCallback& callback) {
  if (!wrapped_) {
    return;
  }
  wrapped_->DropAttachments(attachment_ids, callback);
}

void AttachmentServiceProxy::Core::UploadAttachments(
    const AttachmentIdSet& attachment_ids) {
  if (!wrapped_) {
    return;
  }
  wrapped_->UploadAttachments(attachment_ids);
}

}  // namespace syncer
