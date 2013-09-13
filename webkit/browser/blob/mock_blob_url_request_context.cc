// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/mock_blob_url_request_context.h"

#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/blob/blob_url_request_job.h"
#include "webkit/browser/blob/blob_url_request_job_factory.h"
#include "webkit/common/blob/blob_data.h"


namespace webkit_blob {

MockBlobURLRequestContext::MockBlobURLRequestContext(
    fileapi::FileSystemContext* file_system_context)
    : blob_storage_context_(new BlobStorageContext) {
  // Job factory owns the protocol handler.
  job_factory_.SetProtocolHandler(
      "blob", new BlobProtocolHandler(blob_storage_context_.get(),
                                      file_system_context,
                                      base::MessageLoopProxy::current()));
  set_job_factory(&job_factory_);
}

MockBlobURLRequestContext::~MockBlobURLRequestContext() {
}

ScopedTextBlob::ScopedTextBlob(
    const MockBlobURLRequestContext& request_context,
    const std::string& blob_id,
    const std::string& data)
    : blob_id_(blob_id),
      context_(request_context.blob_storage_context()) {
  DCHECK(context_);
  scoped_refptr<BlobData> blob_data(new BlobData(blob_id_));
  if (!data.empty())
    blob_data->AppendData(data);
  handle_ = context_->AddFinishedBlob(blob_data);
}

ScopedTextBlob::~ScopedTextBlob() {
}

scoped_ptr<BlobDataHandle> ScopedTextBlob::GetBlobDataHandle() {
  return context_->GetBlobDataFromUUID(blob_id_);
}

}  // namespace webkit_blob
