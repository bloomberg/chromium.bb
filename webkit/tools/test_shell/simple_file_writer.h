// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_WRITER_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_WRITER_H_

#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "webkit/fileapi/webfilewriter_base.h"

class URLRequestContext;


// An implementation of WebFileWriter for use in test_shell and DRT.
class SimpleFileWriter : public fileapi::WebFileWriterBase,
                         public base::SupportsWeakPtr<SimpleFileWriter> {
 public:
  SimpleFileWriter(
      const WebKit::WebString& path, WebKit::WebFileWriterClient* client);
  virtual ~SimpleFileWriter();

  // Called by SimpleResourceLoaderBridge when the context is
  // created and destroyed.
  static void InitializeOnIOThread(URLRequestContext* request_context) {
    request_context_ = request_context;
  }
  static void CleanupOnIOThread() {
    request_context_ = NULL;
  }

 protected:
  // WebFileWriterBase overrides
  virtual void DoTruncate(const FilePath& path, int64 offset);
  virtual void DoWrite(const FilePath& path, const GURL& blob_url,
                       int64 offset);
  virtual void DoCancel();

 private:
  class IOThreadProxy;
  scoped_refptr<IOThreadProxy> io_thread_proxy_;
  static URLRequestContext* request_context_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_WRITER_H_
