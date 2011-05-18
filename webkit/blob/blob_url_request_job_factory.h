// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_
#define WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_job_factory.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace net {
class URLRequest;
}  // namespace net

namespace webkit_blob {

class BlobData;
class BlobStorageController;

class BlobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |controller|'s lifetime should exceed the lifetime of the ProtocolHandler.
  explicit BlobProtocolHandler(
      BlobStorageController* blob_storage_controller,
      base::MessageLoopProxy* file_loop_proxy);
  virtual ~BlobProtocolHandler();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  virtual scoped_refptr<BlobData> LookupBlobData(
      net::URLRequest* request) const;

  // No scoped_refptr because |blob_storage_controller_| is owned by the
  // ProfileIOData, which also owns this ProtocolHandler.
  BlobStorageController* const blob_storage_controller_;
  const scoped_refptr<base::MessageLoopProxy> file_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_
