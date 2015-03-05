// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/oobe_resources.h"
#include "grit/oobe_resources_map.h"
#include "ui/oobe/declarations/oobe_web_ui_view.h"
#include "ui/oobe/oobe_md_ui.h"

namespace {

void SetUpDataSource(content::WebUIDataSource* source) {
  source->SetJsonPath("strings.js");
  source->AddResourcePath("", IDR_OOBE_UI_HTML);

  // Add all resources defined in "oobe_resources.grd" file at once.
  const char prefix[] = "resources/";
  const size_t prefix_length = arraysize(prefix) - 1;
  for (size_t i = 0; i < kOobeResourcesSize; ++i) {
    std::string name = kOobeResources[i].name;
    DCHECK(StartsWithASCII(name, prefix, true));
    source->AddResourcePath(name.substr(prefix_length),
                            kOobeResources[i].value);
  }
}

}  // namespace

OobeMdUI::OobeMdUI(content::WebUI* web_ui, const std::string& host)
    : WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(host);
  SetUpDataSource(source);

  view_.reset(new gen::OobeWebUIView(web_ui));
  view_->Init();
  view_->SetUpDataSource(source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

OobeMdUI::~OobeMdUI() {}
