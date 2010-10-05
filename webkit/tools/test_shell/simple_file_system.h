// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_SYSTEM_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_SYSTEM_H_

#include <vector>
#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "webkit/fileapi/file_system_operation.h"

class SimpleFileSystem : public WebKit::WebFileSystem {
 public:
  SimpleFileSystem() {}
  virtual ~SimpleFileSystem();

  void RemoveCompletedOperation(int request_id);

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
  virtual void createFile(const WebKit::WebString& path,
                          bool exclusive,
                          WebKit::WebFileSystemCallbacks* callbacks);
  virtual void createDirectory(const WebKit::WebString& path,
                               bool exclusive,
                               WebKit::WebFileSystemCallbacks* callbacks);
  virtual void fileExists(const WebKit::WebString& path,
                          WebKit::WebFileSystemCallbacks* callbacks);
  virtual void directoryExists(const WebKit::WebString& path,
                               WebKit::WebFileSystemCallbacks* callbacks);
  virtual void readDirectory(const WebKit::WebString& path,
                             WebKit::WebFileSystemCallbacks* callbacks);

 private:
  // Helpers.
  fileapi::FileSystemOperation* GetNewOperation(
      WebKit::WebFileSystemCallbacks* callbacks);

  // Keeps ongoing file system operations.
  typedef IDMap<fileapi::FileSystemOperation, IDMapOwnPointer> OperationsMap;
  OperationsMap operations_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFileSystem);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_FILE_SYSTEM_H_
