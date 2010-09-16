// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/simple_file_system.h"

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemEntry.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileSystemCallbacks;
using WebKit::WebString;

TestShellFileSystemOperation::TestShellFileSystemOperation(
    fileapi::FileSystemOperationClient* client,
    scoped_refptr<base::MessageLoopProxy> proxy,
    WebFileSystemCallbacks* callbacks)
    : fileapi::FileSystemOperation(client, proxy), callbacks_(callbacks) { }

SimpleFileSystem::~SimpleFileSystem() {
  // Drop all the operations.
  for (OperationsMap::const_iterator iter(&operations_);
       !iter.IsAtEnd(); iter.Advance()) {
    operations_.Remove(iter.GetCurrentKey());
  }
}

void SimpleFileSystem::move(
    const WebString& src_path,
    const WebString& dest_path, WebFileSystemCallbacks* callbacks) {
  FilePath dest_filepath(webkit_glue::WebStringToFilePath(dest_path));
  FilePath src_filepath(webkit_glue::WebStringToFilePath(src_path));

  GetNewOperation(callbacks)->Move(src_filepath, dest_filepath);
}

void SimpleFileSystem::copy(
    const WebString& src_path, const WebString& dest_path,
    WebFileSystemCallbacks* callbacks) {
  FilePath dest_filepath(webkit_glue::WebStringToFilePath(dest_path));
  FilePath src_filepath(webkit_glue::WebStringToFilePath(src_path));

  GetNewOperation(callbacks)->Copy(src_filepath, dest_filepath);
}

void SimpleFileSystem::remove(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->Remove(filepath);
}

void SimpleFileSystem::getMetadata(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->GetMetadata(filepath);
}

void SimpleFileSystem::createFile(
    const WebString& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->CreateFile(filepath, exclusive);
}

void SimpleFileSystem::createDirectory(
    const WebString& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->CreateDirectory(filepath, exclusive);
}

void SimpleFileSystem::fileExists(
  const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->FileExists(filepath);
}

void SimpleFileSystem::directoryExists(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->DirectoryExists(filepath);
}

void SimpleFileSystem::readDirectory(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->ReadDirectory(filepath);
}

void SimpleFileSystem::DidFail(WebKit::WebFileError status,
    fileapi::FileSystemOperation* operation) {
  WebFileSystemCallbacks* callbacks =
      static_cast<TestShellFileSystemOperation*>(operation)->callbacks();
  callbacks->didFail(status);
  RemoveOperation(operation);
}

void SimpleFileSystem::DidSucceed(fileapi::FileSystemOperation* operation) {
  WebFileSystemCallbacks* callbacks =
      static_cast<TestShellFileSystemOperation*>(operation)->callbacks();
  callbacks->didSucceed();
  RemoveOperation(operation);
}

void SimpleFileSystem::DidReadMetadata(
    const base::PlatformFileInfo& info,
    fileapi::FileSystemOperation* operation) {
  WebFileSystemCallbacks* callbacks =
      static_cast<TestShellFileSystemOperation*>(operation)->callbacks();
  WebKit::WebFileInfo web_file_info;
  web_file_info.modificationTime = info.last_modified.ToDoubleT();
  callbacks->didReadMetadata(web_file_info);
  RemoveOperation(operation);
}

void SimpleFileSystem::DidReadDirectory(
    const std::vector<base::file_util_proxy::Entry>& entries,
    bool has_more, fileapi::FileSystemOperation* operation) {
  WebFileSystemCallbacks* callbacks =
      static_cast<TestShellFileSystemOperation*>(operation)->callbacks();
  std::vector<WebKit::WebFileSystemEntry> web_entries_vector;
  for (std::vector<base::file_util_proxy::Entry>::const_iterator it =
       entries.begin(); it != entries.end(); ++it) {
    WebKit::WebFileSystemEntry entry;
    entry.name = webkit_glue::FilePathStringToWebString(it->name);
    entry.isDirectory = it->is_directory;
    web_entries_vector.push_back(entry);
  }
  WebKit::WebVector<WebKit::WebFileSystemEntry> web_entries =
      web_entries_vector;
  callbacks->didReadDirectory(web_entries, false);
  RemoveOperation(operation);
}

TestShellFileSystemOperation* SimpleFileSystem::GetNewOperation(
    WebFileSystemCallbacks* callbacks) {
  scoped_ptr<TestShellFileSystemOperation> operation(
      new TestShellFileSystemOperation(
          this,
          base::MessageLoopProxy::CreateForCurrentThread(), callbacks));
  int32 request_id = operations_.Add(operation.get());
  operation->set_request_id(request_id);
  return operation.release();
}

void SimpleFileSystem::RemoveOperation(
    fileapi::FileSystemOperation* operation) {
  int request_id = static_cast<TestShellFileSystemOperation*>(
      operation)->request_id();
  operations_.Remove(request_id);
}
