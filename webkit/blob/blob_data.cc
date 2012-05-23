// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_data.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"

using WebKit::WebBlobData;
using WebKit::WebData;
using WebKit::WebString;

namespace {

// TODO(dpranke): These two functions are cloned from webkit_glue
// to avoid a dependency on that library. This should be fixed.
FilePath::StringType WebStringToFilePathString(const WebString& str) {
#if defined(OS_POSIX)
  return base::SysWideToNativeMB(UTF16ToWideHack(str));
#elif defined(OS_WIN)
  return UTF16ToWideHack(str);
#endif
}

FilePath WebStringToFilePath(const WebString& str) {
  return FilePath(WebStringToFilePathString(str));
}

}

namespace webkit_blob {

BlobData::Item::Item()
    : type(TYPE_DATA),
      data_external(NULL),
      offset(0),
      length(0) {
}

BlobData::Item::~Item() {}

BlobData::BlobData() {}

BlobData::BlobData(const WebBlobData& data) {
  size_t i = 0;
  WebBlobData::Item item;
  while (data.itemAt(i++, item)) {
    switch (item.type) {
      case WebBlobData::Item::TypeData:
        if (!item.data.isEmpty()) {
          // WebBlobData does not allow partial data.
          DCHECK(!item.offset && item.length == -1);
          AppendData(item.data);
        }
        break;
      case WebBlobData::Item::TypeFile:
        AppendFile(
            WebStringToFilePath(item.filePath),
            static_cast<uint64>(item.offset),
            static_cast<uint64>(item.length),
            base::Time::FromDoubleT(item.expectedModificationTime));
        break;
      case WebBlobData::Item::TypeBlob:
        if (item.length) {
          AppendBlob(
              item.blobURL,
              static_cast<uint64>(item.offset),
              static_cast<uint64>(item.length));
        }
        break;
      default:
        NOTREACHED();
    }
  }
  content_type_= data.contentType().utf8().data();
  content_disposition_ = data.contentDisposition().utf8().data();
}

BlobData::~BlobData() {}

}  // namespace webkit_blob
