// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_url_request_job_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/browser/blob/blob_storage_controller.h"
#include "webkit/browser/blob/blob_url_request_job.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace webkit_blob {

BlobProtocolHandler::BlobProtocolHandler(
    webkit_blob::BlobStorageController* blob_storage_controller,
    fileapi::FileSystemContext* file_system_context,
    base::MessageLoopProxy* loop_proxy)
    : blob_storage_controller_(blob_storage_controller),
      file_system_context_(file_system_context),
      file_loop_proxy_(loop_proxy) {
  DCHECK(blob_storage_controller_);
  DCHECK(file_system_context_.get());
  DCHECK(file_loop_proxy_.get());
}

BlobProtocolHandler::~BlobProtocolHandler() {}

net::URLRequestJob* BlobProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  scoped_refptr<webkit_blob::BlobData> data = LookupBlobData(request);
  if (!data.get()) {
    // This request is not coming through resource dispatcher host.
    data = blob_storage_controller_->GetBlobDataFromUrl(request->url());
  }
  return new webkit_blob::BlobURLRequestJob(request,
                                            network_delegate,
                                            data.get(),
                                            file_system_context_.get(),
                                            file_loop_proxy_.get());
}

scoped_refptr<webkit_blob::BlobData>
BlobProtocolHandler::LookupBlobData(net::URLRequest* request) const {
  return NULL;
}

}  // namespace webkit_blob
