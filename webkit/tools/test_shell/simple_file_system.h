// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_SYSTEM_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_SYSTEM_H_

#include <vector>
#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_client.h"

class TestShellFileSystemOperation : public fileapi::FileSystemOperation {
 public:
  TestShellFileSystemOperation(
      fileapi::FileSystemOperationClient* client,
      scoped_refptr<base::MessageLoopProxy> proxy,
      WebKit::WebFileSystemCallbacks* callbacks);

  WebKit::WebFileSystemCallbacks* callbacks() { return callbacks_; }

  void set_request_id(int request_id) { request_id_ = request_id; }
  int  request_id() {  return request_id_; }

 private:
  // Not owned.
  WebKit::WebFileSystemCallbacks* callbacks_;

  // Just for IDMap of operations.
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(TestShellFileSystemOperation);
};

class SimpleFileSystem
    : public WebKit::WebFileSystem,
      public fileapi::FileSystemOperationClient {
 public:
  SimpleFileSystem() {}
  virtual ~SimpleFileSystem();

  // WebKit::WebFileSystem methods.
  virtual void move(const WebKit::WebString& src_path,
      const WebKit::WebString& dest_path,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void copy(const WebKit::WebString& src_path,
      const WebKit::WebString& dest_path,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void remove(const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void readMetadata(const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void createFile(const WebKit::WebString& path, bool exclusive,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void createDirectory(const WebKit::WebString& path, bool exclusive,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void fileExists(const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void directoryExists(const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void readDirectory(const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks* callbacks);

  // FileSystemOperationClient methods.
  virtual void DidFail(WebKit::WebFileError status,
      fileapi::FileSystemOperation* operation);
  virtual void DidSucceed(fileapi::FileSystemOperation* operation);
  virtual void DidReadMetadata(
      const base::PlatformFileInfo& info,
      fileapi::FileSystemOperation* operation);
  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool has_more, fileapi::FileSystemOperation* operation);

 private:
  // Helpers.
  TestShellFileSystemOperation* GetNewOperation(
      WebKit::WebFileSystemCallbacks* callbacks);

  void RemoveOperation(fileapi::FileSystemOperation* operation);

  // Keeps ongoing file system operations.
  typedef IDMap<TestShellFileSystemOperation, IDMapOwnPointer> OperationsMap;
  OperationsMap operations_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFileSystem);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_SYSTEM_H_
