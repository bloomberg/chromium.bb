// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webdropdata.h"

#include <utility>

#include "base/string_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
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
    : referrer_policy(WebKit::WebReferrerPolicyDefault),
      text(NullableString16(true)),
      html(NullableString16(true)) {
  const WebVector<WebDragData::Item>& item_list = drag_data.items();
  for (size_t i = 0; i < item_list.size(); ++i) {
    const WebDragData::Item& item = item_list[i];
    switch (item.storageType) {
      case WebDragData::Item::StorageTypeString: {
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeText)) {
          text = NullableString16(item.stringData, false);
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
          html = NullableString16(item.stringData, false);
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
    : referrer_policy(WebKit::WebReferrerPolicyDefault),
      text(NullableString16(true)),
      html(NullableString16(true)) {
}

WebDropData::~WebDropData() {
}

WebDragData WebDropData::ToDragData() const {
  std::vector<WebDragData::Item> item_list;

  // These fields are currently unused when dragging into WebKit.
  DCHECK(download_metadata.empty());
  DCHECK(file_contents.empty());
  DCHECK(file_description_filename.empty());

  if (!text.is_null()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeText);
    item.stringData = text.string();
    item_list.push_back(item);
  }

  // TODO(dcheng): Do we need to distinguish between null and empty URLs? Is it
  // meaningful to write an empty URL to the clipboard?
  if (!url.is_empty()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeURIList);
    item.stringData = WebString::fromUTF8(url.spec());
    item.title = url_title;
    item_list.push_back(item);
  }

  if (!html.is_null()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeHTML);
    item.stringData = html.string();
    item.baseURL = html_base_url;
    item_list.push_back(item);
  }

  for (std::vector<FileInfo>::const_iterator it =
           filenames.begin(); it != filenames.end(); ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeFilename;
    item.filenameData = it->path;
    item.displayNameData = it->display_name;
    item_list.push_back(item);
  }

  for (std::map<base::string16, base::string16>::const_iterator it =
           custom_data.begin();
       it != custom_data.end();
       ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = it->first;
    item.stringData = it->second;
    item_list.push_back(item);
  }

  WebDragData result;
  result.initialize();
  result.setItems(item_list);
  result.setFilesystemId(filesystem_id);
  return result;
}
