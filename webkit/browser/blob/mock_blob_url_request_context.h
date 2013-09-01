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

class BlobDataHandle;
class BlobStorageContext;

class MockBlobURLRequestContext : public net::URLRequestContext {
 public:
  MockBlobURLRequestContext(fileapi::FileSystemContext* file_system_context);
  virtual ~MockBlobURLRequestContext();

  BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_.get();
  }

 private:
  net::URLRequestJobFactoryImpl job_factory_;
  scoped_ptr<BlobStorageContext> blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(MockBlobURLRequestContext);
};

class ScopedTextBlob {
 public:
  // Registers a blob with the given |id| that contains |data|.
  ScopedTextBlob(const MockBlobURLRequestContext& request_context,
                 const std::string& blob_id,
                 const std::string& data);
  ~ScopedTextBlob();

  // Returns a BlobDataHandle referring to the scoped blob.
  scoped_ptr<BlobDataHandle> GetBlobDataHandle();

 private:
  const std::string blob_id_;
  BlobStorageContext* context_;
  scoped_ptr<BlobDataHandle> handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextBlob);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_MOCK_BLOB_URL_REQUEST_CONTEXT_H_
