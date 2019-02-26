// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

WebAppInstallFinalizer::WebAppInstallFinalizer(WebAppRegistrar* registrar,
                                               WebAppIconManager* icon_manager)
    : registrar_(registrar), icon_manager_(icon_manager) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AppId app_id = GenerateAppIdFromURL(web_app_info->app_url);
  if (registrar_->GetAppById(app_id)) {
    std::move(callback).Run(app_id, InstallResultCode::kAlreadyInstalled);
    return;
  }

  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(base::UTF16ToUTF8(web_app_info->title));
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info->description));
  web_app->SetLaunchUrl(web_app_info->app_url);
  web_app->SetScope(web_app_info->scope);
  web_app->SetThemeColor(web_app_info->theme_color);

  icon_manager_->WriteData(
      std::move(app_id), std::move(web_app_info),
      base::BindOnce(&WebAppInstallFinalizer::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(web_app)));
}

void WebAppInstallFinalizer::OnDataWritten(InstallFinalizedCallback callback,
                                           std::unique_ptr<WebApp> web_app,
                                           bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(AppId(), InstallResultCode::kWriteDataFailed);
    return;
  }

  // TODO(loyso): Add |Icons| object into WebApp to iterate over icons and to
  // load SkBitmap pixels asynchronously (save memory).

  AppId app_id = web_app->app_id();

  registrar_->RegisterApp(std::move(web_app));

  std::move(callback).Run(std::move(app_id), InstallResultCode::kSuccess);
}

}  // namespace web_app
