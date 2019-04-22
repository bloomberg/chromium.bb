// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

content::WebUIDataSource* CreateAppManagementUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAppManagementHost);

  source->AddLocalizedString("appListTitle", IDS_APP_MANAGEMENT_APP_LIST_TITLE);
  source->AddLocalizedString("appNoPermission",
                             IDS_APPLICATION_INFO_APP_NO_PERMISSIONS_TEXT);
  source->AddLocalizedString("back", IDS_APP_MANAGEMENT_BACK);
  source->AddLocalizedString("camera", IDS_APP_MANAGEMENT_CAMERA);
  source->AddLocalizedString("lessApps", IDS_APP_MANAGEMENT_LESS_APPS);
  source->AddLocalizedString("location", IDS_APP_MANAGEMENT_LOCATION);
  source->AddLocalizedString("microphone", IDS_APP_MANAGEMENT_MICROPHONE);
  source->AddLocalizedString("moreApps", IDS_APP_MANAGEMENT_MORE_APPS);
  source->AddLocalizedString("noSearchResults", IDS_APP_MANAGEMENT_NO_RESULTS);
  source->AddLocalizedString("notificationSublabel",
                             IDS_APP_MANAGEMENT_NOTIFICATIONS_SUBLABEL);
  source->AddLocalizedString("notifications", IDS_APP_MANAGEMENT_NOTIFICATIONS);
  source->AddLocalizedString("openAndroidSettings",
                             IDS_APP_MANAGEMENT_ANDROID_SETTINGS);
  source->AddLocalizedString("openExtensionsSettings",
                             IDS_APP_MANAGEMENT_EXTENSIONS_SETTINGS);
  source->AddLocalizedString("openSiteSettings",
                             IDS_APP_MANAGEMENT_SITE_SETTING);
  source->AddLocalizedString("permissions", IDS_APP_MANAGEMENT_PERMISSIONS);
  source->AddLocalizedString("pinToShelf", IDS_APP_MANAGEMENT_PIN_TO_SHELF);
  source->AddLocalizedString("searchPrompt", IDS_APP_MANAGEMENT_SEARCH_PROMPT);
  source->AddLocalizedString("size", IDS_APP_MANAGEMENT_SIZE);
  source->AddLocalizedString("title", IDS_APP_MANAGEMENT_TITLE);
  source->AddLocalizedString("uninstall", IDS_APP_MANAGEMENT_UNINSTALL);
  source->AddLocalizedString("version", IDS_APP_MANAGEMENT_VERSION);

  source->AddResourcePath("app_management.mojom-lite.js",
                          IDR_APP_MANAGEMENT_MOJO_LITE_JS);
  source->AddResourcePath("types.mojom-lite.js",
                          IDR_APP_MANAGEMENT_TYPES_MOJO_LITE_JS);
  source->AddResourcePath("bitmap.mojom-lite.js",
                          IDR_APP_MANAGEMENT_BITMAP_MOJO_LITE_JS);
  source->AddResourcePath("image.mojom-lite.js",
                          IDR_APP_MANAGEMENT_IMAGE_MOJO_LITE_JS);
  source->AddResourcePath("image_info.mojom-lite.js",
                          IDR_APP_MANAGEMENT_IMAGE_INFO_MOJO_LITE_JS);

  source->AddResourcePath("actions.html", IDR_APP_MANAGEMENT_ACTIONS_HTML);
  source->AddResourcePath("actions.js", IDR_APP_MANAGEMENT_ACTIONS_JS);
  source->AddResourcePath("api_listener.html",
                          IDR_APP_MANAGEMENT_API_LISTENER_HTML);
  source->AddResourcePath("api_listener.js",
                          IDR_APP_MANAGEMENT_API_LISTENER_JS);
  source->AddResourcePath("app.html", IDR_APP_MANAGEMENT_APP_HTML);
  source->AddResourcePath("app.js", IDR_APP_MANAGEMENT_APP_JS);
  source->AddResourcePath("app_item.html", IDR_APP_MANAGEMENT_APP_ITEM_HTML);
  source->AddResourcePath("app_item.js", IDR_APP_MANAGEMENT_APP_ITEM_JS);
  source->AddResourcePath("arc_permission_view.html",
                          IDR_APP_MANAGEMENT_ARC_PERMISSION_VIEW_HTML);
  source->AddResourcePath("arc_permission_view.js",
                          IDR_APP_MANAGEMENT_ARC_PERMISSION_VIEW_JS);
  source->AddResourcePath("browser_proxy.html",
                          IDR_APP_MANAGEMENT_BROWSER_PROXY_HTML);
  source->AddResourcePath("browser_proxy.js",
                          IDR_APP_MANAGEMENT_BROWSER_PROXY_JS);
  source->AddResourcePath("chrome_app_permission_view.html",
                          IDR_APP_MANAGEMENT_CHROME_APP_PERMISSION_VIEW_HTML);
  source->AddResourcePath("chrome_app_permission_view.js",
                          IDR_APP_MANAGEMENT_CHROME_APP_PERMISSION_VIEW_JS);
  source->AddResourcePath("constants.html", IDR_APP_MANAGEMENT_CONSTANTS_HTML);
  source->AddResourcePath("constants.js", IDR_APP_MANAGEMENT_CONSTANTS_JS);
  source->AddResourcePath("expandable_app_list.html",
                          IDR_APP_MANAGEMENT_EXPANDABLE_APP_LIST_HTML);
  source->AddResourcePath("expandable_app_list.js",
                          IDR_APP_MANAGEMENT_EXPANDABLE_APP_LIST_JS);
  source->AddResourcePath("dom_switch.html",
                          IDR_APP_MANAGEMENT_DOM_SWITCH_HTML);
  source->AddResourcePath("dom_switch.js", IDR_APP_MANAGEMENT_DOM_SWITCH_JS);
  source->AddResourcePath("fake_page_handler.js",
                          IDR_APP_MANAGEMENT_FAKE_PAGE_HANDLER_JS);
  source->AddResourcePath("main_view.html", IDR_APP_MANAGEMENT_MAIN_VIEW_HTML);
  source->AddResourcePath("main_view.js", IDR_APP_MANAGEMENT_MAIN_VIEW_JS);
  source->AddResourcePath("notifications_view.html",
                          IDR_APP_MANAGEMENT_NOTIFICATIONS_VIEW_HTML);
  source->AddResourcePath("notifications_view.js",
                          IDR_APP_MANAGEMENT_NOTIFICATIONS_VIEW_JS);
  source->AddResourcePath("permission_view_header.html",
                          IDR_APP_MANAGEMENT_PERMISSION_VIEW_HEADER_HTML);
  source->AddResourcePath("permission_view_header.js",
                          IDR_APP_MANAGEMENT_PERMISSION_VIEW_HEADER_JS);
  source->AddResourcePath("metadata_view.html",
                          IDR_APP_MANAGEMENT_METADATA_VIEW_HTML);
  source->AddResourcePath("metadata_view.js",
                          IDR_APP_MANAGEMENT_METADATA_VIEW_JS);
  source->AddResourcePath("permission_item.html",
                          IDR_APP_MANAGEMENT_PERMISSION_ITEM_HTML);
  source->AddResourcePath("permission_item.js",
                          IDR_APP_MANAGEMENT_PERMISSION_ITEM_JS);
  source->AddResourcePath("permission_toggle.html",
                          IDR_APP_MANAGEMENT_PERMISSION_TOGGLE_HTML);
  source->AddResourcePath("permission_toggle.js",
                          IDR_APP_MANAGEMENT_PERMISSION_TOGGLE_JS);
  source->AddResourcePath("pwa_permission_view.html",
                          IDR_APP_MANAGEMENT_PWA_PERMISSION_VIEW_HTML);
  source->AddResourcePath("pwa_permission_view.js",
                          IDR_APP_MANAGEMENT_PWA_PERMISSION_VIEW_JS);
  source->AddResourcePath("reducers.html", IDR_APP_MANAGEMENT_REDUCERS_HTML);
  source->AddResourcePath("reducers.js", IDR_APP_MANAGEMENT_REDUCERS_JS);
  source->AddResourcePath("router.html", IDR_APP_MANAGEMENT_ROUTER_HTML);
  source->AddResourcePath("router.js", IDR_APP_MANAGEMENT_ROUTER_JS);
  source->AddResourcePath("search_view.html",
                          IDR_APP_MANAGEMENT_SEARCH_VIEW_HTML);
  source->AddResourcePath("search_view.js", IDR_APP_MANAGEMENT_SEARCH_VIEW_JS);
  source->AddResourcePath("shared_style.html",
                          IDR_APP_MANAGEMENT_SHARED_STYLE_HTML);
  source->AddResourcePath("shared_vars.html",
                          IDR_APP_MANAGEMENT_SHARED_VARS_HTML);
  source->AddResourcePath("store.html", IDR_APP_MANAGEMENT_STORE_HTML);
  source->AddResourcePath("store.js", IDR_APP_MANAGEMENT_STORE_JS);
  source->AddResourcePath("store_client.html",
                          IDR_APP_MANAGEMENT_STORE_CLIENT_HTML);
  source->AddResourcePath("store_client.js",
                          IDR_APP_MANAGEMENT_STORE_CLIENT_JS);
  source->AddResourcePath("types.js", IDR_APP_MANAGEMENT_TYPES_JS);
  source->AddResourcePath("util.html", IDR_APP_MANAGEMENT_UTIL_HTML);
  source->AddResourcePath("util.js", IDR_APP_MANAGEMENT_UTIL_JS);

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

  // Make the chrome://app-icon/ resource available.
  if (profile) {
    content::URLDataSource::Add(profile,
                                std::make_unique<apps::AppIconSource>(profile));
  }

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "appListPreview", IDS_APP_MANAGEMENT_APP_LIST_PREVIEW);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
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
