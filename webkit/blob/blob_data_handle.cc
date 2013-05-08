// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_data_handle.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_context.h"

namespace webkit_blob {

BlobDataHandle::BlobDataHandle(BlobData* blob_data, BlobStorageContext* context,
                               base::SequencedTaskRunner* task_runner)
    : blob_data_(blob_data),
      context_(context->AsWeakPtr()),
      io_task_runner_(task_runner) {
  // Ensures the uuid remains registered and the underlying data is not deleted.
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  context_->IncrementBlobRefCount(blob_data->uuid());
  blob_data_->AddRef();
}

BlobDataHandle::~BlobDataHandle() {
  if (io_task_runner_->RunsTasksOnCurrentThread()) {
    // Note: Do not test context_ or alter the blob_data_ refcount
    // on the wrong thread.
    if (context_.get())
      context_->DecrementBlobRefCount(blob_data_->uuid());
    blob_data_->Release();
    return;
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeleteHelper, context_, base::Unretained(blob_data_)));
}

BlobData* BlobDataHandle::data() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  return blob_data_;
}

// static
void BlobDataHandle::DeleteHelper(
    base::WeakPtr<BlobStorageContext> context,
    BlobData* blob_data) {
  if (context.get())
    context->DecrementBlobRefCount(blob_data->uuid());
  blob_data->Release();
}

}  // namespace webkit_blob
