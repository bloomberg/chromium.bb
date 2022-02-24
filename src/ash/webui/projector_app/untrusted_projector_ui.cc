// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_ui.h"

#include "ash/webui/grit/ash_projector_app_untrusted_resources.h"
#include "ash/webui/grit/ash_projector_app_untrusted_resources_map.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ash {

namespace {

content::WebUIDataSource* CreateProjectorHTMLSource(
    UntrustedProjectorUIDelegate* delegate) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIUntrustedProjectorAppUrl);

  source->AddResourcePaths(
      base::make_span(kAshProjectorAppUntrustedResources,
                      kAshProjectorAppUntrustedResourcesSize));
  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppBundleResources,
                      kChromeosProjectorAppBundleResourcesSize));
  source->AddResourcePath("", IDR_ASH_PROJECTOR_APP_UNTRUSTED_APP_INDEX_HTML);

  // Provide a list of specific script resources (javascript files and inlined
  // scripts inside html) or their sha-256 hashes to allow to be executed.
  // "wasm-eval" is added to allow wasm. "chrome-untrusted://resources" is
  // needed to allow the post message api.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' chrome-untrusted://resources;");
  // Allow fonts.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      "font-src https://fonts.gstatic.com;");
  // Allow styles to include inline styling needed for Polymer elements.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      // Allows streaming video.
      "media-src 'self' https://*.drive.google.com;");
  // Allow images to also handle data urls.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src blob: data: 'self' https://*.googleusercontent.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' https://www.googleapis.com "
      "https://drive.google.com;");

  // TODO(b/197120695): re-enable trusted type after fixing the issue that icon
  // template is setting innerHTML.
  source->DisableTrustedTypesCSP();

  source->AddFrameAncestor(GURL(kChromeUITrustedProjectorUrl));

  delegate->PopulateLoadTimeData(source);
  source->UseStringsJs();

  return source;
}

}  // namespace

UntrustedProjectorUI::UntrustedProjectorUI(
    content::WebUI* web_ui,
    UntrustedProjectorUIDelegate* delegate)
    : UntrustedWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context,
                                CreateProjectorHTMLSource(delegate));
}

UntrustedProjectorUI::~UntrustedProjectorUI() = default;

}  // namespace ash
