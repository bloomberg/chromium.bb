// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webdropdata.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

using WebKit::WebData;
using WebKit::WebDragData;
using WebKit::WebString;
using WebKit::WebVector;

WebDropData::WebDropData(const WebDragData& drag_data)
    : url(drag_data.url()),
      url_title(drag_data.urlTitle()),
      download_metadata(drag_data.downloadMetadata()),
      file_extension(drag_data.fileExtension()),
      plain_text(drag_data.plainText()),
      text_html(drag_data.htmlText()),
      html_base_url(drag_data.htmlBaseURL()),
      file_description_filename(drag_data.fileContentFilename()) {
  if (drag_data.containsFilenames()) {
    WebVector<WebString> filenames_copy;
    drag_data.filenames(filenames_copy);
    for (size_t i = 0; i < filenames_copy.size(); ++i)
      filenames.push_back(filenames_copy[i]);
  }
  WebData contents = drag_data.fileContent();
  if (!contents.isEmpty())
    file_contents.assign(contents.data(), contents.size());
}

WebDropData::WebDropData() {
}

WebDropData::~WebDropData() {
}

WebDragData WebDropData::ToDragData() const {
  WebDragData result;
  result.initialize();
  result.setURL(url);
  result.setURLTitle(url_title);
  result.setFileExtension(file_extension);
  result.setFilenames(filenames);
  result.setPlainText(plain_text);
  result.setHTMLText(text_html);
  result.setHTMLBaseURL(html_base_url);
  result.setFileContentFilename(file_description_filename);
  result.setFileContent(WebData(file_contents.data(), file_contents.size()));
  return result;
}
