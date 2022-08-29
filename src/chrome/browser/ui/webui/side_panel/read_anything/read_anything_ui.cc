// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_resources.h"
#include "chrome/grit/side_panel_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

ReadAnythingUI::ReadAnythingUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIReadAnythingSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"readAnythingTabTitle", IDS_READ_ANYTHING_TITLE},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  webui::SetupWebUIDataSource(
      source, base::make_span(kSidePanelResources, kSidePanelResourcesSize),
      IDR_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

ReadAnythingUI::~ReadAnythingUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadAnythingUI)

void ReadAnythingUI::BindInterface(
    mojo::PendingReceiver<read_anything::mojom::PageHandlerFactory> receiver) {
  read_anything_page_factory_receiver_.reset();
  read_anything_page_factory_receiver_.Bind(std::move(receiver));
}

void ReadAnythingUI::CreatePageHandler(
    mojo::PendingRemote<read_anything::mojom::Page> page,
    mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver) {
  DCHECK(page);
  read_anything_page_handler_ = std::make_unique<ReadAnythingPageHandler>(
      std::move(page), std::move(receiver));
  if (embedder())
    embedder()->ShowUI();
}
