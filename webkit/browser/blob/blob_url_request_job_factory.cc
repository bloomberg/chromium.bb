// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_url_request_job_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_url_request_job.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace webkit_blob {

namespace {

int kUserDataKey;  // The value is not important, the addr is a key.

BlobDataHandle* GetRequestedBlobDataHandle(net::URLRequest* request) {
  return static_cast<BlobDataHandle*>(request->GetUserData(&kUserDataKey));
}

}  // namespace

// static
net::URLRequest* BlobProtocolHandler::CreateBlobRequest(
    scoped_ptr<BlobDataHandle> blob_data_handle,
    const net::URLRequestContext* request_context,
    net::URLRequest::Delegate* request_delegate) {
  const GURL kBlobUrl("blob://see_user_data/");
  net::URLRequest* request = request_context->CreateRequest(
      kBlobUrl, request_delegate);
  SetRequestedBlobDataHandle(request, blob_data_handle.Pass());
  return request;
}

// static
void BlobProtocolHandler::SetRequestedBlobDataHandle(
    net::URLRequest* request,
    scoped_ptr<BlobDataHandle> blob_data_handle) {
  request->SetUserData(&kUserDataKey, blob_data_handle.release());
}

BlobProtocolHandler::BlobProtocolHandler(
    fileapi::FileSystemContext* file_system_context,
    base::MessageLoopProxy* loop_proxy)
    : file_system_context_(file_system_context),
      file_loop_proxy_(loop_proxy) {
}

BlobProtocolHandler::~BlobProtocolHandler() {
}

net::URLRequestJob* BlobProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new webkit_blob::BlobURLRequestJob(
      request, network_delegate, LookupBlobData(request),
      file_system_context_, file_loop_proxy_);
}

scoped_refptr<webkit_blob::BlobData>
BlobProtocolHandler::LookupBlobData(net::URLRequest* request) const {
  BlobDataHandle* blob_data_handle = GetRequestedBlobDataHandle(request);
  if (blob_data_handle)
    return blob_data_handle->data();
  return NULL;
}

}  // namespace webkit_blob
