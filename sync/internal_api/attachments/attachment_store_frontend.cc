// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_store_frontend.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/attachment_store_backend.h"

namespace syncer {

namespace {

// NoOp is needed to bind base::Passed(backend) in AttachmentStoreFrontend dtor.
// It doesn't need to do anything.
void NoOp(scoped_ptr<AttachmentStoreBackend> backend) {
}

}  // namespace

AttachmentStoreFrontend::AttachmentStoreFrontend(
    scoped_ptr<AttachmentStoreBackend> backend,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : backend_(backend.Pass()), backend_task_runner_(backend_task_runner) {
  DCHECK(backend_);
  DCHECK(backend_task_runner_.get());
}

AttachmentStoreFrontend::~AttachmentStoreFrontend() {
  DCHECK(backend_);
  // To delete backend post task that doesn't do anything, but binds backend
  // through base::Passed. This way backend will be deleted regardless whether
  // task runs or not.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&NoOp, base::Passed(&backend_)));
}

void AttachmentStoreFrontend::Init(
    const AttachmentStore::InitCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::Init,
                            base::Unretained(backend_.get()), callback));
}

void AttachmentStoreFrontend::Read(
    const AttachmentIdList& ids,
    const AttachmentStore::ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::Read,
                            base::Unretained(backend_.get()), ids, callback));
}

void AttachmentStoreFrontend::Write(
    AttachmentStore::AttachmentReferrer referrer,
    const AttachmentList& attachments,
    const AttachmentStore::WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::Write,
                            base::Unretained(backend_.get()), referrer,
                            attachments, callback));
}

void AttachmentStoreFrontend::Drop(
    AttachmentStore::AttachmentReferrer referrer,
    const AttachmentIdList& ids,
    const AttachmentStore::DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentStoreBackend::Drop,
                 base::Unretained(backend_.get()), referrer, ids, callback));
}

void AttachmentStoreFrontend::ReadMetadata(
    const AttachmentIdList& ids,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::ReadMetadata,
                            base::Unretained(backend_.get()), ids, callback));
}

void AttachmentStoreFrontend::ReadAllMetadata(
    AttachmentStore::AttachmentReferrer referrer,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentStoreBackend::ReadAllMetadata,
                 base::Unretained(backend_.get()), referrer, callback));
}

}  // namespace syncer
