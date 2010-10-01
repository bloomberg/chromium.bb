// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_data.h"

#include "base/logging.h"
#include "base/time.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBlobData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebBlobData;
using WebKit::WebData;
using WebKit::WebString;

namespace webkit_blob {

BlobData::Item::Item()
    : type_(TYPE_DATA),
      offset_(0),
      length_(0) {
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
            webkit_glue::WebStringToFilePath(item.filePath),
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
