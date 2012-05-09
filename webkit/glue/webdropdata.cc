// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webdropdata.h"

#include <utility>

#include "base/string_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "ui/base/clipboard/clipboard.h"

using WebKit::WebData;
using WebKit::WebDragData;
using WebKit::WebString;
using WebKit::WebVector;

WebDropData::FileInfo::FileInfo() {
}

WebDropData::FileInfo::FileInfo(const string16& path,
                                const string16& display_name)
    : path(path),
      display_name(display_name) {
}

WebDropData::WebDropData(const WebDragData& drag_data) {
  const WebVector<WebDragData::Item>& item_list = drag_data.items();
  for (size_t i = 0; i < item_list.size(); ++i) {
    const WebDragData::Item& item = item_list[i];
    switch (item.storageType) {
      case WebDragData::Item::StorageTypeString: {
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeText)) {
          plain_text = item.stringData;
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
          text_html = item.stringData;
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

WebDropData::WebDropData() {
}

WebDropData::~WebDropData() {
}

WebDragData WebDropData::ToDragData() const {
  std::vector<WebDragData::Item> item_list;

  // These fields are currently unused when dragging into WebKit.
  DCHECK(download_metadata.empty());
  DCHECK(file_contents.empty());
  DCHECK(file_description_filename.empty());

  // TODO(dcheng): We need a way to distinguish between null and empty strings.
  if (!plain_text.empty()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeText);
    item.stringData = plain_text;
    item_list.push_back(item);
  }

  if (!url.is_empty()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeURIList);
    item.stringData = WebString::fromUTF8(url.spec());
    item.title = url_title;
    item_list.push_back(item);
  }

  if (!text_html.empty()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeHTML);
    item.stringData = text_html;
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

  for (std::map<string16, string16>::const_iterator it = custom_data.begin();
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
