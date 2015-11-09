// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
#define STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/storage_browser_export.h"

namespace storage {

class FileSystemContext;
struct DirectoryEntry;

// A request job that handles reading filesystem: URLs for directories.
class STORAGE_EXPORT FileSystemDirURLRequestJob : public net::URLRequestJob {
 public:
  FileSystemDirURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& storage_domain,
      FileSystemContext* file_system_context);

  // URLRequestJob methods:
  void Start() override;
  void Kill() override;
  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override;
  bool GetCharset(std::string* charset) override;

  // FilterContext methods (via URLRequestJob):
  bool GetMimeType(std::string* mime_type) const override;
  // TODO(adamk): Implement GetResponseInfo and GetResponseCode to simulate
  // an HTTP response.

 private:
  class CallbackDispatcher;

  ~FileSystemDirURLRequestJob() override;

  void StartAsync();
  void DidAttemptAutoMount(base::File::Error result);
  void DidReadDirectory(base::File::Error result,
                        const std::vector<DirectoryEntry>& entries,
                        bool has_more);

  std::string data_;
  FileSystemURL url_;
  const std::string storage_domain_;
  FileSystemContext* file_system_context_;
  base::WeakPtrFactory<FileSystemDirURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDirURLRequestJob);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
