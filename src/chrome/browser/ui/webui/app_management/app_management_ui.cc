// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

content::WebUIDataSource* CreateAppManagementUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAppLauncherPageHost);

  source->AddResourcePath("app_management.mojom-lite.js",
                          IDR_APP_MANAGEMENT_MOJO_LITE_JS);
  source->AddResourcePath("app.html", IDR_APP_MANAGEMENT_APP_HTML);
  source->AddResourcePath("app.js", IDR_APP_MANAGEMENT_APP_JS);
  source->AddResourcePath("browser_proxy.html",
                          IDR_APP_MANAGEMENT_BROWSER_PROXY_HTML);
  source->AddResourcePath("browser_proxy.js",
                          IDR_APP_MANAGEMENT_BROWSER_PROXY_JS);
  source->SetDefaultResource(IDR_APP_MANAGEMENT_INDEX_HTML);

  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// AppManagementUI
//
///////////////////////////////////////////////////////////////////////////////

AppManagementUI::AppManagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true), page_factory_binding_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the data source.
  content::WebUIDataSource* source = CreateAppManagementUIHTMLSource(profile);
  content::WebUIDataSource::Add(profile, source);

  AddHandlerToRegistry(base::BindRepeating(
      &AppManagementUI::BindPageHandlerFactory, base::Unretained(this)));
}

AppManagementUI::~AppManagementUI() = default;

bool AppManagementUI::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kAppManagement);
}

void AppManagementUI::BindPageHandlerFactory(
    app_management::mojom::PageHandlerFactoryRequest request) {
  if (page_factory_binding_.is_bound())
    page_factory_binding_.Unbind();

  page_factory_binding_.Bind(std::move(request));
}

void AppManagementUI::CreatePageHandler(
    app_management::mojom::PagePtr page,
    app_management::mojom::PageHandlerRequest request) {
  DCHECK(page);

  page_handler_.reset(new AppManagementPageHandler(std::move(request),
                                                   std::move(page), web_ui()));
}
