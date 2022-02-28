// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/internals_ui.h"

#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/internals_resources.h"
#include "chrome/grit/internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/internals/lens/lens_internals_ui_message_handler.h"
#include "chrome/browser/ui/webui/internals/notifications/notifications_internals_ui_message_handler.h"
#include "chrome/browser/ui/webui/internals/query_tiles/query_tiles_internals_ui_message_handler.h"
#else
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/internals/sessions/session_service_internals_handler.h"
#endif

namespace {

bool ShouldHandleWebUIRequestCallback(const std::string& path) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(path))
    return true;
#endif
  return false;
}

void HandleWebUIRequestCallback(
    Profile* profile,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(path)) {
    return SessionServiceInternalsHandler::HandleWebUIRequestCallback(
        profile, path, std::move(callback));
  }
#endif
  NOTREACHED();
}

}  // namespace

InternalsUI::InternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  profile_ = Profile::FromWebUI(web_ui);
  source_ = content::WebUIDataSource::Create(chrome::kChromeUIInternalsHost);
  source_->AddResourcePaths(
      base::make_span(kInternalsResources, kInternalsResourcesSize));
  source_->DisableTrustedTypesCSP();

  // chrome://internals/
  // Redirects to: chrome://chrome-urls/#internals
  source_->AddResourcePath("", IDR_INTERNALS_INTERNALS_HTML);

  // Add your sub-URL internals WebUI here.
  // Keep this set of sub-URLs in sync with |kChromeInternalsPathURLs|.
#if defined(OS_ANDROID)
  // chrome://internals/lens
  AddLensInternals(web_ui);
  // chrome://internals/notifications
  source_->AddResourcePath(
      "notifications",
      IDR_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_HTML);
  web_ui->AddMessageHandler(
      std::make_unique<NotificationsInternalsUIMessageHandler>(profile_));

  // chrome://internals/query-tiles
  if (!profile_->IsOffTheRecord())
    AddQueryTilesInternals(web_ui);
#else
  source_->AddResourcePath("user-education",
                           IDR_USER_EDUCATION_INTERNALS_INDEX_HTML);

  // chrome://internals/web-app
  // This page has moved to chrome://web-app-internals, see
  // WebAppInternalsSource.
  // TODO(crbug.com/1226263): Clean up this redirect after M94 goes stable.
  source_->AddResourcePath("web-app", IDR_WEB_APP_INTERNALS_HTML);
#endif  // defined(OS_ANDROID)

  // chrome://internals/session-service
  source_->SetRequestFilter(
      base::BindRepeating(&ShouldHandleWebUIRequestCallback),
      base::BindRepeating(&HandleWebUIRequestCallback, profile_));

  content::WebUIDataSource::Add(profile_, source_);
}

InternalsUI::~InternalsUI() = default;

#if defined(OS_ANDROID)
void InternalsUI::AddLensInternals(content::WebUI* web_ui) {
  source_->AddResourcePath("lens", IDR_LENS_INTERNALS_LENS_INTERNALS_HTML);

  web_ui->AddMessageHandler(
      std::make_unique<LensInternalsUIMessageHandler>(profile_));
}

void InternalsUI::AddQueryTilesInternals(content::WebUI* web_ui) {
  source_->AddResourcePath("query_tiles_internals.js",
                           IDR_QUERY_TILES_INTERNALS_JS);
  source_->AddResourcePath("query_tiles_internals_browser_proxy.js",
                           IDR_QUERY_TILES_INTERNALS_BROWSER_PROXY_JS);
  source_->AddResourcePath("query-tiles", IDR_QUERY_TILES_INTERNALS_HTML);
  web_ui->AddMessageHandler(
      std::make_unique<QueryTilesInternalsUIMessageHandler>(profile_));
}
#else   // defined(OS_ANDROID)
void InternalsUI::BindInterface(
    mojo::PendingReceiver<
        mojom::user_education_internals::UserEducationInternalsPageHandler>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<UserEducationInternalsPageHandlerImpl>(profile_),
      std::move(receiver));
}
#endif  // defined(OS_ANDROID)

WEB_UI_CONTROLLER_TYPE_IMPL(InternalsUI)
