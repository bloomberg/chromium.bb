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
  string16 url_str;
  if (ui::ClipboardUtil::GetUrl(data_object, &url_str, &drop_data->url_title,
                                false)) {
    GURL test_url(url_str);
    if (test_url.is_valid())
      drop_data->url = test_url;
  }
  std::vector<string16> filenames;
  ui::ClipboardUtil::GetFilenames(data_object, &filenames);
  for (size_t i = 0; i < filenames.size(); ++i)
    drop_data->filenames.push_back(FileInfo(filenames[i], string16()));
  ui::ClipboardUtil::GetPlainText(data_object, &drop_data->plain_text);
  std::string base_url;
  ui::ClipboardUtil::GetHtml(data_object, &drop_data->text_html, &base_url);
  if (!base_url.empty()) {
    drop_data->html_base_url = GURL(base_url);
  }
  ui::ClipboardUtil::GetWebCustomData(data_object,
      &drop_data->custom_data);
}
