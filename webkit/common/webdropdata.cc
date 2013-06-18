// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/webdropdata.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "ui/base/clipboard/clipboard.h"

using WebKit::WebData;
using WebKit::WebDragData;
using WebKit::WebString;
using WebKit::WebVector;

WebDropData::FileInfo::FileInfo() {
}

WebDropData::FileInfo::FileInfo(const base::string16& path,
                                const base::string16& display_name)
    : path(path),
      display_name(display_name) {
}

WebDropData::WebDropData(const WebDragData& drag_data)
    : referrer_policy(WebKit::WebReferrerPolicyDefault) {
  const WebVector<WebDragData::Item>& item_list = drag_data.items();
  for (size_t i = 0; i < item_list.size(); ++i) {
    const WebDragData::Item& item = item_list[i];
    switch (item.storageType) {
      case WebDragData::Item::StorageTypeString: {
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeText)) {
          text = base::NullableString16(item.stringData, false);
          break;
        }
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeURIList)) {
          url = GURL(item.stringData);
          url_title = item.title;
          break;
        }
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeDownloadURL)) {
          download_metadata = item.stringData;
          break;
        }
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeHTML)) {
          html = base::NullableString16(item.stringData, false);
          html_base_url = item.baseURL;
          break;
        }
        custom_data.insert(std::make_pair(item.stringType, item.stringData));
        break;
      }
      case WebDragData::Item::StorageTypeBinaryData:
        file_contents.assign(item.binaryData.data(),
                             item.binaryData.size());
        file_description_filename = item.title;
        break;
      case WebDragData::Item::StorageTypeFilename:
        // TODO(varunjain): This only works on chromeos. Support win/mac/gtk.
        filenames.push_back(FileInfo(item.filenameData,
                                     item.displayNameData));
        break;
    }
  }
}

WebDropData::WebDropData()
    : referrer_policy(WebKit::WebReferrerPolicyDefault) {
}

WebDropData::~WebDropData() {
}
