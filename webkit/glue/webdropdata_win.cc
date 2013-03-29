// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webdropdata.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "googleurl/src/gurl.h"
#include "ui/base/clipboard/clipboard_util_win.h"

// static
void WebDropData::PopulateWebDropData(IDataObject* data_object,
                                      WebDropData* drop_data) {
  base::string16 url_str;
  if (ui::ClipboardUtil::GetUrl(data_object, &url_str, &drop_data->url_title,
                                false)) {
    GURL test_url(url_str);
    if (test_url.is_valid())
      drop_data->url = test_url;
  }
  std::vector<base::string16> filenames;
  ui::ClipboardUtil::GetFilenames(data_object, &filenames);
  for (size_t i = 0; i < filenames.size(); ++i)
    drop_data->filenames.push_back(FileInfo(filenames[i], base::string16()));
  base::string16 text;
  ui::ClipboardUtil::GetPlainText(data_object, &text);
  if (!text.empty()) {
    drop_data->text = NullableString16(text, false);
  }
  base::string16 html;
  std::string html_base_url;
  ui::ClipboardUtil::GetHtml(data_object, &html, &html_base_url);
  if (!html.empty()) {
    drop_data->html = NullableString16(html, false);
  }
  if (!html_base_url.empty()) {
    drop_data->html_base_url = GURL(html_base_url);
  }
  ui::ClipboardUtil::GetWebCustomData(data_object,
      &drop_data->custom_data);
}
