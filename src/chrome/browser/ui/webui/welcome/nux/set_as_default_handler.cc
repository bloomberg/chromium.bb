// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/set_as_default_handler.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace nux {

SetAsDefaultHandler::SetAsDefaultHandler()
    : settings::DefaultBrowserHandler() {}

SetAsDefaultHandler::~SetAsDefaultHandler() {}

void SetAsDefaultHandler::RecordSetAsDefaultUMA() {
  // TODO(scottchen): Add UMA tracking.
}

void SetAsDefaultHandler::AddSources(content::WebUIDataSource* html_source) {
  // Localized strings.

  // Add required resources.
  html_source->AddResourcePath("nux_set_as_default.html",
                               IDR_NUX_SET_AS_DEFAULT_HTML);
  html_source->AddResourcePath("nux_set_as_default.js",
                               IDR_NUX_SET_AS_DEFAULT_JS);
  html_source->AddResourcePath("nux_set_as_default_proxy.html",
                               IDR_NUX_SET_AS_DEFAULT_PROXY_HTML);
  html_source->AddResourcePath("nux_set_as_default_proxy.js",
                               IDR_NUX_SET_AS_DEFAULT_PROXY_JS);

  // Add icons
}

}  // namespace nux