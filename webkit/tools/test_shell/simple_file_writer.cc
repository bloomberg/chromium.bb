// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/simple_file_writer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_interface.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using fileapi::FileSystemContext;
using fileapi::FileSystemOperationInterface;
using fileapi::WebFileWriterBase;
using WebKit::WebFileWriterClient;
using WebKit::WebString;
using WebKit::WebURL;

net::URLRequestContext* SimpleFileWriter::request_context_ = NULL;

// Helper class to proxy to write and truncate calls to the IO thread,
// and to proxy the results back to the main thead. There is a one-to-one
// relationship between SimpleFileWriters and IOThreadBackends.
class SimpleFileWriter::IOThreadProxy
    : public base::RefCountedThreadSafe<SimpleFileWriter::IOThreadProxy> {
 public:
  IOThreadProxy(const base::WeakPtr<SimpleFileWriter>& simple_writer,
                FileSystemContext* file_system_context)
      : simple_writer_(simple_writer),
        operation_(NULL),
        file_system_context_(file_system_context) {
    // The IO thread needs to be running for this class to work.
    SimpleResourceLoaderBridge::EnsureIOThread();
    io_thread_ = SimpleResourceLoaderBridge::GetIoThread();
    main_thread_ = base::MessageLoopProxy::current();
  }

  void Truncate(const GURL& path, int64 offset) {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::Truncate, this, path, offset));
      return;
    }
    DCHECK(!operation_);
    operation_ = GetNewOperation(path);
    operation_->Truncate(path, offset,
                         base::Bind(&IOThreadProxy::DidFinish, this));
  }

  void Write(const GURL& path, const GURL& blob_url, int64 offset) {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::Write, this, path, blob_url, offset));
      return;
    }
    DCHECK(request_context_);
    DCHECK(!operation_);
    operation_ = GetNewOperation(path);
    operation_->Write(request_context_, path, blob_url, offset,
                      base::Bind(&IOThreadProxy::DidWrite, this));
  }

  void Cancel() {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::Cancel, this));
      return;
    }
    if (!operation_) {
      DidFailOnMainThread(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
      return;
    }
    operation_->Cancel(base::Bind(&IOThreadProxy::DidFinish, this));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadProxy>;
  virtual ~IOThreadProxy() {}

  FileSystemOperationInterface* GetNewOperation(const GURL& path) {
    return file_system_context_->CreateFileSystemOperation(path);
  }

  void DidSucceedOnMainThread() {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::DidSucceedOnMainThread, this));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidSucceed();
  }

  void DidFailOnMainThread(base::PlatformFileError error_code) {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::DidFailOnMainThread, this, error_code));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidFail(error_code);
  }

  void DidWriteOnMainThread(int64 bytes, bool complete) {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::DidWriteOnMainThread,
                     this, bytes, complete));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidWrite(bytes, complete);
  }

  void ClearOperation() {
    DCHECK(io_thread_->BelongsToCurrentThread());
    operation_ = NULL;
  }

  void DidFinish(base::PlatformFileError result) {
    if (result == base::PLATFORM_FILE_OK)
      DidSucceedOnMainThread();
    else
      DidFailOnMainThread(result);
    ClearOperation();
  }

  void DidWrite(base::PlatformFileError result, int64 bytes, bool complete) {
    if (result == base::PLATFORM_FILE_OK) {
      DidWriteOnMainThread(bytes, complete);
      if (complete)
        ClearOperation();
    } else {
      DidFailOnMainThread(result);
      ClearOperation();
    }
  }

  scoped_refptr<base::MessageLoopProxy> io_thread_;
  scoped_refptr<base::MessageLoopProxy> main_thread_;

  // Only used on the main thread.
  base::WeakPtr<SimpleFileWriter> simple_writer_;

  // Only used on the io thread.
  FileSystemOperationInterface* operation_;

  scoped_refptr<FileSystemContext> file_system_context_;
};


SimpleFileWriter::SimpleFileWriter(
    const GURL& path,
    WebFileWriterClient* client,
    FileSystemContext* file_system_context)
  : WebFileWriterBase(path, client),
    io_thread_proxy_(new IOThreadProxy(AsWeakPtr(), file_system_context)) {
}

SimpleFileWriter::~SimpleFileWriter() {
}

void SimpleFileWriter::DoTruncate(const GURL& path, int64 offset) {
  io_thread_proxy_->Truncate(path, offset);
}

void SimpleFileWriter::DoWrite(
    const GURL& path, const GURL& blob_url, int64 offset) {
  io_thread_proxy_->Write(path, blob_url, offset);
}

void SimpleFileWriter::DoCancel() {
  io_thread_proxy_->Cancel();
}
