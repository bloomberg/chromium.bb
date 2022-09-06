// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image_editor/image_editor_ui.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/image_editor_resources.h"
#include "chrome/grit/image_editor_untrusted_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace image_editor {

ImageEditorUI::ImageEditorUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  // Set up the chrome://image-editor source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIImageEditorHost);
  html_source->SetDefaultResource(IDR_IMAGE_EDITOR_IMAGE_EDITOR_HTML);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StringPrintf("frame-src %s;",
                         chrome::kChromeUIUntrustedImageEditorURL));

  // Allow use of SharedArrayBuffer (required by wasm code in the iframe guest).
  html_source->OverrideCrossOriginOpenerPolicy("same-origin");
  html_source->OverrideCrossOriginEmbedderPolicy("require-corp");

  content::WebUIDataSource::Add(profile_, html_source);

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

ImageEditorUI::~ImageEditorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ImageEditorUI)

}  // namespace image_editor
