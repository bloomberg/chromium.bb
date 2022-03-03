// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image_editor/image_editor_ui.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/image_editor/editor_untrusted_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace image_editor {

ImageEditorUI::ImageEditorUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  // Set up the chrome://image-editor source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIImageEditorHost);
  html_source->SetDefaultResource(IDR_IMAGE_EDITOR_HTML);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StringPrintf("frame-src %s;",
                         chrome::kChromeUIUntrustedImageEditorURL));
  content::WebUIDataSource::Add(profile_, html_source);
  content::URLDataSource::Add(
      profile_, std::make_unique<EditorUntrustedSource>(profile_));
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

ImageEditorUI::~ImageEditorUI() = default;

void ImageEditorUI::RecordUserAction(mojom::EditAction action) {
  base::UmaHistogramEnumeration("Sharing.DesktopScreenshot.Action", action);
}

void ImageEditorUI::BindInterface(
    mojo::PendingReceiver<image_editor::mojom::ScreenshotCoordinator>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ImageEditorUI)

}  // namespace image_editor
