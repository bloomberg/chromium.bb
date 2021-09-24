// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_ui.h"

#include <utility>

#include "ash/grit/ash_media_app_resources.h"
#include "ash/webui/media_app_ui/media_app_page_handler.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "chromeos/components/web_applications/webui_test_prod_util.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {
namespace {

content::WebUIDataSource* CreateHostDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIMediaAppHost);

  // Add resources from ash_media_app_resources.pak.
  source->SetDefaultResource(IDR_MEDIA_APP_INDEX_HTML);
  source->AddResourcePath("launch.js", IDR_MEDIA_APP_LAUNCH_JS);
  source->AddLocalizedString("appTitle", IDS_MEDIA_APP_APP_NAME);

  // Redirects "system_assets/app_icon_*.png" (from manifest.json) to the icons
  // for the gallery app.
  // TODO(b/141588875): Switch these to IDR_MEDIA_APP_APP_ICON_*_PNG in the
  // internal media_app_bundle_resources.grd file (and add more icon
  // resolutions) when the final icon is ready.
  source->AddResourcePath("system_assets/app_icon_16.png",
                          IDR_MEDIA_APP_GALLERY_ICON_16_PNG);
  source->AddResourcePath("system_assets/app_icon_32.png",
                          IDR_MEDIA_APP_GALLERY_ICON_32_PNG);
  source->AddResourcePath("system_assets/app_icon_48.png",
                          IDR_MEDIA_APP_GALLERY_ICON_48_PNG);
  source->AddResourcePath("system_assets/app_icon_64.png",
                          IDR_MEDIA_APP_GALLERY_ICON_64_PNG);
  source->AddResourcePath("system_assets/app_icon_96.png",
                          IDR_MEDIA_APP_GALLERY_ICON_96_PNG);
  source->AddResourcePath("system_assets/app_icon_128.png",
                          IDR_MEDIA_APP_GALLERY_ICON_128_PNG);
  source->AddResourcePath("system_assets/app_icon_192.png",
                          IDR_MEDIA_APP_GALLERY_ICON_192_PNG);
  source->AddResourcePath("system_assets/app_icon_256.png",
                          IDR_MEDIA_APP_GALLERY_ICON_256_PNG);
  return source;
}

}  // namespace

MediaAppUI::MediaAppUI(content::WebUI* web_ui,
                       std::unique_ptr<MediaAppUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source = CreateHostDataSource();
  content::WebUIDataSource::Add(browser_context, host_source);

  // The guest is in an <iframe>. Add it to CSP.
  std::string csp = std::string("frame-src ") + kChromeUIMediaAppGuestURL + ";";
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);
  // Allow use of SharedArrayBuffer (required by wasm code in the iframe guest).
  host_source->OverrideCrossOriginOpenerPolicy("same-origin");
  host_source->OverrideCrossOriginEmbedderPolicy("require-corp");

  if (MaybeConfigureTestableDataSource(host_source)) {
    host_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::TrustedTypes,
        std::string("trusted-types test-harness;"));
  }

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIMediaAppURL));
  allowlist->RegisterAutoGrantedPermissions(
      host_origin, {
                       ContentSettingsType::COOKIES,
                       ContentSettingsType::FILE_HANDLING,
                       ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                       ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                       ContentSettingsType::IMAGES,
                       ContentSettingsType::JAVASCRIPT,
                       ContentSettingsType::SOUND,
                   });
  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

MediaAppUI::~MediaAppUI() = default;

void MediaAppUI::BindInterface(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void MediaAppUI::CreatePageHandler(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<MediaAppPageHandler>(this, std::move(receiver));
}

bool MediaAppUI::IsJavascriptErrorReportingEnabled() {
  // JavaScript errors are reported via CrashReportPrivate.reportError. Don't
  // send duplicate reports via WebUI.
  return false;
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaAppUI)

}  // namespace ash
