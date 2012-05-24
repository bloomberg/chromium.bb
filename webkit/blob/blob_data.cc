// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
        if (item.length) {
          AppendFile(
              WebStringToFilePath(item.filePath),
              static_cast<uint64>(item.offset),
              static_cast<uint64>(item.length),
              base::Time::FromDoubleT(item.expectedModificationTime));
        }
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

void BlobData::AppendData(const char* data, size_t length) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToData(data, length);
}

void BlobData::AppendFile(const FilePath& file_path, uint64 offset,
                          uint64 length,
                          const base::Time& expected_modification_time) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToFile(file_path, offset, length,
                          expected_modification_time);
}

void BlobData::AppendBlob(const GURL& blob_url, uint64 offset, uint64 length) {
  DCHECK(length > 0);
  items_.push_back(Item());
  items_.back().SetToBlob(blob_url, offset, length);
}

int64 BlobData::GetMemoryUsage() const {
  int64 memory = 0;
  for (std::vector<Item>::const_iterator iter = items_.begin();
       iter != items_.end(); ++iter) {
    if (iter->type == TYPE_DATA)
      memory += iter->data.size();
  }
  return memory;
}

}  // namespace webkit_blob
