// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_VIEW_BLOB_INTERNALS_JOB_H_
#define WEBKIT_BLOB_VIEW_BLOB_INTERNALS_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_simple_job.h"
#include "webkit/storage/webkit_storage_export.h"

namespace net {
class URLRequest;
}  // namespace net

namespace webkit_blob {

class BlobData;
class BlobStorageController;

// A job subclass that implements a protocol to inspect the internal
// state of blob registry.
class WEBKIT_STORAGE_EXPORT ViewBlobInternalsJob
    : public net::URLRequestSimpleJob {
 public:
  ViewBlobInternalsJob(net::URLRequest* request,
                       net::NetworkDelegate* network_delegate,
                       BlobStorageController* blob_storage_controller);

  virtual void Start() OVERRIDE;
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE;
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE;
  virtual void Kill() OVERRIDE;

 private:
  virtual ~ViewBlobInternalsJob();

  void DoWorkAsync();
  void GenerateHTML(std::string* out) const;
  static void GenerateHTMLForBlobData(const BlobData& blob_data,
                                      std::string* out);

  BlobStorageController* blob_storage_controller_;
  base::WeakPtrFactory<ViewBlobInternalsJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewBlobInternalsJob);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_VIEW_BLOB_INTERNALS_JOB_H_
