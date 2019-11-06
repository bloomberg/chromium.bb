// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CRASH_SERVICE_UPLOADER_H_
#define CHROME_BROWSER_TRACING_CRASH_SERVICE_UPLOADER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/trace_uploader.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// TraceCrashServiceUploader uploads traces to the Chrome crash service.
class TraceCrashServiceUploader : public content::TraceUploader {
 public:
  explicit TraceCrashServiceUploader(
      scoped_refptr<network::SharedURLLoaderFactory>);
  ~TraceCrashServiceUploader() override;

  void SetUploadURL(const std::string& url);

  void SetMaxUploadBytes(size_t max_upload_bytes);

  // content::TraceUploader
  void DoUpload(const std::string& file_contents,
                UploadMode upload_mode,
                std::unique_ptr<const base::DictionaryValue> metadata,
                const UploadProgressCallback& progress_callback,
                UploadDoneCallback done_callback) override;

 private:
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnURLLoaderUploadProgress(uint64_t position, uint64_t size);

  void DoCompressOnBackgroundThread(
      const std::string& file_contents,
      UploadMode upload_mode,
      const std::string& upload_url,
      std::unique_ptr<const base::DictionaryValue> metadata);

  // Sets up a multipart body to be uploaded. The body is produced according
  // to RFC 2046.
  void SetupMultipart(const std::string& product,
                      const std::string& version,
                      std::unique_ptr<const base::DictionaryValue> metadata,
                      const std::string& trace_filename,
                      const std::string& trace_contents,
                      std::string* post_data);
  void AddTraceFile(const std::string& trace_filename,
                    const std::string& trace_contents,
                    std::string* post_data);
  // Compresses the input and returns whether compression was successful.
  bool Compress(std::string input,
                int max_compressed_bytes,
                char* compressed_contents,
                int* compressed_bytes);
  void CreateAndStartURLLoader(const std::string& upload_url,
                               const std::string& post_data);
  void OnUploadError(const std::string& error_message);

  UploadProgressCallback progress_callback_;
  UploadDoneCallback done_callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::string upload_url_;

  size_t max_upload_bytes_;

  DISALLOW_COPY_AND_ASSIGN(TraceCrashServiceUploader);
};

#endif  // CHROME_BROWSER_TRACING_CRASH_SERVICE_UPLOADER_H_
