// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/web_engine_main_delegate.h"

#include <utility>

#include "base/base_paths.h"
#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/intl_profile_watcher.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/engine/browser/web_engine_browser_main.h"
#include "fuchsia/engine/browser/web_engine_content_browser_client.h"
#include "fuchsia/engine/common/cors_exempt_headers.h"
#include "fuchsia/engine/common/web_engine_content_client.h"
#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"
#include "fuchsia/engine/switches.h"
#include "google_apis/google_api_keys.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace {

WebEngineMainDelegate* g_current_web_engine_main_delegate = nullptr;

void InitializeResources() {
  constexpr char kWebUiResourcesPakPath[] = "ui/resources/webui_resources.pak";
  constexpr char kWebUiGeneratedResourcesPakPath[] =
      "ui/resources/webui_generated_resources.pak";

  // TODO(1164990): Update //ui's DIR_LOCALES to use DIR_ASSETS, rather than
  // using Override() here.
  base::FilePath asset_root;
  bool result = base::PathService::Get(base::DIR_ASSETS, &asset_root);
  DCHECK(result);
  const base::FilePath locales_path = asset_root.AppendASCII("locales");
  result = base::PathService::Override(ui::DIR_LOCALES, locales_path);
  DCHECK(result);

  // Initialize common locale-agnostic resources.
  const std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::i18n::GetConfiguredLocale(), nullptr,
      ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  VLOG(1) << "Loaded resources including locale: " << locale;

  // Conditionally load WebUI resource PAK if visible from namespace.
  const base::FilePath webui_resources_path =
      asset_root.Append(kWebUiResourcesPakPath);
  if (base::PathExists(webui_resources_path)) {
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        webui_resources_path, ui::SCALE_FACTOR_NONE);
  }

  const base::FilePath webui_generated_resources_path =
      asset_root.Append(kWebUiGeneratedResourcesPakPath);
  if (base::PathExists(webui_generated_resources_path)) {
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        webui_generated_resources_path, ui::SCALE_FACTOR_NONE);
  }
}

}  // namespace

// static
WebEngineMainDelegate* WebEngineMainDelegate::GetInstanceForTest() {
  return g_current_web_engine_main_delegate;
}

WebEngineMainDelegate::WebEngineMainDelegate() {
  g_current_web_engine_main_delegate = this;
}

WebEngineMainDelegate::~WebEngineMainDelegate() = default;

bool WebEngineMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!cr_fuchsia::InitLoggingFromCommandLine(*command_line)) {
    *exit_code = 1;
    return true;
  }

  if (command_line->HasSwitch(switches::kGoogleApiKey)) {
    google_apis::SetAPIKey(
        command_line->GetSwitchValueASCII(switches::kGoogleApiKey));
  }

  SetCorsExemptHeaders(base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kCorsExemptHeaders),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return false;
}

void WebEngineMainDelegate::PreSandboxStartup() {
  // Early during startup, configure the process with the primary locale and
  // load resources. If locale-specific resources are loaded then they must be
  // explicitly reloaded after each change to the primary locale.
  // In the browser process the locale determines the accept-language header
  // contents, and is supplied to renderers for Blink to report to web content.
  std::string initial_locale =
      base::FuchsiaIntlProfileWatcher::GetPrimaryLocaleIdForInitialization();
  base::i18n::SetICUDefaultLocale(initial_locale);

  InitializeResources();
}

int WebEngineMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

  return WebEngineBrowserMain(main_function_params);
}

content::ContentClient* WebEngineMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<WebEngineContentClient>();
  return content_client_.get();
}

content::ContentBrowserClient*
WebEngineMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_ = std::make_unique<WebEngineContentBrowserClient>();
  return browser_client_.get();
}

content::ContentRendererClient*
WebEngineMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WebEngineContentRendererClient>();
  return renderer_client_.get();
}
