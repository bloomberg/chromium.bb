// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_store_handle.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "sync/api/attachments/attachment.h"

namespace syncer {

namespace {

// NoOp is needed to bind base::Passed(backend) in AttachmentStoreHandle dtor.
// It doesn't need to do anything.
void NoOp(scoped_ptr<AttachmentStoreBase> backend) {
}

}  // namespace

AttachmentStoreHandle::AttachmentStoreHandle(
    scoped_ptr<AttachmentStoreBase> backend,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : backend_(backend.Pass()), backend_task_runner_(backend_task_runner) {
  DCHECK(backend_);
  DCHECK(backend_task_runner_.get());
}

AttachmentStoreHandle::~AttachmentStoreHandle() {
  DCHECK(backend_);
  // To delete backend post task that doesn't do anything, but binds backend
  // through base::Passed. This way backend will be deleted regardless whether
  // task runs or not.
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NoOp, base::Passed(&backend_)));
}

void AttachmentStoreHandle::Read(const AttachmentIdList& ids,
                                 const ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&AttachmentStoreBase::Read,
                                            base::Unretained(backend_.get()),
                                            ids,
                                            callback));
}

void AttachmentStoreHandle::Write(const AttachmentList& attachments,
                                  const WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&AttachmentStoreBase::Write,
                                            base::Unretained(backend_.get()),
                                            attachments,
                                            callback));
}

void AttachmentStoreHandle::Drop(const AttachmentIdList& ids,
                                 const DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&AttachmentStoreBase::Drop,
                                            base::Unretained(backend_.get()),
                                            ids,
                                            callback));
}

}  // namespace syncer
