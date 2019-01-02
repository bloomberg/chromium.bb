// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fileapi/webfilewriter_impl.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/fileapi/file_system_dispatcher.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

typedef FileSystemDispatcher::StatusCallback StatusCallback;
typedef FileSystemDispatcher::WriteCallback WriteCallback;

WebFileWriterImpl::WebFileWriterImpl(
    const GURL& path,
    blink::WebFileWriterClient* client,
    Type type,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : WebFileWriterBase(path, client),
      request_id_(0),
      type_(type),
      file_system_dispatcher_(std::make_unique<FileSystemDispatcher>(
          std::move(main_thread_task_runner))) {}

WebFileWriterImpl::~WebFileWriterImpl() {}

void WebFileWriterImpl::DoTruncate(const GURL& path, int64_t offset) {
  if (type_ == TYPE_SYNC) {
    file_system_dispatcher_->TruncateSync(
        path, offset,
        base::Bind(&WebFileWriterImpl::DidFinish, base::Unretained(this)));
  } else {
    file_system_dispatcher_->Truncate(
        path, offset, &request_id_,
        base::Bind(&WebFileWriterImpl::DidFinish, base::Unretained(this)));
  }
}

void WebFileWriterImpl::DoWrite(const GURL& path,
                                const std::string& blob_id,
                                int64_t offset) {
  if (type_ == TYPE_SYNC) {
    file_system_dispatcher_->WriteSync(
        path, blob_id, offset,
        base::Bind(&WebFileWriterImpl::DidWrite, base::Unretained(this)),
        base::Bind(&WebFileWriterImpl::DidFinish, base::Unretained(this)));
  } else {
    file_system_dispatcher_->Write(
        path, blob_id, offset, &request_id_,
        base::Bind(&WebFileWriterImpl::DidWrite, base::Unretained(this)),
        base::Bind(&WebFileWriterImpl::DidFinish, base::Unretained(this)));
  }
}

void WebFileWriterImpl::DoCancel() {
  DCHECK_EQ(type_, TYPE_ASYNC);
  file_system_dispatcher_->Cancel(
      request_id_,
      base::Bind(&WebFileWriterImpl::DidFinish, base::Unretained(this)));
}

}  // namespace content
