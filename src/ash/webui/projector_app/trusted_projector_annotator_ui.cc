// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/trusted_projector_annotator_ui.h"

#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources_map.h"
#include "ash/webui/projector_app/annotator_message_handler.h"
#include "ash/webui/projector_app/projector_message_handler.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

content::WebUIDataSource* CreateProjectorAnnotatorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIProjectorAnnotatorHost);

  // TODO(b/216523790): Split trusted annotator resources into a separate
  // bundle.
  source->AddResourcePaths(base::make_span(
      kAshProjectorAppTrustedResources, kAshProjectorAppTrustedResourcesSize));

  source->AddResourcePath(
      "", IDR_ASH_PROJECTOR_APP_TRUSTED_ANNOTATOR_ANNOTATOR_EMBEDDER_HTML);

  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedAnnotatorUrl + ";";
  // Allow use of SharedArrayBuffer (required by wasm code in the iframe guest).
  source->OverrideCrossOriginOpenerPolicy("same-origin");
  source->OverrideCrossOriginEmbedderPolicy("require-corp");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  return source;
}

}  // namespace

TrustedProjectorAnnotatorUI::TrustedProjectorAnnotatorUI(
    content::WebUI* web_ui,
    const GURL& url,
    PrefService* pref_service)
    : MojoBubbleWebUIController(web_ui, /*enable_chrome_send=*/true) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context,
                                CreateProjectorAnnotatorHTMLSource());

  // The Annotator and Projector SWA embed contents in a sandboxed
  // chrome-untrusted:// iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

TrustedProjectorAnnotatorUI::~TrustedProjectorAnnotatorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TrustedProjectorAnnotatorUI)

}  // namespace ash
