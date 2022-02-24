// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webapks/webapks_handler.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "content/public/browser/web_ui.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/android/color_utils_android.h"
#include "ui/gfx/color_utils.h"

WebApksHandler::WebApksHandler()
    : delegate_(base::BindRepeating(&WebApksHandler::OnWebApkInfoRetrieved,
                                    base::Unretained(this))) {}

WebApksHandler::~WebApksHandler() {}

void WebApksHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "requestWebApksInfo",
      base::BindRepeating(&WebApksHandler::HandleRequestWebApksInfo,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "requestWebApkUpdate",
      base::BindRepeating(&WebApksHandler::HandleRequestWebApkUpdate,
                          base::Unretained(this)));
}

void WebApksHandler::HandleRequestWebApksInfo(const base::ListValue* args) {
  AllowJavascript();
  delegate_.RetrieveWebApks();
}

void WebApksHandler::HandleRequestWebApkUpdate(const base::ListValue* args) {
  AllowJavascript();
  for (const auto& val : args->GetListDeprecated()) {
    if (val.is_string())
      ShortcutHelper::SetForceWebApkUpdate(val.GetString());
  }
}

void WebApksHandler::OnWebApkInfoRetrieved(const WebApkInfo& webapk_info) {
  if (!IsJavascriptAllowed())
    return;
  base::DictionaryValue result;
  result.SetStringKey("name", webapk_info.name);
  result.SetStringKey("shortName", webapk_info.short_name);
  result.SetStringKey("packageName", webapk_info.package_name);
  result.SetStringKey("id", webapk_info.id);
  result.SetIntKey("shellApkVersion", webapk_info.shell_apk_version);
  result.SetIntKey("versionCode", webapk_info.version_code);
  result.SetStringKey("uri", webapk_info.uri);
  result.SetStringKey("scope", webapk_info.scope);
  result.SetStringKey("manifestUrl", webapk_info.manifest_url);
  result.SetStringKey("manifestStartUrl", webapk_info.manifest_start_url);
  result.SetStringKey("displayMode",
                      blink::DisplayModeToString(webapk_info.display));
  result.SetStringKey(
      "orientation",
      blink::WebScreenOrientationLockTypeToString(webapk_info.orientation));
  result.SetStringKey("themeColor",
                      ui::OptionalSkColorToString(webapk_info.theme_color));
  result.SetStringKey("backgroundColor", ui::OptionalSkColorToString(
                                             webapk_info.background_color));
  result.SetDoubleKey("lastUpdateCheckTimeMs",
                      webapk_info.last_update_check_time.ToJsTime());
  result.SetDoubleKey("lastUpdateCompletionTimeMs",
                      webapk_info.last_update_completion_time.ToJsTime());
  result.SetBoolKey("relaxUpdates", webapk_info.relax_updates);
  result.SetStringKey("backingBrowser",
                      webapk_info.backing_browser_package_name);
  result.SetBoolKey("isBackingBrowser", webapk_info.is_backing_browser);
  result.SetStringKey("updateStatus",
                      webapk_info.is_backing_browser
                          ? webapk_info.update_status
                          : "Current browser doesn't own this WebAPK.");
  FireWebUIListener("web-apk-info", result);
}
