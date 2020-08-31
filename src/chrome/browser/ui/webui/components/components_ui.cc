// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components/components_ui.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/components/components_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

namespace {

content::WebUIDataSource* CreateComponentsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIComponentsHost);

  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  static constexpr webui::LocalizedString kStrings[] = {
      {"componentsTitle", IDS_COMPONENTS_TITLE},
      {"componentsNoneInstalled", IDS_COMPONENTS_NONE_INSTALLED},
      {"componentVersion", IDS_COMPONENTS_VERSION},
      {"checkUpdate", IDS_COMPONENTS_CHECK_FOR_UPDATE},
      {"noComponents", IDS_COMPONENTS_NO_COMPONENTS},
      {"statusLabel", IDS_COMPONENTS_STATUS_LABEL},
      {"checkingLabel", IDS_COMPONENTS_CHECKING_LABEL},
  };
  AddLocalizedStringsBulk(source, kStrings);

  source->AddBoolean(
      "isGuest",
#if defined(OS_CHROMEOS)
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()
#else
      profile->IsOffTheRecord()
#endif
  );
  source->UseStringsJs();
  source->AddResourcePath("components.js", IDR_COMPONENTS_COMPONENTS_JS);
  source->SetDefaultResource(IDR_COMPONENTS_COMPONENTS_HTML);
  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ComponentsUI
//
///////////////////////////////////////////////////////////////////////////////

ComponentsUI::ComponentsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ComponentsHandler>(
      g_browser_process->component_updater()));

  // Set up the chrome://components/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateComponentsUIHTMLSource(profile));
}

ComponentsUI::~ComponentsUI() {}

// static
base::RefCountedMemory* ComponentsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_PLUGINS_FAVICON, scale_factor);
}
