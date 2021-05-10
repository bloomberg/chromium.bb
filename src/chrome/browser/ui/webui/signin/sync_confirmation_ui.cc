// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

SyncConfirmationUI::SyncConfirmationUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  // Initializing the WebUIDataSource in the constructor is needed for polymer
  // tests.
  Initialize(/*profile_creation_flow_color=*/base::nullopt);
}

SyncConfirmationUI::~SyncConfirmationUI() = default;

void SyncConfirmationUI::InitializeMessageHandlerWithBrowser(Browser* browser) {
  InitializeMessageHandler(browser);
}

void SyncConfirmationUI::InitializeMessageHandlerForCreationFlow(
    SkColor profile_color) {
  // Redo the initialization with `profile_color`.
  Initialize(profile_color);
  InitializeMessageHandler(/*browser=*/nullptr);
}

void SyncConfirmationUI::Initialize(
    base::Optional<SkColor> profile_creation_flow_color) {
  const bool is_sync_allowed =
      ProfileSyncServiceFactory::IsSyncAllowed(profile_);

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISyncConfirmationHost);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  static constexpr webui::ResourcePath kResources[] = {
      {"signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS},
      {"signin_vars_css.js", IDR_SIGNIN_VARS_CSS_JS},
      {"sync_confirmation_browser_proxy.js",
       IDR_SYNC_CONFIRMATION_BROWSER_PROXY_JS},
      {"sync_confirmation.js", IDR_SYNC_CONFIRMATION_JS},
  };
  source->AddResourcePaths(kResources);

  if (is_sync_allowed) {
    InitializeForSyncConfirmation(source, profile_creation_flow_color);
  } else {
    InitializeForSyncDisabled(source);
  }

  source->DisableTrustedTypesCSP();

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(
      g_browser_process->GetApplicationLocale(), &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(profile_, source);
}

void SyncConfirmationUI::InitializeMessageHandler(Browser* browser) {
  web_ui()->AddMessageHandler(std::make_unique<SyncConfirmationHandler>(
      profile_, js_localized_string_to_ids_map_, browser));
}

void SyncConfirmationUI::InitializeForSyncConfirmation(
    content::WebUIDataSource* source,
    base::Optional<SkColor> profile_creation_flow_color) {
  // Resources for testing.
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");

  AddStringResource(source, "syncConfirmationTitle",
                    IDS_SYNC_CONFIRMATION_TITLE);
  AddStringResource(source, "syncConfirmationSyncInfoTitle",
                    IDS_SYNC_CONFIRMATION_SYNC_INFO_TITLE);
  AddStringResource(source, "syncConfirmationSyncInfoDesc",
                    IDS_SYNC_CONFIRMATION_SYNC_INFO_DESC);
  AddStringResource(source, "syncConfirmationSettingsInfo",
                    IDS_SYNC_CONFIRMATION_SETTINGS_INFO);
  AddStringResource(source, "syncConfirmationSettingsLabel",
                    IDS_SYNC_CONFIRMATION_SETTINGS_BUTTON_LABEL);
  AddStringResource(source, "syncConfirmationConfirmLabel",
                    IDS_SYNC_CONFIRMATION_CONFIRM_BUTTON_LABEL);

  const int kAccountPictureSize = 128;
  std::string avatar_picture_url;
  if (profile_creation_flow_color.has_value()) {
    SkColor fill_color = *profile_creation_flow_color;
    gfx::Image avatar_picture = profiles::GetPlaceholderAvatarIconWithColors(
        /*fill_color=*/fill_color,
        /*stroke_color=*/GetAvatarStrokeColor(fill_color), kAccountPictureSize);
    avatar_picture_url = webui::GetBitmapDataUrl(avatar_picture.AsBitmap());
  } else {
    avatar_picture_url = profiles::GetPlaceholderAvatarIconUrl();
  }
  source->AddString("accountPictureUrl", avatar_picture_url);

  source->AddResourcePath("sync_confirmation_app.js",
                          IDR_SYNC_CONFIRMATION_APP_JS);
  source->SetDefaultResource(IDR_SYNC_CONFIRMATION_HTML);

  source->AddBoolean("isProfileCreationFlow",
                     profile_creation_flow_color.has_value());
  if (!profile_creation_flow_color.has_value()) {
    AddStringResource(source, "syncConfirmationUndoLabel", IDS_CANCEL);

    source->AddResourcePath(
        "images/sync_confirmation_illustration.svg",
        IDR_SYNC_CONFIRMATION_IMAGES_SYNC_CONFIRMATION_ILLUSTRATION_SVG);
    source->AddResourcePath(
        "images/sync_confirmation_illustration_dark.svg",
        IDR_SYNC_CONFIRMATION_IMAGES_SYNC_CONFIRMATION_ILLUSTRATION_DARK_SVG);
    return;
  }

  AddStringResource(source, "syncConfirmationUndoLabel", IDS_NO_THANKS);
  source->AddString("highlightColor", color_utils::SkColorToRgbaString(
                                          *profile_creation_flow_color));

  source->AddResourcePath(
      "images/sync_confirmation_refreshed_illustration.svg",
      IDR_SYNC_CONFIRMATION_IMAGES_SYNC_CONFIRMATION_REFRESHED_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "images/sync_confirmation_refreshed_illustration_dark.svg",
      IDR_SYNC_CONFIRMATION_IMAGES_SYNC_CONFIRMATION_REFRESHED_ILLUSTRATION_DARK_SVG);
}

void SyncConfirmationUI::InitializeForSyncDisabled(
    content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_SYNC_DISABLED_CONFIRMATION_HTML);
  source->AddResourcePath("sync_disabled_confirmation_app.js",
                          IDR_SYNC_DISABLED_CONFIRMATION_APP_JS);

  AddStringResource(source, "syncDisabledConfirmationTitle",
                    IDS_SYNC_DISABLED_CONFIRMATION_CHROME_SYNC_TITLE);
  AddStringResource(source, "syncDisabledConfirmationDetails",
                    IDS_SYNC_DISABLED_CONFIRMATION_DETAILS);
  AddStringResource(source, "syncDisabledConfirmationConfirmLabel",
                    IDS_SYNC_DISABLED_CONFIRMATION_CONFIRM_BUTTON_LABEL);
  AddStringResource(source, "syncDisabledConfirmationUndoLabel",
                    IDS_SYNC_DISABLED_CONFIRMATION_UNDO_BUTTON_LABEL);
}

void SyncConfirmationUI::AddStringResource(content::WebUIDataSource* source,
                                           const std::string& name,
                                           int ids) {
  source->AddLocalizedString(name, ids);

  // When the strings are passed to the HTML, the Unicode NBSP symbol (\u00A0)
  // will be automatically replaced with "&nbsp;". This change must be mirrored
  // in the string-to-ids map. Note that "\u00A0" is actually two characters,
  // so we must use base::ReplaceSubstrings* rather than base::ReplaceChars.
  // TODO(msramek): Find a more elegant solution.
  std::string sanitized_string =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(ids));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");

  js_localized_string_to_ids_map_[sanitized_string] = ids;
}
