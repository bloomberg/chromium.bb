// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/sample_system_web_app_ui/untrusted_sample_system_web_app_ui.h"

#include "ash/webui/grit/ash_sample_system_web_app_untrusted_resources_map.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ash {

UntrustedSampleSystemWebAppUIConfig::UntrustedSampleSystemWebAppUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIUntrustedSampleSystemWebAppHost) {}

UntrustedSampleSystemWebAppUIConfig::~UntrustedSampleSystemWebAppUIConfig() =
    default;

std::unique_ptr<content::WebUIController>
UntrustedSampleSystemWebAppUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<UntrustedSampleSystemWebAppUI>(web_ui);
}

UntrustedSampleSystemWebAppUI::UntrustedSampleSystemWebAppUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::Create(kChromeUIUntrustedSampleSystemWebAppURL);
  untrusted_source->AddResourcePaths(
      base::make_span(kAshSampleSystemWebAppUntrustedResources,
                      kAshSampleSystemWebAppUntrustedResourcesSize));
  untrusted_source->AddFrameAncestor(GURL(kChromeUISampleSystemWebAppURL));

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, untrusted_source);
}

UntrustedSampleSystemWebAppUI::~UntrustedSampleSystemWebAppUI() = default;

void UntrustedSampleSystemWebAppUI::BindInterface(
    mojo::PendingReceiver<mojom::sample_swa::UntrustedPageInterfacesFactory>
        factory) {
  if (untrusted_page_factory_.is_bound())
    untrusted_page_factory_.reset();

  untrusted_page_factory_.Bind(std::move(factory));
}

void UntrustedSampleSystemWebAppUI::CreateParentPage(
    mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage>
        child_untrusted_page,
    mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage>
        parent_trusted_page) {
  // Find the parent frame's controller.
  auto* chrome_frame = web_ui()->GetWebContents()->GetMainFrame();
  if (!chrome_frame)
    return;

  CHECK(chrome_frame->GetWebUI());

  auto* sample_ui_controller =
      chrome_frame->GetWebUI()->GetController()->GetAs<SampleSystemWebAppUI>();
  CHECK(sample_ui_controller);

  sample_ui_controller->CreateParentPage(std::move(child_untrusted_page),
                                         std::move(parent_trusted_page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(UntrustedSampleSystemWebAppUI)

}  // namespace ash
