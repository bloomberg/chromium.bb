// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/mock_blob_url_request_context.h"

#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"

namespace webkit_blob {

namespace {

class MockBlobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit MockBlobProtocolHandler(
      BlobStorageController* blob_storage_controller,
      fileapi::FileSystemContext* file_system_context)
      : blob_storage_controller_(blob_storage_controller),
        file_system_context_(file_system_context) {}

  virtual ~MockBlobProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new BlobURLRequestJob(
        request,
        network_delegate,
        blob_storage_controller_->GetBlobDataFromUrl(request->url()),
        file_system_context_,
        base::MessageLoopProxy::current());
  }

 private:
  webkit_blob::BlobStorageController* const blob_storage_controller_;
  fileapi::FileSystemContext* const file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(MockBlobProtocolHandler);
};

}  // namespace

MockBlobURLRequestContext::MockBlobURLRequestContext(
    fileapi::FileSystemContext* file_system_context)
    : blob_storage_controller_(new BlobStorageController) {
  // Job factory owns the protocol handler.
  job_factory_.SetProtocolHandler(
      "blob", new MockBlobProtocolHandler(blob_storage_controller_.get(),
                                          file_system_context));
  set_job_factory(&job_factory_);
}

MockBlobURLRequestContext::~MockBlobURLRequestContext() {}

ScopedTextBlob::ScopedTextBlob(
    const MockBlobURLRequestContext& request_context,
    const GURL& blob_url,
    const std::string& data)
    : blob_url_(blob_url),
      blob_storage_controller_(request_context.blob_storage_controller()) {
  DCHECK(blob_storage_controller_);
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendData(data);
  blob_storage_controller_->AddFinishedBlob(blob_url_, blob_data);
}

ScopedTextBlob::~ScopedTextBlob() {
  blob_storage_controller_->RemoveBlob(blob_url_);
}

}  // namespace webkit_blob
