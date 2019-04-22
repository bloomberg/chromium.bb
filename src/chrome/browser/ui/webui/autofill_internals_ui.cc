// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_internals_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

#include "base/bind.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"

namespace {

content::WebUIDataSource* CreateAutofillInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAutofillInternalsHost);

  source->SetDefaultResource(IDR_AUTOFILL_INTERNALS_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

AutofillInternalsUI::AutofillInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://autofill-internals source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateAutofillInternalsHTMLSource());
}
