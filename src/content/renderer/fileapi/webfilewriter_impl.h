// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FILEAPI_WEBFILEWRITER_IMPL_H_
#define CONTENT_RENDERER_FILEAPI_WEBFILEWRITER_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "content/renderer/fileapi/webfilewriter_base.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class FileSystemDispatcher;

// An implementation of WebFileWriter for use in chrome renderers and workers.
class WebFileWriterImpl : public WebFileWriterBase {
 public:
  enum Type {
    TYPE_SYNC,
    TYPE_ASYNC,
  };

  WebFileWriterImpl(
      const GURL& path,
      blink::WebFileWriterClient* client,
      Type type,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~WebFileWriterImpl() override;

 protected:
  // WebFileWriterBase overrides
  void DoTruncate(const GURL& path, int64_t offset) override;
  void DoWrite(const GURL& path,
               const std::string& blob_id,
               int64_t offset) override;
  void DoCancel() override;

 private:
  int request_id_;
  Type type_;
  std::unique_ptr<FileSystemDispatcher> file_system_dispatcher_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_FILEAPI_WEBFILEWRITER_IMPL_H_
