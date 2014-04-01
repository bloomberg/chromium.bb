// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_service_proxy.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "sync/api/sync_data.h"

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
    const AttachmentMap& attachments) {
  task_runner->PostTask(FROM_HERE, base::Bind(callback, result, attachments));
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

AttachmentServiceProxy::AttachmentServiceProxy() {}

AttachmentServiceProxy::AttachmentServiceProxy(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    base::WeakPtr<syncer::AttachmentService> wrapped)
    : wrapped_task_runner_(wrapped_task_runner), wrapped_(wrapped) {
  DCHECK(wrapped_task_runner_);
}

AttachmentServiceProxy::~AttachmentServiceProxy() {}

void AttachmentServiceProxy::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(wrapped_task_runner_);
  GetOrDownloadCallback proxy_callback = base::Bind(
      &ProxyGetOrDownloadCallback, base::MessageLoopProxy::current(), callback);
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::GetOrDownloadAttachments,
                 wrapped_,
                 attachment_ids,
                 proxy_callback));
}

void AttachmentServiceProxy::DropAttachments(
    const AttachmentIdList& attachment_ids,
    const DropCallback& callback) {
  DCHECK(wrapped_task_runner_);
  DropCallback proxy_callback = base::Bind(
      &ProxyDropCallback, base::MessageLoopProxy::current(), callback);
  wrapped_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&AttachmentService::DropAttachments,
                                            wrapped_,
                                            attachment_ids,
                                            proxy_callback));
}

void AttachmentServiceProxy::OnSyncDataAdd(const SyncData& sync_data) {
  DCHECK(wrapped_task_runner_);
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::OnSyncDataAdd, wrapped_, sync_data));
}

void AttachmentServiceProxy::OnSyncDataDelete(const SyncData& sync_data) {
  DCHECK(wrapped_task_runner_);
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::OnSyncDataDelete, wrapped_, sync_data));
}

void AttachmentServiceProxy::OnSyncDataUpdate(
    const AttachmentIdList& old_attachment_ids,
    const SyncData& updated_sync_data) {
  DCHECK(wrapped_task_runner_);
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::OnSyncDataUpdate,
                 wrapped_,
                 old_attachment_ids,
                 updated_sync_data));
}

}  // namespace syncer
