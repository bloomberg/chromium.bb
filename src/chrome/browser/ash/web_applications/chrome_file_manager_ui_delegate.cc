// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_file_manager_ui_delegate.h"

#include <memory.h>

#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_allowlist.h"

ChromeFileManagerUIDelegate::ChromeFileManagerUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {
  DCHECK(web_ui_);
}

void ChromeFileManagerUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) const {
  base::Value dict = GetFileManagerStrings();

  const std::string locale = g_browser_process->GetApplicationLocale();
  AddFileManagerFeatureStrings(locale, Profile::FromWebUI(web_ui_), &dict);

  source->AddLocalizedStrings(base::Value::AsDictionaryValue(dict));
}
