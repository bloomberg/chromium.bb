// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webfileutilities_impl.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebString;

namespace webkit_glue {

WebFileUtilitiesImpl::WebFileUtilitiesImpl()
    : sandbox_enabled_(true) {
}

WebFileUtilitiesImpl::~WebFileUtilitiesImpl() {
}

bool WebFileUtilitiesImpl::fileExists(const WebString& path) {
  FilePath file_path = webkit_base::WebStringToFilePath(path);
  return file_util::PathExists(file_path);
}

bool WebFileUtilitiesImpl::deleteFile(const WebString& path) {
  NOTREACHED();
  return false;
}

bool WebFileUtilitiesImpl::deleteEmptyDirectory(const WebString& path) {
  NOTREACHED();
  return false;
}

bool WebFileUtilitiesImpl::getFileInfo(const WebString& path,
                                       WebKit::WebFileInfo& web_file_info) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return false;
  }
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(webkit_base::WebStringToFilePath(path),
                              &file_info))
    return false;

  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

WebString WebFileUtilitiesImpl::directoryName(const WebString& path) {
  FilePath file_path(webkit_base::WebStringToFilePath(path));
  return webkit_base::FilePathToWebString(file_path.DirName());
}

WebString WebFileUtilitiesImpl::pathByAppendingComponent(
    const WebString& webkit_path,
    const WebString& webkit_component) {
  FilePath path(webkit_base::WebStringToFilePath(webkit_path));
  FilePath component(webkit_base::WebStringToFilePath(webkit_component));
  FilePath combined_path = path.Append(component);
  return webkit_base::FilePathStringToWebString(combined_path.value());
}

bool WebFileUtilitiesImpl::makeAllDirectories(const WebString& path) {
  DCHECK(!sandbox_enabled_);
  FilePath file_path = webkit_base::WebStringToFilePath(path);
  return file_util::CreateDirectory(file_path);
}

WebString WebFileUtilitiesImpl::getAbsolutePath(const WebString& path) {
  FilePath file_path(webkit_base::WebStringToFilePath(path));
  file_util::AbsolutePath(&file_path);
  return webkit_base::FilePathStringToWebString(file_path.value());
}

bool WebFileUtilitiesImpl::isDirectory(const WebString& path) {
  FilePath file_path(webkit_base::WebStringToFilePath(path));
  return file_util::DirectoryExists(file_path);
}

WebKit::WebURL WebFileUtilitiesImpl::filePathToURL(const WebString& path) {
  return net::FilePathToFileURL(webkit_base::WebStringToFilePath(path));
}

base::PlatformFile WebFileUtilitiesImpl::openFile(const WebString& path,
                                                  int mode) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return base::kInvalidPlatformFileValue;
  }
  return base::CreatePlatformFile(
      webkit_base::WebStringToFilePath(path),
      (mode == 0) ? (base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ)
                  : (base::PLATFORM_FILE_CREATE_ALWAYS |
                     base::PLATFORM_FILE_WRITE),
      NULL, NULL);
}

void WebFileUtilitiesImpl::closeFile(base::PlatformFile& handle) {
  if (handle == base::kInvalidPlatformFileValue)
    return;
  if (base::ClosePlatformFile(handle))
    handle = base::kInvalidPlatformFileValue;
}

long long WebFileUtilitiesImpl::seekFile(base::PlatformFile handle,
                                         long long offset,
                                         int origin) {
  if (handle == base::kInvalidPlatformFileValue)
    return -1;
  return base::SeekPlatformFile(handle,
                                static_cast<base::PlatformFileWhence>(origin),
                                offset);
}

bool WebFileUtilitiesImpl::truncateFile(base::PlatformFile handle,
                                        long long offset) {
  if (handle == base::kInvalidPlatformFileValue || offset < 0)
    return false;
  return base::TruncatePlatformFile(handle, offset);
}

int WebFileUtilitiesImpl::readFromFile(base::PlatformFile handle,
                                       char* data,
                                       int length) {
  if (handle == base::kInvalidPlatformFileValue || !data || length <= 0)
    return -1;
  return base::ReadPlatformFileCurPosNoBestEffort(handle, data, length);
}

int WebFileUtilitiesImpl::writeToFile(base::PlatformFile handle,
                                      const char* data,
                                      int length) {
  if (handle == base::kInvalidPlatformFileValue || !data || length <= 0)
    return -1;
  return base::WritePlatformFileCurPosNoBestEffort(handle, data, length);
}

}  // namespace webkit_glue
