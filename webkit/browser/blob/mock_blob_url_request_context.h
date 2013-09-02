// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_MOCK_BLOB_URL_REQUEST_CONTEXT_H_
#define WEBKIT_BLOB_MOCK_BLOB_URL_REQUEST_CONTEXT_H_

#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace fileapi {
class FileSystemContext;
}

namespace webkit_blob {

class BlobStorageController;

class MockBlobURLRequestContext : public net::URLRequestContext {
 public:
  MockBlobURLRequestContext(fileapi::FileSystemContext* file_system_context);
  virtual ~MockBlobURLRequestContext();

  BlobStorageController* blob_storage_controller() const {
    return blob_storage_controller_.get();
  }

 private:
  net::URLRequestJobFactoryImpl job_factory_;
  scoped_ptr<BlobStorageController> blob_storage_controller_;

  DISALLOW_COPY_AND_ASSIGN(MockBlobURLRequestContext);
};

class ScopedTextBlob {
 public:
  ScopedTextBlob(const MockBlobURLRequestContext& request_context,
                 const GURL& blob_url,
                 const std::string& data);
  ~ScopedTextBlob();

 private:
  const GURL blob_url_;
  BlobStorageController* blob_storage_controller_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextBlob);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_MOCK_BLOB_URL_REQUEST_CONTEXT_H_
