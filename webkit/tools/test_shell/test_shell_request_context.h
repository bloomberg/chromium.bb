// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_REQUEST_CONTEXT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_REQUEST_CONTEXT_H_

#include "base/threading/thread.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"

class FilePath;

namespace fileapi {
class FileSystemContext;
}

namespace webkit_blob {
class BlobStorageController;
}

// A basic net::URLRequestContext that only provides an in-memory cookie store.
class TestShellRequestContext : public net::URLRequestContext {
 public:
  // Use an in-memory cache
  TestShellRequestContext();

  // Use an on-disk cache at the specified location.  Optionally, use the cache
  // in playback or record mode.
  TestShellRequestContext(const FilePath& cache_path,
                          net::HttpCache::Mode cache_mode,
                          bool no_proxy);

  virtual ~TestShellRequestContext();

  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE;

  webkit_blob::BlobStorageController* blob_storage_controller() const {
    return blob_storage_controller_.get();
  }

  fileapi::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

 private:
  void Init(const FilePath& cache_path, net::HttpCache::Mode cache_mode,
            bool no_proxy);

  net::URLRequestContextStorage storage_;
  scoped_ptr<webkit_blob::BlobStorageController> blob_storage_controller_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_REQUEST_CONTEXT_H_
