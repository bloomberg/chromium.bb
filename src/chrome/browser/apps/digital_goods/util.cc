// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/util.h"

#include "base/optional.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/web_contents.h"

namespace apps {

std::string GetTwaPackageName(content::RenderFrameHost* render_frame_host) {
  auto* apk_web_app_service = chromeos::ApkWebAppService::Get(
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));

  if (!apk_web_app_service) {
    return "";
  }

  base::Optional<std::string> twa_package_name =
      apk_web_app_service->GetPackageNameForWebApp(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetLastCommittedURL());

  return twa_package_name.value_or("");
}

std::string GetScope(content::RenderFrameHost* render_frame_host) {
  web_app::AppRegistrar& registrar =
      web_app::WebAppProvider::Get(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))
          ->registrar();
  base::Optional<web_app::AppId> app_id = registrar.FindAppWithUrlInScope(
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetLastCommittedURL());
  if (!app_id) {
    return "";
  }

  GURL scope = registrar.GetAppScope(app_id.value());
  if (!scope.is_valid()) {
    return "";
  }

  return scope.spec();
}

}  // namespace apps
