// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webfileutilities_impl.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebString;

namespace webkit_glue {

WebFileUtilitiesImpl::WebFileUtilitiesImpl()
    : sandbox_enabled_(true) {
}

WebFileUtilitiesImpl::~WebFileUtilitiesImpl() {
}

bool WebFileUtilitiesImpl::getFileInfo(const WebString& path,
                                       WebKit::WebFileInfo& web_file_info) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return false;
  }
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(base::FilePath::FromUTF16Unsafe(path),
                              &file_info))
    return false;

  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

WebString WebFileUtilitiesImpl::directoryName(const WebString& path) {
  return base::FilePath::FromUTF16Unsafe(path).DirName().AsUTF16Unsafe();
}

WebString WebFileUtilitiesImpl::baseName(const WebString& path) {
  return base::FilePath::FromUTF16Unsafe(path).BaseName().AsUTF16Unsafe();
}

WebKit::WebURL WebFileUtilitiesImpl::filePathToURL(const WebString& path) {
  return net::FilePathToFileURL(base::FilePath::FromUTF16Unsafe(path));
}

base::PlatformFile WebFileUtilitiesImpl::openFile(const WebString& path,
                                                  int mode) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return base::kInvalidPlatformFileValue;
  }
  // mode==0 (read-only) is the only supported mode.
  // TODO(kinuko): Remove this parameter.
  DCHECK_EQ(0, mode);
  return base::CreatePlatformFile(
      base::FilePath::FromUTF16Unsafe(path),
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL, NULL);
}

void WebFileUtilitiesImpl::closeFile(base::PlatformFile& handle) {
  if (handle == base::kInvalidPlatformFileValue)
    return;
  if (base::ClosePlatformFile(handle))
    handle = base::kInvalidPlatformFileValue;
}

int WebFileUtilitiesImpl::readFromFile(base::PlatformFile handle,
                                       char* data,
                                       int length) {
  if (handle == base::kInvalidPlatformFileValue || !data || length <= 0)
    return -1;
  return base::ReadPlatformFileCurPosNoBestEffort(handle, data, length);
}

}  // namespace webkit_glue
