// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace storage {
class FileSystemContext;
}  // namespace storage

namespace net {
class URLRequestContext;
}  // namespace net

namespace storage {

class BlobData;
class BlobDataHandle;
class BlobStorageContext;

class STORAGE_EXPORT BlobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // A helper to manufacture an URLRequest to retrieve the given blob.
  static scoped_ptr<net::URLRequest> CreateBlobRequest(
      scoped_ptr<BlobDataHandle> blob_data_handle,
      const net::URLRequestContext* request_context,
      net::URLRequest::Delegate* request_delegate);

  // This class ignores the request's URL and uses the value given
  // to SetRequestedBlobDataHandle instead.
  static void SetRequestedBlobDataHandle(
      net::URLRequest* request,
      scoped_ptr<BlobDataHandle> blob_data_handle);

  BlobProtocolHandler(
      BlobStorageContext* context,
      storage::FileSystemContext* file_system_context,
      const scoped_refptr<base::MessageLoopProxy>& file_loop_proxy);
  virtual ~BlobProtocolHandler();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  scoped_refptr<BlobData> LookupBlobData(
      net::URLRequest* request) const;

  base::WeakPtr<BlobStorageContext> context_;
  const scoped_refptr<storage::FileSystemContext> file_system_context_;
  const scoped_refptr<base::MessageLoopProxy> file_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_
