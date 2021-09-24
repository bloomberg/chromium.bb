// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/untrusted_eche_app_ui.h"

#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_eche_app_resources.h"
#include "chromeos/grit/chromeos_eche_bundle_resources.h"
#include "chromeos/grit/chromeos_eche_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "url/gurl.h"

namespace chromeos {
namespace eche_app {

UntrustedEcheAppUIConfig::UntrustedEcheAppUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  eche_app::kChromeUIEcheAppGuestHost) {}

UntrustedEcheAppUIConfig::~UntrustedEcheAppUIConfig() = default;

std::unique_ptr<content::WebUIController>
UntrustedEcheAppUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<UntrustedEcheAppUI>(web_ui);
}

UntrustedEcheAppUI::UntrustedEcheAppUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(eche_app::kChromeUIEcheAppGuestURL);

  html_source->AddResourcePath("untrusted_index.html",
                               IDR_CHROMEOS_ECHE_UNTRUSTED_INDEX_HTML);
  html_source->AddResourcePath("js/app_bundle.js",
                               IDR_CHROMEOS_ECHE_APP_BUNDLE_JS);
  html_source->AddResourcePath("assets/app_bundle.css",
                               IDR_CHROMEOS_ECHE_APP_BUNDLE_CSS);
  html_source->AddResourcePath(
      "message_pipe.js",
      IDR_CHROMEOS_ECHE_APP_______SYSTEM_APPS_PUBLIC_JS_MESSAGE_PIPE_JS);
  html_source->AddResourcePath("message_types.js",
                               IDR_CHROMEOS_ECHE_APP_MESSAGE_TYPES_JS);
  html_source->AddResourcePath("receiver.js",
                               IDR_CHROMEOS_ECHE_APP_RECEIVER_JS);

  html_source->AddResourcePaths(base::make_span(
      kChromeosEcheBundleResources, kChromeosEcheBundleResourcesSize));

  html_source->AddFrameAncestor(GURL(eche_app::kChromeUIEcheAppURL));

  // DisableTrustedTypesCSP to support TrustedTypePolicy named 'goog#html'.
  // It is the Closure templating system that renders our UI, as it does many
  // other web apps using it.
  html_source->DisableTrustedTypesCSP();
  // TODO(b/194964287): Audit and tighten CSP.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

UntrustedEcheAppUI::~UntrustedEcheAppUI() = default;

}  // namespace eche_app
}  // namespace chromeos
