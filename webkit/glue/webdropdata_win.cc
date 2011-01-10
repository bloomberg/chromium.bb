// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
  std::wstring url_str;
  if (ui::ClipboardUtil::GetUrl(data_object, &url_str, &drop_data->url_title,
                                false)) {
    GURL test_url(url_str);
    if (test_url.is_valid())
      drop_data->url = test_url;
  }
  ui::ClipboardUtil::GetFilenames(data_object, &drop_data->filenames);
  ui::ClipboardUtil::GetPlainText(data_object, &drop_data->plain_text);
  std::string base_url;
  ui::ClipboardUtil::GetHtml(data_object, &drop_data->text_html, &base_url);
  if (!base_url.empty()) {
    drop_data->html_base_url = GURL(base_url);
  }
  ui::ClipboardUtil::GetFileContents(data_object,
      &drop_data->file_description_filename, &drop_data->file_contents);
}
