// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_handle.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {

BlobDataHandle::BlobDataHandleShared::BlobDataHandleShared(
    const std::string& uuid,
    BlobStorageContext* context,
    base::SequencedTaskRunner* task_runner)
    : uuid_(uuid), context_(context->AsWeakPtr()) {
  context_->IncrementBlobRefCount(uuid);
}

scoped_ptr<BlobDataSnapshot>
BlobDataHandle::BlobDataHandleShared::CreateSnapshot() const {
  return context_->CreateSnapshot(uuid_).Pass();
}

const std::string& BlobDataHandle::BlobDataHandleShared::uuid() const {
  return uuid_;
}

BlobDataHandle::BlobDataHandleShared::~BlobDataHandleShared() {
  if (context_.get())
    context_->DecrementBlobRefCount(uuid_);
}

BlobDataHandle::BlobDataHandle(const std::string& uuid,
                               BlobStorageContext* context,
                               base::SequencedTaskRunner* task_runner)
    : io_task_runner_(task_runner),
      shared_(new BlobDataHandleShared(uuid, context, task_runner)) {
  DCHECK(io_task_runner_.get());
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
}

BlobDataHandle::BlobDataHandle(const BlobDataHandle& other) {
  io_task_runner_ = other.io_task_runner_;
  shared_ = other.shared_;
}

BlobDataHandle::~BlobDataHandle() {
  BlobDataHandleShared* raw = shared_.get();
  raw->AddRef();
  shared_ = nullptr;
  io_task_runner_->ReleaseSoon(FROM_HERE, raw);
}

scoped_ptr<BlobDataSnapshot> BlobDataHandle::CreateSnapshot() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  return shared_->CreateSnapshot().Pass();
}

const std::string& BlobDataHandle::uuid() const {
  return shared_->uuid();
}

}  // namespace storage
